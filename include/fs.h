#ifndef FS_H
#define FS_H

#include "types.h"

#define FS_MAGIC        0x21213F5u
#define FS_SIZE         (1024u * 1024u)
#define BLOCK_SIZE      512u
#define TOTAL_BLOCKS    (FS_SIZE / BLOCK_SIZE)

#define MAX_INODES      1024u
#define INODE_SIZE      256u
#define INODE_DIRECT    8u

#define SUPERBLOCK_OFF   0x00000u
#define INODE_BITMAP_OFF 0x00200u
#define BLOCK_BITMAP_OFF 0x00400u
#define INODE_TABLE_OFF  0x00600u
#define DATA_OFF         0x40600u

#define DATA_BLOCKS      ((FS_SIZE - DATA_OFF) / BLOCK_SIZE)

#define MAX_FDS          16

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t inode_table_off;
    uint32_t data_off;
    uint8_t reserved[512 - 28];
} superblock_t;

typedef struct __attribute__((packed)) {
    uint32_t size;
    uint32_t blocks[INODE_DIRECT];
    uint32_t block_count;
    uint8_t type;
    char name[28];

    uint8_t reserved[
        INODE_SIZE -
        (4 + (INODE_DIRECT * 4) + 4 + 1 + 28)
    ];
} inode_t;

#define INODE_FREE 0
#define INODE_FILE 1
#define INODE_DIR  2

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_CREAT  0x04
#define O_TRUNC  0x08

void fs_init(void);

int fs_open(const char *name, int flags);
int fs_read(int fd, void *buf, int n);
int fs_write(int fd, const void *buf, int n);

void fs_close(int fd);

int fs_unlink(const char *name);

int fs_ls(inode_t *out, int max);

#endif
