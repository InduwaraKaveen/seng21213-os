#include "../include/fs.h"
#include "../include/ramdisk.h"

typedef struct {
    uint8_t used;
    uint16_t inode_index;
    uint8_t flags;
    uint32_t position;
} file_desc_t;

static superblock_t superblock;

static uint8_t inode_bitmap[MAX_INODES / 8];
static uint8_t block_bitmap[DATA_BLOCKS / 8 + 1];

static file_desc_t fd_table[MAX_FDS];

typedef char inode_size_must_be_256[
    (sizeof(inode_t) == 256) ? 1 : -1
];

static uint32_t fs_strlen(const char *s)
{
    uint32_t n = 0;

    if (s == 0) {
        return 0;
    }

    while (s[n] != '\0') {
        n++;
    }

    return n;
}

static int fs_strcmp(const char *a, const char *b)
{
    if (a == 0 || b == 0) {
        return -1;
    }

    while (*a && *b && *a == *b) {
        a++;
        b++;
    }

    return (unsigned char)*a - (unsigned char)*b;
}

static int bitmap_test(const uint8_t *bitmap, uint32_t index)
{
    return (bitmap[index / 8] >> (index % 8)) & 1;
}

static void bitmap_set(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] |= (uint8_t)(1u << (index % 8));
}

static void bitmap_clear(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] &= (uint8_t)~(1u << (index % 8));
}

static uint32_t inode_offset(uint32_t index)
{
    return INODE_TABLE_OFF + (index * INODE_SIZE);
}

static uint32_t data_block_offset(uint32_t block)
{
    return DATA_OFF + (block * BLOCK_SIZE);
}

static void inode_read(uint32_t index, inode_t *inode)
{
    ramdisk_read(inode_offset(index), inode, sizeof(inode_t));
}

static void inode_write(uint32_t index, const inode_t *inode)
{
    ramdisk_write(inode_offset(index), inode, sizeof(inode_t));
}

static void metadata_save(void)
{
    ramdisk_write(SUPERBLOCK_OFF,
                  &superblock,
                  sizeof(superblock));

    ramdisk_write(INODE_BITMAP_OFF,
                  inode_bitmap,
                  sizeof(inode_bitmap));

    ramdisk_write(BLOCK_BITMAP_OFF,
                  block_bitmap,
                  sizeof(block_bitmap));
}

static void metadata_load(void)
{
    ramdisk_read(SUPERBLOCK_OFF,
                 &superblock,
                 sizeof(superblock));

    ramdisk_read(INODE_BITMAP_OFF,
                 inode_bitmap,
                 sizeof(inode_bitmap));

    ramdisk_read(BLOCK_BITMAP_OFF,
                 block_bitmap,
                 sizeof(block_bitmap));
}

