/*
 * diskio.c - Disk / image I/O abstraction
 *
 * Handles:
 *   \\.\PhysicalDriveN   whole physical disk (may need partition number)
 *   \\.\X:               volume/partition letter
 *   \\.\HarddiskVolumeN  volume by number
 *   path\to\file.img     raw image, flat or with MBR added GPT but idk if it works
 */

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include "diskio.h"
#include "ext4.h"

/*  Helpers */

static BOOL raw_read(HANDLE h, uint64_t byte_offset, void *buf, uint32_t size)
{
    OVERLAPPED ov = {0};
    ov.Offset     = (DWORD)(byte_offset & 0xFFFFFFFF);
    ov.OffsetHigh = (DWORD)(byte_offset >> 32);
    DWORD read = 0;
    if (!ReadFile(h, buf, size, &read, &ov))
        return FALSE;
    return read == size;
}

/*  Partition table parsing */

static BOOL parse_mbr(HANDLE h, int part_idx, uint64_t *offset, uint64_t *size)
{
    mbr_t mbr;
    if (!raw_read(h, 0, &mbr, sizeof(mbr)))
        return FALSE;
    if (mbr.signature != 0xAA55)
        return FALSE;
    if (part_idx < 1 || part_idx > 4)
        return FALSE;

    mbr_partition_entry_t *p = &mbr.partitions[part_idx - 1];
    if (p->lba_start == 0)
        return FALSE;

    *offset = (uint64_t)p->lba_start * DISKIO_SECTOR_SIZE;
    *size   = (uint64_t)p->lba_size  * DISKIO_SECTOR_SIZE;
    return TRUE;
}

static BOOL parse_gpt(HANDLE h, int part_idx, uint64_t *offset, uint64_t *size)
{
    gpt_header_t hdr;
    if (!raw_read(h, 512, &hdr, sizeof(hdr)))
        return FALSE;
    if (memcmp(hdr.signature, "EFI PART", 8) != 0)
        return FALSE;

    uint32_t entry_size = hdr.sizeof_partition_entry;
    if (entry_size < sizeof(gpt_partition_entry_t) || entry_size > 4096)
        return FALSE;

    uint8_t entry_buf[4096];
    int linux_count = 0;

    for (uint32_t i = 0; i < hdr.num_partition_entries; i++) {
        uint64_t entry_off = hdr.partition_entry_lba * 512ULL + (uint64_t)i * entry_size;
        if (!raw_read(h, entry_off, entry_buf, entry_size))
            continue;

        gpt_partition_entry_t *e = (gpt_partition_entry_t *)entry_buf;

        /* skip empty entries */
        BOOL all_zero = TRUE;
        for (int j = 0; j < 16; j++) if (e->type_guid[j]) { all_zero = FALSE; break; }
        if (all_zero) continue;

        linux_count++;
        if (linux_count == part_idx) {
            *offset = e->start_lba * 512ULL;
            *size   = (e->end_lba - e->start_lba + 1) * 512ULL;
            return TRUE;
        }
    }
    return FALSE;
}

/*  diskio_list_partitions */

void diskio_list_partitions(const char *path)
{
    HANDLE h = CreateFileA(path,
                           GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                           NULL, OPEN_EXISTING,
                           FILE_FLAG_NO_BUFFERING, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open %s: error %lu\n", path, GetLastError());
        return;
    }

    uint8_t sector[512];
    if (!raw_read(h, 0, sector, 512)) {
        CloseHandle(h);
        return;
    }

    /* Try GPT first (LBA 1) */
    gpt_header_t gpt;
    BOOL is_gpt = FALSE;
    if (raw_read(h, 512, &gpt, sizeof(gpt)) &&
        memcmp(gpt.signature, "EFI PART", 8) == 0)
    {
        is_gpt = TRUE;
        fprintf(stderr, "GPT disk detected. Non-empty partitions:\n");
        int idx = 0;
        for (uint32_t i = 0; i < gpt.num_partition_entries; i++) {
            uint64_t off = gpt.partition_entry_lba * 512ULL +
                           (uint64_t)i * gpt.sizeof_partition_entry;
            gpt_partition_entry_t e;
            if (!raw_read(h, off, &e, sizeof(e))) continue;
            BOOL empty = TRUE;
            for (int j = 0; j < 16; j++) if (e.type_guid[j]) { empty = FALSE; break; }
            if (empty) continue;
            idx++;
            char name[74] = {0};
            for (int j = 0; j < 36 && e.name[j]; j++) name[j] = (char)e.name[j];
            fprintf(stderr, "  Partition %d: start=%-10llu size=%-10llu MB  name=%s\n",
                    idx,
                    (unsigned long long)(e.start_lba * 512ULL),
                    (unsigned long long)((e.end_lba - e.start_lba + 1) * 512ULL / 1048576),
                    name);
        }
    }

    if (!is_gpt) {
        /* Try MBR */
        mbr_t *mbr = (mbr_t *)sector;
        if (mbr->signature == 0xAA55) {
            fprintf(stderr, "MBR disk detected. Primary partitions:\n");
            for (int i = 0; i < 4; i++) {
                mbr_partition_entry_t *p = &mbr->partitions[i];
                if (!p->lba_start) continue;
                fprintf(stderr,
                        "  Partition %d: type=0x%02X  start=%-10llu size=%-6llu MB\n",
                        i + 1, p->type,
                        (unsigned long long)((uint64_t)p->lba_start * 512),
                        (unsigned long long)((uint64_t)p->lba_size  * 512 / 1048576));
            }
        } else {
            fprintf(stderr, "No recognisable partition table. Try --partition 0 for raw ext4.\n");
        }
    }
    CloseHandle(h);
}

