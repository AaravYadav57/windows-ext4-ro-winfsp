#pragma once

#include <windows.h>
#include <stdint.h>

#define DISKIO_SECTOR_SIZE  512

typedef enum {
    DISK_SRC_PARTITION,   
    DISK_SRC_IMAGE_RAW,   
    DISK_SRC_IMAGE_MBR,   
    DISK_SRC_IMAGE_GPT,  
} disk_source_type_t;

typedef struct {
    HANDLE            hFile;            
    disk_source_type_t type;
    uint64_t          partition_offset; 
    uint64_t          partition_size;   
    uint32_t          block_size;       
    BOOL              is_device;        
    char              path[MAX_PATH];
} disk_handle_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t lba_size;
} mbr_partition_entry_t;

typedef struct {
    uint8_t               bootstrap[446];
    mbr_partition_entry_t partitions[4];
    uint16_t              signature;   
} mbr_t;

typedef struct {
    uint8_t  signature[8];   
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t sizeof_partition_entry;
    uint32_t partition_entry_array_crc32;
} gpt_header_t;

typedef struct {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t start_lba;
    uint64_t end_lba;
    uint64_t attributes;
    uint16_t name[36];   
} gpt_partition_entry_t;
#pragma pack(pop)

static const uint8_t LINUX_DATA_GUID[16] = {
    0xAF, 0x3D, 0xC6, 0x0F,
    0x83, 0x84, 0x72, 0x47,
    0x8E, 0x79, 0x3D, 0x69,
    0xD8, 0x47, 0x7D, 0xE4
};
BOOL diskio_open(const char *path, int partition, disk_handle_t *out);

BOOL diskio_read(disk_handle_t *dh, uint64_t offset, void *buf, uint32_t size);

void diskio_close(disk_handle_t *dh);

BOOL diskio_probe_ext4(disk_handle_t *dh);
void diskio_list_partitions(const char *path);
