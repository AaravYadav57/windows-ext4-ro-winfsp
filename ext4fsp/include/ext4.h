#pragma once

#include <stdint.h>
#define EXT4_SUPER_MAGIC        0xEF53
#define EXT4_SUPERBLOCK_OFFSET  1024
#define EXT4_SUPERBLOCK_SIZE    1024
#define EXT4_FEATURE_INCOMPAT_FILETYPE      0x0002
#define EXT4_FEATURE_INCOMPAT_EXTENTS       0x0040
#define EXT4_FEATURE_INCOMPAT_64BIT         0x0080
#define EXT4_FEATURE_INCOMPAT_FLEX_BG       0x0200
#define EXT4_FEATURE_INCOMPAT_CSUM_SEED     0x2000
#define EXT4_FEATURE_INCOMPAT_LARGEDIR      0x4000
#define EXT4_FEATURE_INCOMPAT_INLINE_DATA   0x8000
#define EXT4_FEATURE_INCOMPAT_ENCRYPT       0x10000
#define EXT4_FEATURE_RO_COMPAT_SPARSE_SUPER     0x0001
#define EXT4_FEATURE_RO_COMPAT_LARGE_FILE       0x0002
#define EXT4_FEATURE_RO_COMPAT_HUGE_FILE        0x0008
#define EXT4_FEATURE_RO_COMPAT_DIR_NLINK        0x0020
#define EXT4_FEATURE_RO_COMPAT_EXTRA_ISIZE      0x0040
#define EXT4_FEATURE_RO_COMPAT_METADATA_CSUM    0x0400
#define EXT4_FEATURE_COMPAT_HAS_JOURNAL     0x0004
#define EXT4_S_IFMT     0xF000
#define EXT4_S_IFSOCK   0xC000
#define EXT4_S_IFLNK    0xA000
#define EXT4_S_IFREG    0x8000
#define EXT4_S_IFBLK    0x6000
#define EXT4_S_IFDIR    0x4000
#define EXT4_S_IFCHR    0x2000
#define EXT4_S_IFIFO    0x1000
#define EXT4_EXTENTS_FL     0x00080000
#define EXT4_INLINE_DATA_FL 0x10000000
#define EXT4_FT_UNKNOWN     0
#define EXT4_FT_REG_FILE    1
#define EXT4_FT_DIR         2
#define EXT4_FT_CHRDEV      3
#define EXT4_FT_BLKDEV      4
#define EXT4_FT_FIFO        5
#define EXT4_FT_SOCK        6
#define EXT4_FT_SYMLINK     7
#define EXT4_ROOT_INO       2
#define EXT4_NAME_LEN       255
#define EXT4_NDIR_BLOCKS    12
#define EXT4_IND_BLOCK      EXT4_NDIR_BLOCKS
#define EXT4_DIND_BLOCK     (EXT4_IND_BLOCK + 1)
#define EXT4_TIND_BLOCK     (EXT4_DIND_BLOCK + 1)
#define EXT4_N_BLOCKS       (EXT4_TIND_BLOCK + 1)

#pragma pack(push, 1)

typedef struct {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count_lo;
    uint32_t s_r_blocks_count_lo;
    uint32_t s_free_blocks_count_lo;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;      
    uint32_t s_log_cluster_size;
    uint32_t s_blocks_per_group;
    uint32_t s_clusters_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_reserved_gdt_blocks;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_jnl_backup_type;
    uint16_t s_desc_size;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_mkfs_time;
    uint32_t s_jnl_blocks[17];
    uint32_t s_blocks_count_hi;
    uint32_t s_r_blocks_count_hi;
    uint32_t s_free_blocks_count_hi;
    uint16_t s_min_extra_isize;
    uint16_t s_want_extra_isize;
    uint32_t s_flags;
    uint16_t s_raid_stride;
    uint16_t s_mmp_update_interval;
    uint64_t s_mmp_block;
    uint32_t s_raid_stripe_width;
    uint8_t  s_log_groups_per_flex;
    uint8_t  s_checksum_type;
    uint16_t s_reserved_pad;
    uint64_t s_kbytes_written;
    uint32_t s_snapshot_inum;
    uint32_t s_snapshot_id;
    uint64_t s_snapshot_r_blocks_count;
    uint32_t s_snapshot_list;
    uint32_t s_error_count;
    uint32_t s_first_error_time;
    uint32_t s_first_error_ino;
    uint64_t s_first_error_block;
    uint8_t  s_first_error_func[32];
    uint32_t s_first_error_line;
    uint32_t s_last_error_time;
    uint32_t s_last_error_ino;
    uint32_t s_last_error_line;
    uint64_t s_last_error_block;
    uint8_t  s_last_error_func[32];
    uint8_t  s_mount_opts[64];
    uint32_t s_usr_quota_inum;
    uint32_t s_grp_quota_inum;
    uint32_t s_overhead_clusters;
    uint32_t s_backup_bgs[2];
    uint8_t  s_encrypt_algos[4];
    uint8_t  s_encrypt_pw_salt[16];
    uint32_t s_lpf_ino;
    uint32_t s_prj_quota_inum;
    uint32_t s_checksum_seed;
    uint8_t  s_wtime_hi;
    uint8_t  s_mtime_hi;
    uint8_t  s_mkfs_time_hi;
    uint8_t  s_lastcheck_hi;
    uint8_t  s_first_error_time_hi;
    uint8_t  s_last_error_time_hi;
    uint8_t  s_pad[2];
    uint16_t s_encoding;
    uint16_t s_encoding_flags;
    uint32_t s_reserved[95];
    uint32_t s_checksum;
} ext4_super_block_t;