static int inode_find(const char *name)
{
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        if (!bitmap_test(inode_bitmap, i)) {
            continue;
        }

        inode_t inode;
        inode_read(i, &inode);

        if (inode.type == INODE_FILE &&
            fs_strcmp(inode.name, name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

static int inode_alloc(void)
{
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        if (!bitmap_test(inode_bitmap, i)) {
            bitmap_set(inode_bitmap, i);

            if (superblock.free_inodes > 0) {
                superblock.free_inodes--;
            }

            return (int)i;
        }
    }

    return -1;
}

static void inode_free(uint32_t index)
{
    if (index >= MAX_INODES) {
        return;
    }

    if (bitmap_test(inode_bitmap, index)) {
        bitmap_clear(inode_bitmap, index);
        superblock.free_inodes++;
    }
}

static int block_alloc(void)
{
    for (uint32_t i = 0; i < DATA_BLOCKS; i++) {
        if (!bitmap_test(block_bitmap, i)) {
            bitmap_set(block_bitmap, i);

            if (superblock.free_blocks > 0) {
                superblock.free_blocks--;
            }

            return (int)i;
        }
    }

    return -1;
}

static void block_free(uint32_t index)
{
    if (index >= DATA_BLOCKS) {
        return;
    }

    if (bitmap_test(block_bitmap, index)) {
        bitmap_clear(block_bitmap, index);
        superblock.free_blocks++;
    }
}

static void inode_release_blocks(inode_t *inode)
{
    for (uint32_t i = 0; i < inode->block_count &&
                         i < INODE_DIRECT; i++) {
        block_free(inode->blocks[i]);
        inode->blocks[i] = 0;
    }

    inode->block_count = 0;
    inode->size = 0;
}

static void format_filesystem(void)
{
    ramdisk_init();

    for (uint32_t i = 0; i < sizeof(inode_bitmap); i++) {
        inode_bitmap[i] = 0;
    }

    for (uint32_t i = 0; i < sizeof(block_bitmap); i++) {
        block_bitmap[i] = 0;
    }

    superblock.magic = FS_MAGIC;
    superblock.total_blocks = TOTAL_BLOCKS;
    superblock.total_inodes = MAX_INODES;
    superblock.free_blocks = DATA_BLOCKS;
    superblock.free_inodes = MAX_INODES;
    superblock.inode_table_off = INODE_TABLE_OFF;
    superblock.data_off = DATA_OFF;

    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].used = 0;
        fd_table[i].inode_index = 0;
        fd_table[i].flags = 0;
        fd_table[i].position = 0;
    }

    metadata_save();
}

void fs_init(void)
{
    metadata_load();

    if (superblock.magic != FS_MAGIC ||
        superblock.total_blocks != TOTAL_BLOCKS ||
        superblock.total_inodes != MAX_INODES ||
        superblock.inode_table_off != INODE_TABLE_OFF ||
        superblock.data_off != DATA_OFF) {
        format_filesystem();
        return;
    }

    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].used = 0;
        fd_table[i].inode_index = 0;
        fd_table[i].flags = 0;
        fd_table[i].position = 0;
    }
}

int fs_open(const char *name, int flags)
{
    if (name == 0 || name[0] == '\0') {
        return -1;
    }

    uint32_t name_len = fs_strlen(name);

    if (name_len >= 28) {
        return -1;
    }

    int inode_index = inode_find(name);

    if (inode_index < 0) {
        if (!(flags & O_CREAT)) {
            return -1;
        }

        inode_index = inode_alloc();

        if (inode_index < 0) {
            return -1;
        }

        inode_t inode;

        for (uint32_t i = 0; i < sizeof(inode_t); i++) {
            ((uint8_t *)&inode)[i] = 0;
        }

        inode.type = INODE_FILE;

        for (uint32_t i = 0; i <= name_len; i++) {
            inode.name[i] = name[i];
        }

        inode_write((uint32_t)inode_index, &inode);
        metadata_save();
    }

    if (flags & O_TRUNC) {
        inode_t inode;
        inode_read((uint32_t)inode_index, &inode);

        inode_release_blocks(&inode);
        inode_write((uint32_t)inode_index, &inode);

        metadata_save();
    }

    for (int fd = 0; fd < MAX_FDS; fd++) {
        if (!fd_table[fd].used) {
            fd_table[fd].used = 1;
            fd_table[fd].inode_index = (uint16_t)inode_index;
            fd_table[fd].flags = (uint8_t)flags;
            fd_table[fd].position = 0;

            return fd;
        }
    }

    return -1;
}

