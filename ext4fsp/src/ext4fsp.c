/*
 * ext4fsp.c - WinFsp-based read-only ext4 filesystem driver
 *
 * Implements the WinFsp FileSystemHost interface using the ext4fs layer.
 *
 * Build deps:
 *   WinFsp SDK headers  (winfsp/winfsp.h)
 *   winfsp-x64.lib      (or winfsp-x86.lib)
 *
 * Run-time deps:
 *   WinFsp installed on the target machine (driver service must be running).
 */

/*
 * These must come before winfsp.h on newer WinFsp / VS2026.
 * winfsp.h uses NTSTATUS / PNTSTATUS which live in ntdef.h.
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winternl.h>   /* NTSTATUS, UNICODE_STRING, etc.  */
#include <ntstatus.h>   /* STATUS_* constants               */
#include <winfsp/winfsp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <malloc.h>     /* _aligned_malloc / _aligned_free  */
#include "ext4fs.h"
#include "diskio.h"

/* ── Driver context ─────────────────────────────────────────────────────── */
typedef struct {
    FSP_FILE_SYSTEM  *FileSystem;
    ext4fs_t          ext4;
    char              volume_label[64];
} EXT4_CONTEXT;

/* ── Time conversion: Unix → FILETIME (100-ns intervals since 1601) ──────── */
static UINT64 unix_to_filetime(uint32_t unix_sec, uint32_t nsec)
{
    /* 11644473600 seconds between 1601-01-01 and 1970-01-01 */
    UINT64 ft = (UINT64)(unix_sec + 11644473600ULL) * 10000000ULL
              + nsec / 100;
    return ft;
}

/* ── Inode → FileInfo ───────────────────────────────────────────────────── */
static void fill_fileinfo(ext4fs_t *fs, uint32_t ino, FSP_FSCTL_FILE_INFO *info)
{
    ext4_stat_t st;
    if (!ext4fs_stat(fs, ino, &st)) {
        memset(info, 0, sizeof(*info));
        return;
    }

    uint16_t mode = st.mode & EXT4_S_IFMT;
    UINT32 attrs  = FILE_ATTRIBUTE_READONLY;

    if (mode == EXT4_S_IFDIR)
        attrs |= FILE_ATTRIBUTE_DIRECTORY;
    else if (mode == EXT4_S_IFLNK)
        attrs |= FILE_ATTRIBUTE_REPARSE_POINT;

    info->FileAttributes    = attrs;
    info->ReparseTag        = 0;
    info->AllocationSize    = (st.size + 511) & ~(UINT64)511;
    info->FileSize          = st.size;
    info->CreationTime      = unix_to_filetime(st.crtime,  st.crtime_nsec);
    info->LastAccessTime    = unix_to_filetime(st.atime,   st.atime_nsec);
    info->LastWriteTime     = unix_to_filetime(st.mtime,   st.mtime_nsec);
    info->ChangeTime        = unix_to_filetime(st.ctime,   st.ctime_nsec);
    /* Use inode number as file index */
    info->IndexNumber       = ino;
    info->HardLinks         = st.nlink;
    info->EaSize            = 0;
}

/* ── File node: we store the inode number ───────────────────────────────── */
/* WinFsp passes FileNode/FileDesc as PVOID; we use inode number directly   */
#define INO_TO_PVOID(ino) ((PVOID)(ULONG_PTR)(ino))
#define PVOID_TO_INO(p)   ((uint32_t)(ULONG_PTR)(p))