/*  diskio_open */

BOOL diskio_open(const char *path, int partition, disk_handle_t *out)
{
    memset(out, 0, sizeof(*out));
    strncpy_s(out->path, sizeof(out->path), path, _TRUNCATE);

    /* Detect if this is a device path */
    out->is_device = (strncmp(path, "\\\\.\\", 4) == 0);

    DWORD flags = FILE_FLAG_NO_BUFFERING;
    if (!out->is_device) flags |= FILE_ATTRIBUTE_NORMAL;

    out->hFile = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        flags,
        NULL
    );

    if (out->hFile == INVALID_HANDLE_VALUE) {
        /* retry without NO_BUFFERING for image files */
        if (!out->is_device) {
            out->hFile = CreateFileA(path, GENERIC_READ,
                FILE_SHARE_READ, NULL, OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL, NULL);
        }
        if (out->hFile == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "diskio_open: cannot open '%s': error %lu\n",
                    path, GetLastError());
            return FALSE;
        }
    }

    /* Determine partition offset ---------------------------------------- */
    if (partition == 0) {
        /* Treat whole device/file as raw ext4 */
        out->type             = DISK_SRC_IMAGE_RAW;
        out->partition_offset = 0;
        out->partition_size   = 0;
    } else {
        /* Try GPT first, then MBR */
        BOOL found = FALSE;

        uint64_t off = 0, sz = 0;

        /* GPT */
        found = parse_gpt(out->hFile, partition, &off, &sz);
        if (found) {
            out->type             = DISK_SRC_IMAGE_GPT;
            out->partition_offset = off;
            out->partition_size   = sz;
        } else {
            /* MBR */
            found = parse_mbr(out->hFile, partition, &off, &sz);
            if (found) {
                out->type             = DISK_SRC_IMAGE_MBR;
                out->partition_offset = off;
                out->partition_size   = sz;
            }
        }

        if (!found) {
            fprintf(stderr, "diskio_open: partition %d not found in '%s'.\n"
                            "  Use --list-partitions to see available partitions.\n",
                    partition, path);
            CloseHandle(out->hFile);
            out->hFile = INVALID_HANDLE_VALUE;
            return FALSE;
        }
    }

    return TRUE;
}

/*  diskio_read  */

BOOL diskio_read(disk_handle_t *dh, uint64_t offset, void *buf, uint32_t size)
{
    if (dh->hFile == INVALID_HANDLE_VALUE) return FALSE;

    uint64_t abs_offset = dh->partition_offset + offset;

    /* For NO_BUFFERING devices, reads must be sector-aligned.             */
    /* We handle unaligned reads by reading aligned super-set + copying.   */
    if (dh->is_device) {
        uint64_t aligned_start = abs_offset & ~(uint64_t)(DISKIO_SECTOR_SIZE - 1);
        uint32_t prefix        = (uint32_t)(abs_offset - aligned_start);
        uint32_t aligned_size  = (prefix + size + DISKIO_SECTOR_SIZE - 1)
                                 & ~(DISKIO_SECTOR_SIZE - 1);

        uint8_t *tmp = (uint8_t *)_aligned_malloc(aligned_size, DISKIO_SECTOR_SIZE);
        if (!tmp) return FALSE;

        BOOL ok = raw_read(dh->hFile, aligned_start, tmp, aligned_size);
        if (ok) memcpy(buf, tmp + prefix, size);
        _aligned_free(tmp);
        return ok;
    }

    return raw_read(dh->hFile, abs_offset, buf, size);
}

/*  diskio_probe_ext4  */

BOOL diskio_probe_ext4(disk_handle_t *dh)
{
    uint16_t magic = 0;
    /* superblock magic is at byte 56 of the superblock (offset 1024+56 = 1080) */
    if (!diskio_read(dh, 1024 + 56, &magic, sizeof(magic)))
        return FALSE;
    return magic == EXT4_SUPER_MAGIC;
}

/*  diskio_close  */

void diskio_close(disk_handle_t *dh)
{
    if (dh->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(dh->hFile);
        dh->hFile = INVALID_HANDLE_VALUE;
    }
}