int fs_read(int fd, void *buf, int n)
{
    if (fd < 0 || fd >= MAX_FDS ||
        buf == 0 || n <= 0) {
        return 0;
    }

    if (!fd_table[fd].used) {
        return -1;
    }

    if (!(fd_table[fd].flags & O_RDONLY)) {
        return -1;
    }

    inode_t inode;
    inode_read(fd_table[fd].inode_index, &inode);

    if (fd_table[fd].position >= inode.size) {
        return 0;
    }

    uint32_t remaining =
        inode.size - fd_table[fd].position;

    uint32_t count = (uint32_t)n;

    if (count > remaining) {
        count = remaining;
    }

    uint32_t total = 0;

    while (total < count) {
        uint32_t position =
            fd_table[fd].position + total;

        uint32_t block_index = position / BLOCK_SIZE;
        uint32_t block_offset = position % BLOCK_SIZE;

        if (block_index >= inode.block_count ||
            block_index >= INODE_DIRECT) {
            break;
        }

        uint32_t chunk = BLOCK_SIZE - block_offset;

        if (chunk > count - total) {
            chunk = count - total;
        }

        ramdisk_read(
            data_block_offset(inode.blocks[block_index]) +
                block_offset,
            (uint8_t *)buf + total,
            chunk
        );

        total += chunk;
    }

    fd_table[fd].position += total;

    return (int)total;
}

int fs_write(int fd, const void *buf, int n)
{
    if (fd < 0 || fd >= MAX_FDS ||
        buf == 0 || n <= 0) {
        return 0;
    }

    if (!fd_table[fd].used) {
        return -1;
    }

    if (!(fd_table[fd].flags & O_WRONLY)) {
        return -1;
    }

    inode_t inode;
    inode_read(fd_table[fd].inode_index, &inode);

    uint32_t max_size = INODE_DIRECT * BLOCK_SIZE;

    if (fd_table[fd].position >= max_size) {
        return 0;
    }

    uint32_t available =
        max_size - fd_table[fd].position;

    uint32_t count = (uint32_t)n;

    if (count > available) {
        count = available;
    }

    uint32_t total = 0;

    while (total < count) {
        uint32_t position =
            fd_table[fd].position + total;

        uint32_t block_index = position / BLOCK_SIZE;
        uint32_t block_offset = position % BLOCK_SIZE;

        if (block_index >= INODE_DIRECT) {
            break;
        }

        if (block_index >= inode.block_count) {
            int new_block = block_alloc();

            if (new_block < 0) {
                break;
            }

            inode.blocks[block_index] = (uint32_t)new_block;
            inode.block_count++;
        }

        uint32_t chunk = BLOCK_SIZE - block_offset;

        if (chunk > count - total) {
            chunk = count - total;
        }

        ramdisk_write(
            data_block_offset(inode.blocks[block_index]) +
                block_offset,
            (const uint8_t *)buf + total,
            chunk
        );

        total += chunk;
    }

    fd_table[fd].position += total;

    if (fd_table[fd].position > inode.size) {
        inode.size = fd_table[fd].position;
    }

    inode_write(fd_table[fd].inode_index, &inode);
    metadata_save();

    return (int)total;
}

void fs_close(int fd)
{
    if (fd < 0 || fd >= MAX_FDS) {
        return;
    }

    fd_table[fd].used = 0;
    fd_table[fd].inode_index = 0;
    fd_table[fd].flags = 0;
    fd_table[fd].position = 0;
}

int fs_unlink(const char *name)
{
    if (name == 0 || name[0] == '\0') {
        return -1;
    }

    int inode_index = inode_find(name);

    if (inode_index < 0) {
        return -1;
    }

    inode_t inode;
    inode_read((uint32_t)inode_index, &inode);

    inode_release_blocks(&inode);

    inode_write((uint32_t)inode_index, &inode);
    inode_free((uint32_t)inode_index);

    metadata_save();

    return 0;
}

int fs_ls(inode_t *out, int max)
{
    if (out == 0 || max <= 0) {
        return 0;
    }

    int count = 0;

    for (uint32_t i = 0;
         i < MAX_INODES && count < max;
         i++) {

        if (!bitmap_test(inode_bitmap, i)) {
            continue;
        }

        inode_t inode;
        inode_read(i, &inode);

        if (inode.type != INODE_FILE) {
            continue;
        }

        out[count++] = inode;
    }

    return count;
}