/* ── Convert Windows path to Unix (backslash → slash) ──────────────────── */
static void wpath_to_unix(const WCHAR *wpath, char *out, int out_size)
{
    char utf8[4096];
    WideCharToMultiByte(CP_UTF8, 0, wpath, -1, utf8, sizeof(utf8), NULL, NULL);
    /* Replace backslashes with forward slashes */
    for (int i = 0; utf8[i] && i < out_size - 1; i++) {
        out[i] = (utf8[i] == '\\') ? '/' : utf8[i];
    }
    out[out_size - 1] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  WinFsp FileSystem Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

static NTSTATUS GetVolumeInfo(FSP_FILE_SYSTEM *FileSystem,
                              FSP_FSCTL_VOLUME_INFO *VolumeInfo)
{
    EXT4_CONTEXT *ctx = (EXT4_CONTEXT *)FileSystem->UserContext;

    VolumeInfo->TotalSize     = (UINT64)ctx->ext4.total_blocks
                              * ctx->ext4.block_size;
    VolumeInfo->FreeSize      = 0;  /* read-only; advertise 0 free */
    VolumeInfo->VolumeLabelLength =
        (UINT16)(wcslen((WCHAR *)VolumeInfo->VolumeLabel) * sizeof(WCHAR));

    WCHAR label[32] = {0};
    MultiByteToWideChar(CP_UTF8, 0,
                        ctx->volume_label, -1,
                        label, 32);
    UINT16 label_len = (UINT16)(wcslen(label) * sizeof(WCHAR));
    if (label_len > sizeof(VolumeInfo->VolumeLabel))
        label_len = sizeof(VolumeInfo->VolumeLabel);
    memcpy(VolumeInfo->VolumeLabel, label, label_len);
    VolumeInfo->VolumeLabelLength = label_len;

    return STATUS_SUCCESS;
}

static NTSTATUS GetSecurityByName(FSP_FILE_SYSTEM *FileSystem,
                                  PWSTR FileName,
                                  PUINT32 PFileAttributes,
                                  PSECURITY_DESCRIPTOR SecurityDescriptor,
                                  SIZE_T *PSecurityDescriptorSize)
{
    EXT4_CONTEXT *ctx = (EXT4_CONTEXT *)FileSystem->UserContext;
    char path[4096];
    wpath_to_unix(FileName, path, sizeof(path));

    uint32_t ino = ext4fs_lookup(&ctx->ext4, path);
    if (!ino) return STATUS_OBJECT_NAME_NOT_FOUND;

    ext4_stat_t st;
    if (!ext4fs_stat(&ctx->ext4, ino, &st))
        return STATUS_OBJECT_NAME_NOT_FOUND;

    uint16_t mode = st.mode & EXT4_S_IFMT;
    if (PFileAttributes) {
        UINT32 attrs = FILE_ATTRIBUTE_READONLY;
        if (mode == EXT4_S_IFDIR)  attrs |= FILE_ATTRIBUTE_DIRECTORY;
        if (mode == EXT4_S_IFLNK)  attrs |= FILE_ATTRIBUTE_REPARSE_POINT;
        *PFileAttributes = attrs;
    }

    /* Return a minimal "everyone read" security descriptor */
    if (PSecurityDescriptorSize) {
        static const SECURITY_DESCRIPTOR empty_sd = {
            SECURITY_DESCRIPTOR_REVISION, 0,
            SE_DACL_PRESENT | SE_DACL_DEFAULTED, NULL, NULL, NULL, NULL
        };
        if (SecurityDescriptor && *PSecurityDescriptorSize >= sizeof(empty_sd)) {
            memcpy(SecurityDescriptor, &empty_sd, sizeof(empty_sd));
        }
        *PSecurityDescriptorSize = sizeof(empty_sd);
    }
    return STATUS_SUCCESS;
}

static NTSTATUS Open(FSP_FILE_SYSTEM *FileSystem,
                     PWSTR FileName, UINT32 CreateOptions,
                     UINT32 GrantedAccess,
                     PVOID *PFileNode, PVOID *PFileDesc,
                     FSP_FSCTL_FILE_INFO *FileInfo,
                     PWSTR *PNormalizedName,
                     SIZE_T *PNormalizedNameSize)
{
    EXT4_CONTEXT *ctx = (EXT4_CONTEXT *)FileSystem->UserContext;
    char path[4096];
    wpath_to_unix(FileName, path, sizeof(path));

    uint32_t ino = ext4fs_lookup(&ctx->ext4, path);
    if (!ino) return STATUS_OBJECT_NAME_NOT_FOUND;

    fill_fileinfo(&ctx->ext4, ino, FileInfo);
    *PFileNode = INO_TO_PVOID(ino);
    *PFileDesc = NULL;
    return STATUS_SUCCESS;
}

static NTSTATUS Close(FSP_FILE_SYSTEM *FileSystem,
                      PVOID FileNode, PVOID FileDesc)
{
    /* Nothing to free; inode number is in the pointer itself */
    (void)FileSystem; (void)FileNode; (void)FileDesc;
    return STATUS_SUCCESS;
}

static NTSTATUS GetFileInfo(FSP_FILE_SYSTEM *FileSystem,
                            PVOID FileNode, PVOID FileDesc,
                            FSP_FSCTL_FILE_INFO *FileInfo)
{
    EXT4_CONTEXT *ctx = (EXT4_CONTEXT *)FileSystem->UserContext;
    uint32_t ino = PVOID_TO_INO(FileNode);
    fill_fileinfo(&ctx->ext4, ino, FileInfo);
    return STATUS_SUCCESS;
}

static NTSTATUS Read(FSP_FILE_SYSTEM *FileSystem,
                     PVOID FileNode, PVOID FileDesc,
                     PVOID Buffer, UINT64 Offset, ULONG Length,
                     PULONG PBytesTransferred)
{
    EXT4_CONTEXT *ctx = (EXT4_CONTEXT *)FileSystem->UserContext;
    uint32_t ino = PVOID_TO_INO(FileNode);
    uint64_t read = 0;

    if (!ext4fs_read_file(&ctx->ext4, ino, Offset, Length, Buffer, &read))
        return STATUS_UNSUCCESSFUL;

    *PBytesTransferred = (ULONG)read;
    return STATUS_SUCCESS;
}

/* ── Directory enumeration ──────────────────────────────────────────────── */

/* We allocate an iterator per ReadDirectory call.                          */
/* FileDesc per-handle state holds the iterator pointer.                    */

typedef struct {
    ext4_dir_iter_t iter;
    BOOLEAN         need_dot;
    BOOLEAN         need_dotdot;
    uint32_t        parent_ino;
    /* buffered entry when pattern filtering */
    BOOLEAN         has_buffered;
    ext4_dirent_t   buffered;
} DIR_HANDLE;

static NTSTATUS OpenDir(FSP_FILE_SYSTEM *FileSystem,
                        PVOID FileNode, PVOID FileDesc,
                        PWSTR Pattern, PWSTR Marker,
                        PVOID *PDirBuffer)
{
    EXT4_CONTEXT *ctx = (EXT4_CONTEXT *)FileSystem->UserContext;
    uint32_t ino = PVOID_TO_INO(FileNode);

    DIR_HANDLE *dh = (DIR_HANDLE *)calloc(1, sizeof(DIR_HANDLE));
    if (!dh) return STATUS_NO_MEMORY;

    if (!ext4_dir_iter_open(&ctx->ext4, ino, &dh->iter)) {
        free(dh);
        return STATUS_UNSUCCESSFUL;
    }
    dh->need_dot    = TRUE;
    dh->need_dotdot = TRUE;
    dh->parent_ino  = ino; /* approximate; '.' and '..' share same ino for root */

    *PDirBuffer = dh;
    return STATUS_SUCCESS;
}

static NTSTATUS ReadDirectory(FSP_FILE_SYSTEM *FileSystem,
                              PVOID FileNode, PVOID FileDesc,
                              PWSTR Pattern, PWSTR Marker,
                              PVOID Buffer, ULONG BufferLength,
                              PULONG PBytesTransferred)
{
    EXT4_CONTEXT *ctx  = (EXT4_CONTEXT *)FileSystem->UserContext;
    uint32_t      ino  = PVOID_TO_INO(FileNode);
    DIR_HANDLE   *dh;

    /* First call: open the iterator */
    /* WinFsp calls ReadDirectory with FileDesc as our DIR_HANDLE * once   */
    /* OpenDir was called. For simplicity we do both here.                 */

    dh = (DIR_HANDLE *)calloc(1, sizeof(DIR_HANDLE));
    if (!dh) return STATUS_NO_MEMORY;
    if (!ext4_dir_iter_open(&ctx->ext4, ino, &dh->iter)) {
        free(dh);
        return STATUS_UNSUCCESSFUL;
    }

    FSP_FILE_SYSTEM_INTERFACE *iface = FileSystem->Interface;
    NTSTATUS status = STATUS_SUCCESS;

    /* Emit "." */
    {
        FSP_FSCTL_FILE_INFO fi;
        fill_fileinfo(&ctx->ext4, ino, &fi);
        fi.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        WCHAR dot[] = L".";
        if (!FspFileSystemAddDirInfo(NULL, dot, &fi, Buffer, BufferLength, PBytesTransferred))
            goto done;
    }

    /* Emit ".." */
    {
        FSP_FSCTL_FILE_INFO fi;
        /* For simplicity use same inode info */
        fill_fileinfo(&ctx->ext4, ino, &fi);
        fi.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
        WCHAR dotdot[] = L"..";
        if (!FspFileSystemAddDirInfo(NULL, dotdot, &fi, Buffer, BufferLength, PBytesTransferred))
            goto done;
    }

    /* Emit directory entries */
    ext4_dirent_t de;
    while (ext4_dir_iter_next(&dh->iter, &de)) {
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
            continue;

        FSP_FSCTL_FILE_INFO fi;
        fill_fileinfo(&ctx->ext4, de.inode, &fi);

        WCHAR wname[EXT4_NAME_LEN + 1];
        MultiByteToWideChar(CP_UTF8, 0, de.name, -1, wname, EXT4_NAME_LEN + 1);

        if (!FspFileSystemAddDirInfo(NULL, wname, &fi,
                                     Buffer, BufferLength, PBytesTransferred))
            break;
    }

done:
    ext4_dir_iter_close(&dh->iter);
    free(dh);
    return STATUS_SUCCESS;
}

/* ── Write stubs (read-only) ────────────────────────────────────────────── */
static NTSTATUS Create(FSP_FILE_SYSTEM *FS, PWSTR FN, UINT32 CO, UINT32 GA,
    UINT32 FA, PSECURITY_DESCRIPTOR SD, UINT64 AS,
    PVOID *FNode, PVOID *FDesc, FSP_FSCTL_FILE_INFO *FI, PWSTR *NN, SIZE_T *NS)
{ return STATUS_MEDIA_WRITE_PROTECTED; }

static NTSTATUS SetBasicInfo(FSP_FILE_SYSTEM *FS, PVOID FN, PVOID FD,
    UINT32 FA, UINT64 CT, UINT64 LAT, UINT64 LWT, UINT64 CHT,
    FSP_FSCTL_FILE_INFO *FI)
{ return STATUS_MEDIA_WRITE_PROTECTED; }

static NTSTATUS SetFileSize(FSP_FILE_SYSTEM *FS, PVOID FN, PVOID FD,
    UINT64 NS, BOOLEAN CA, FSP_FSCTL_FILE_INFO *FI)
{ return STATUS_MEDIA_WRITE_PROTECTED; }

static NTSTATUS CanDelete(FSP_FILE_SYSTEM *FS, PVOID FN, PVOID FD, PWSTR FNm)
{ return STATUS_MEDIA_WRITE_PROTECTED; }

static NTSTATUS Rename(FSP_FILE_SYSTEM *FS, PVOID FN, PVOID FD,
    PWSTR FNm, PWSTR NFNm, BOOLEAN RIFE)
{ return STATUS_MEDIA_WRITE_PROTECTED; }

static NTSTATUS SetSecurity(FSP_FILE_SYSTEM *FS, PVOID FN, PVOID FD,
    SECURITY_INFORMATION SI, PSECURITY_DESCRIPTOR SD)
{ return STATUS_MEDIA_WRITE_PROTECTED; }

static NTSTATUS Write(FSP_FILE_SYSTEM *FS, PVOID FN, PVOID FD,
    PVOID Buf, UINT64 Off, ULONG Len, BOOLEAN WCE, BOOLEAN CEF,
    PULONG PBT)
{ return STATUS_MEDIA_WRITE_PROTECTED; }

static NTSTATUS Flush(FSP_FILE_SYSTEM *FS, PVOID FN, PVOID FD,
    FSP_FSCTL_FILE_INFO *FI)
{ return STATUS_SUCCESS; }

/* ── Interface table ────────────────────────────────────────────────────── */
static FSP_FILE_SYSTEM_INTERFACE g_Ext4Interface = {
    .GetVolumeInfo      = GetVolumeInfo,
    .GetSecurityByName  = GetSecurityByName,
    .Create             = Create,
    .Open               = Open,
    .Overwrite          = NULL,
    .Cleanup            = NULL,
    .Close              = (VOID(*)(FSP_FILE_SYSTEM *, PVOID, PVOID))Close,
    .Read               = Read,
    .Write              = Write,
    .Flush              = Flush,
    .GetFileInfo        = GetFileInfo,
    .SetBasicInfo       = SetBasicInfo,
    .SetFileSize        = SetFileSize,
    .CanDelete          = CanDelete,
    .Rename             = Rename,
    .GetSecurity        = NULL,
    .SetSecurity        = SetSecurity,
    .ReadDirectory      = ReadDirectory,
    .ResolveReparsePoints = NULL,
    .GetReparsePoint    = NULL,
    .SetReparsePoint    = NULL,
    .DeleteReparsePoint = NULL,
    .GetStreamInfo      = NULL,
    .GetDirInfoByName   = NULL,
    .Control            = NULL,
    .SetDelete          = NULL,
    .CreateEx           = NULL,
    .OverwriteEx        = NULL,
    .GetEa              = NULL,
    .SetEa              = NULL,
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  Entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

static void usage(const char *prog)
{
    fprintf(stderr,
        "ext4fsp - Read-only ext4 filesystem driver for Windows (WinFsp)\n"
        "\n"
        "Usage:\n"
        "  %s [options] <source> <mount-point>\n"
        "\n"
        "Source (one of):\n"
        "  path\\to\\image.img         Raw ext4 image file (no partition table)\n"
        "  path\\to\\disk.img -p N     Image file, mount partition N\n"
        "  \\\\.\\PhysicalDriveN        Physical disk (mount partition N with -p)\n"
        "  \\\\.\\X:                    Windows volume/partition letter\n"
        "\n"
        "Mount point:\n"
        "  *                          Let WinFsp assign a drive letter\n"
        "  Z:                         Use drive letter Z\n"
        "  C:\\mnt\\myext4             Directory mount point\n"
        "\n"
        "Options:\n"
        "  -p N   Partition number to mount (1-based; 0 = treat as raw ext4)\n"
        "         GPT: N is the index of non-empty partitions (1=first)\n"
        "         MBR: N is the primary partition number (1-4)\n"
        "  --list-partitions  List partitions in source and exit\n"
        "  -d     Debug mode (verbose WinFsp logging)\n"
        "  -h     Show this help\n"
        "\n"
        "Examples:\n"
        "  %s ubuntu.img *\n"
        "  %s ubuntu.img Z:\n"
        "  %s disk.img -p 1 *\n"
        "  %s \\\\.\\PhysicalDrive1 -p 2 *\n"
        "  %s ubuntu.img --list-partitions\n"
        "\n"
        "Requires WinFsp to be installed: https://winfsp.dev\n",
        prog, prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[])
{
    const char *source     = NULL;
    const char *mountpoint = NULL;
    int         partition  = 0;
    BOOL        debug      = FALSE;
    BOOL        list_parts = FALSE;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]); return 0;
        } else if (strcmp(argv[i], "-d") == 0) {
            debug = TRUE;
        } else if (strcmp(argv[i], "--list-partitions") == 0) {
            list_parts = TRUE;
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            partition = atoi(argv[++i]);
        } else if (!source) {
            source = argv[i];
        } else if (!mountpoint) {
            mountpoint = argv[i];
        }
    }

    if (!source) { usage(argv[0]); return 1; }

    if (list_parts) {
        diskio_list_partitions(source);
        return 0;
    }

    if (!mountpoint) {
        fprintf(stderr, "Error: mount point required.\n\n");
        usage(argv[0]);
        return 1;
    }

    /* ── Open disk ────────────────────────────────────────────────────── */
    disk_handle_t disk;
    if (!diskio_open(source, partition, &disk)) {
        fprintf(stderr, "Failed to open source '%s'\n", source);
        return 1;
    }

    if (!diskio_probe_ext4(&disk)) {
        fprintf(stderr,
                "Warning: ext4 magic not found at expected offset.\n"
                "  The image may not contain an ext4 filesystem at partition %d.\n"
                "  Use --list-partitions to check available partitions.\n"
                "  Continuing anyway...\n", partition);
    }

    /* ── Open ext4 ────────────────────────────────────────────────────── */
    EXT4_CONTEXT ctx;
    memset(&ctx, 0, sizeof(ctx));

    if (!ext4fs_open(&disk, &ctx.ext4)) {
        fprintf(stderr, "Failed to open ext4 filesystem\n");
        diskio_close(&disk);
        return 1;
    }

    ext4fs_volume_label(&ctx.ext4, ctx.volume_label, sizeof(ctx.volume_label));
    if (ctx.volume_label[0] == '\0')
        strcpy_s(ctx.volume_label, sizeof(ctx.volume_label), "ext4");

    fprintf(stderr, "Volume label: %s\n", ctx.volume_label);
    fprintf(stderr, "Block size:   %u bytes\n", ctx.ext4.block_size);
    fprintf(stderr, "Total blocks: %llu\n",
            (unsigned long long)ctx.ext4.total_blocks);
    fprintf(stderr, "Groups:       %u\n", ctx.ext4.total_groups);

    /* ── Create WinFsp filesystem ─────────────────────────────────────── */
    FSP_FSCTL_VOLUME_PARAMS params = {0};
    params.Version              = sizeof(params);
    params.SectorSize           = (UINT16)ctx.ext4.block_size;
    params.SectorsPerAllocationUnit = 1;
    params.MaxComponentLength   = EXT4_NAME_LEN;
    params.VolumeCreationTime   = 0;
    params.VolumeSerialNumber   = 0x45585434; /* "EXT4" */
    params.FileInfoTimeout      = 1000;
    params.CaseSensitiveSearch  = 1;
    params.CasePreservedNames   = 1;
    params.UnicodeOnDisk        = 1;
    params.PersistentAcls       = 0;
    params.ReparsePoints        = 0;
    params.ReadOnlyVolume       = 1;
    params.PostCleanupWhenModifiedOnly = 1;

    WCHAR prefix[MAX_PATH] = {0};
    params.Prefix[0] = L'\0';

    /* Volume label */
    MultiByteToWideChar(CP_UTF8, 0, ctx.volume_label, -1,
                        params.VolumeLabel,
                        sizeof(params.VolumeLabel) / sizeof(WCHAR));

    WCHAR wmount[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, mountpoint, -1, wmount, MAX_PATH);

    NTSTATUS st = FspFileSystemCreate(
        L"" FSP_FSCTL_DISK_DEVICE_NAME,
        &params,
        &g_Ext4Interface,
        &ctx.FileSystem
    );

    if (!NT_SUCCESS(st)) {
        fprintf(stderr, "FspFileSystemCreate failed: 0x%08lX\n"
                        "  Ensure WinFsp is installed and the service is running.\n", st);
        ext4fs_close(&ctx.ext4);
        return 1;
    }

    ctx.FileSystem->UserContext = &ctx;

    if (debug) FspFileSystemSetDebugLog(ctx.FileSystem, ~0U);

    st = FspFileSystemStartDispatcher(ctx.FileSystem, 0);
    if (!NT_SUCCESS(st)) {
        fprintf(stderr, "FspFileSystemStartDispatcher failed: 0x%08lX\n", st);
        FspFileSystemDelete(ctx.FileSystem);
        ext4fs_close(&ctx.ext4);
        return 1;
    }

    /* Mount */
    st = FspFileSystemSetMountPoint(ctx.FileSystem, wmount);
    if (!NT_SUCCESS(st)) {
        fprintf(stderr, "FspFileSystemSetMountPoint failed: 0x%08lX\n"
                        "  Try a different mount point (e.g. * or Z:)\n", st);
        FspFileSystemStopDispatcher(ctx.FileSystem);
        FspFileSystemDelete(ctx.FileSystem);
        ext4fs_close(&ctx.ext4);
        return 1;
    }

    /* Get actual mount point */
    PWSTR actual = FspFileSystemMountPoint(ctx.FileSystem);
    fprintf(stderr, "\next4fsp: mounted '%s' (partition %d) at %ls\n",
            source, partition, actual ? actual : L"(unknown)");
    fprintf(stderr, "Press Ctrl+C to unmount and exit.\n\n");

    /* Wait for signal */
    FspServiceRunEx(NULL, NULL, NULL, NULL);

    /* Cleanup */
    FspFileSystemSetMountPoint(ctx.FileSystem, NULL);
    FspFileSystemStopDispatcher(ctx.FileSystem);
    FspFileSystemDelete(ctx.FileSystem);
    ext4fs_close(&ctx.ext4);

    fprintf(stderr, "\next4fsp: unmounted. Goodbye.\n");
    return 0;
}