typedef struct {
    uint32_t bg_block_bitmap_lo;
    uint32_t bg_inode_bitmap_lo;
    uint32_t bg_inode_table_lo;
    uint16_t bg_free_blocks_count_lo;
    uint16_t bg_free_inodes_count_lo;
    uint16_t bg_used_dirs_count_lo;
    uint16_t bg_flags;
    uint32_t bg_exclude_bitmap_lo;
    uint16_t bg_block_bitmap_csum_lo;
    uint16_t bg_inode_bitmap_csum_lo;
    uint16_t bg_itable_unused_lo;
    uint16_t bg_checksum;
    /* 64-bit fields */
    uint32_t bg_block_bitmap_hi;
    uint32_t bg_inode_bitmap_hi;
    uint32_t bg_inode_table_hi;
    uint16_t bg_free_blocks_count_hi;
    uint16_t bg_free_inodes_count_hi;
    uint16_t bg_used_dirs_count_hi;
    uint16_t bg_itable_unused_hi;
    uint32_t bg_exclude_bitmap_hi;
    uint16_t bg_block_bitmap_csum_hi;
    uint16_t bg_inode_bitmap_csum_hi;
    uint32_t bg_reserved;
} ext4_group_desc_t;

typedef struct {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size_lo;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks_lo;
    uint32_t i_flags;
    union {
        struct { uint32_t l_i_version; } linux1;
    } osd1;
    uint32_t i_block[EXT4_N_BLOCKS];   
    uint32_t i_generation;
    uint32_t i_file_acl_lo;
    uint32_t i_size_high;
    uint32_t i_obso_faddr;
    union {
        struct {
            uint16_t l_i_blocks_high;
            uint16_t l_i_file_acl_high;
            uint16_t l_i_uid_high;
            uint16_t l_i_gid_high;
            uint16_t l_i_checksum_lo;
            uint16_t l_i_reserved;
        } linux2;
    } osd2;
    uint16_t i_extra_isize;
    uint16_t i_checksum_hi;
    uint32_t i_ctime_extra;
    uint32_t i_mtime_extra;
    uint32_t i_atime_extra;
    uint32_t i_crtime;
    uint32_t i_crtime_extra;
    uint32_t i_version_hi;
    uint32_t i_projid;
} ext4_inode_t;

/* ── Extent tree 
typedef struct {
    uint16_t eh_magic;       /* 0xF30A */
    uint16_t eh_entries;
    uint16_t eh_max;
    uint16_t eh_depth;
    uint32_t eh_generation;
} ext4_extent_header_t;

#define EXT4_EXT_MAGIC 0xF30A

typedef struct {
    uint32_t ee_block;       /* first logical block */
    uint16_t ee_len;         /* number of blocks (bit15: uninitialized) */
    uint16_t ee_start_hi;    /* high 16 bits of physical block */
    uint32_t ee_start_lo;    /* low 32 bits of physical block */
} ext4_extent_t;

typedef struct {
    uint32_t ei_block;       /* first logical block of this index */
    uint32_t ei_leaf_lo;
    uint16_t ei_leaf_hi;
    uint16_t ei_unused;
} ext4_extent_idx_t;

/* ── Directory entry*/
typedef struct {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[EXT4_NAME_LEN];
} ext4_dir_entry_t;

/* Hash-tree directory (dx) root */
typedef struct {
    uint32_t reserved_zero;
    uint8_t  hash_version;
    uint8_t  info_length;
    uint8_t  indirect_levels;
    uint8_t  unused_flags;
} ext4_dx_root_info_t;

#pragma pack(pop)
