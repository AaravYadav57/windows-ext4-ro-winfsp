#pragma once
/*
* ext4fs.h - ext4 filesystem reader api thing
*
* supports:
* * ext4 (ext2/ext3/ too i think)
* * 64 bit block numbers
* * extent trees
* * old indirect block stuff from ext2/ext3
* * htree dirs
* * inline data (small files and symlinks inside inode)
* * large inodes / extra isize stuff
* * symlinks
*
* probably more stuff too idk
  */


#include <windows.h>
#include <stdint.h>
#include "ext4.h"
#include "diskio.h"

#define EXT4FS_MAX_PATH     4096
#define EXT4FS_CACHE_BLOCKS 256    /* LRU block cache size */

/* ── Filesystem context  */
typedef struct {
    disk_handle_t  disk;
    ext4_super_block_t sb;

    /* derived from superblock */
    uint32_t  block_size;
    uint32_t  inodes_per_group;
    uint32_t  inode_size;
    uint64_t  total_blocks;
    uint32_t  total_groups;
    uint32_t  gdt_size;          /* bytes per group descriptor */
    BOOL      has_64bit;
    BOOL      has_extents;
    BOOL      has_huge_file;
    BOOL      has_inline_data;

    /* block cache */
    struct {
        uint64_t block_no;
        uint8_t *data;
        uint64_t last_used;
    } cache[EXT4FS_CACHE_BLOCKS];
    uint64_t  cache_clock;
} ext4fs_t;

/* ── Directory iterator */
typedef struct {
    ext4fs_t       *fs;
    uint32_t        ino;
    uint64_t        dir_size;
    uint64_t        pos;          /* byte offset in directory data */
    uint8_t        *block_buf;    /* current block buffer (block_size bytes) */
    uint64_t        cur_block;    /* logical block index (in directory data) */
    uint64_t        cur_block_no; /* physical block number loaded */
} ext4_dir_iter_t;

/* ── Resolved directory entry  */
typedef struct {
    uint32_t  inode;
    uint8_t   file_type;           /* EXT4_FT_* */
    char      name[EXT4_NAME_LEN + 1];
    uint8_t   name_len;
} ext4_dirent_t;

/* ── Stat-like info  */
typedef struct {
    uint32_t  mode;
    uint64_t  size;
    uint32_t  uid, gid;
    uint32_t  atime, mtime, ctime, crtime;
    uint32_t  atime_nsec, mtime_nsec, ctime_nsec, crtime_nsec;
    uint32_t  nlink;
    uint64_t  blocks;           /* 512-byte blocks */
    uint32_t  flags;
    uint32_t  ino;
} ext4_stat_t;

/* ── API ─*/

/* Open / close */
BOOL ext4fs_open(disk_handle_t *disk, ext4fs_t *out);
void ext4fs_close(ext4fs_t *fs);

/* Block I/O */
BOOL ext4fs_read_block(ext4fs_t *fs, uint64_t block_no, void *buf);

/* Inode operations */
BOOL ext4fs_read_inode(ext4fs_t *fs, uint32_t ino, ext4_inode_t *out);
BOOL ext4fs_stat(ext4fs_t *fs, uint32_t ino, ext4_stat_t *out);

/* File data */
BOOL ext4fs_read_file(ext4fs_t *fs, uint32_t ino,
                      uint64_t offset, uint64_t length,
                      void *buf, uint64_t *bytes_read);

/* Logical block → physical block mapping */
BOOL ext4fs_map_block(ext4fs_t *fs, const ext4_inode_t *inode,
                      uint64_t logical_block, uint64_t *phys_block);

/* Symlink target */
BOOL ext4fs_readlink(ext4fs_t *fs, uint32_t ino,
                     char *buf, uint32_t buf_size, uint32_t *len_out);

/* Directory operations */
BOOL ext4_dir_iter_open(ext4fs_t *fs, uint32_t dir_ino, ext4_dir_iter_t *out);
BOOL ext4_dir_iter_next(ext4_dir_iter_t *it, ext4_dirent_t *entry);
void ext4_dir_iter_close(ext4_dir_iter_t *it);

/* Path lookup: returns inode number or 0 on failure */
uint32_t ext4fs_lookup(ext4fs_t *fs, const char *path);

/* Volume label */
void ext4fs_volume_label(ext4fs_t *fs, char *buf, uint32_t buf_size);
