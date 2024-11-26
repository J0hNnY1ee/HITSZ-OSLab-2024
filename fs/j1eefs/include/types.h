#ifndef _TYPES_H_
#define _TYPES_H_

/******************************************************************************
 * SECTION: Type def
 *******************************************************************************/
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef int boolean;
typedef uint16_t flag16;

typedef enum jfs_file_type { // 定义文件类型
  JFS_REG_FILE,
  JFS_DIR,
  // JFS_SYM_LINK // 实验不要求链接
} JFS_FILE_TYPE;
/******************************************************************************
 * SECTION: Macro
 *******************************************************************************/
#define TRUE 1
#define FALSE 0
#define UINT32_BITS 32
#define UINT8_BITS 8

#define JFS_MAGIC_NUM 0x52415453 //宏定义幻数
#define JFS_SUPER_OFS 0
#define JFS_ROOT_INO 0

#define JFS_ERROR_NONE 0
#define JFS_ERROR_ACCESS EACCES
#define JFS_ERROR_SEEK ESPIPE
#define JFS_ERROR_ISDIR EISDIR
#define JFS_ERROR_NOSPACE ENOSPC
#define JFS_ERROR_EXISTS EEXIST
#define JFS_ERROR_NOTFOUND ENOENT
#define JFS_ERROR_UNSUPPORTED ENXIO
#define JFS_ERROR_IO EIO       /* Error Input/Output */
#define JFS_ERROR_INVAL EINVAL /* Invalid Args */

#define JFS_MAX_FILE_NAME 128
#define JFS_INODE_PER_FILE 1
#define JFS_DATA_PER_FILE 3 // Modified
#define JFS_DEFAULT_PERM 0777

#define JFS_IOC_MAGIC 'S'
#define JFS_IOC_SEEK _IO(JFS_IOC_MAGIC, 0)

#define JFS_FLAG_BUF_DIRTY 0x1
#define JFS_FLAG_BUF_OCCUPY 0x2

// Disk layout design
#define JFS_INODE_PER_BLK 8 // 8 inode per blk
#define JFS_SUPER_BLKS 1    // one super blk
#define JFS_INODE_MAP_BLKS 1
#define JFS_DATA_MAP_BLKS 1
#define JFS_INODE_BLKS 128 // 4096/4/8
#define JFS_DATA_BLKS 3965 // 4096-128-1-1-1
/******************************************************************************
 * SECTION: Macro Function
 *******************************************************************************/
#define JFS_IO_SZ() (jfs_super.sz_io)    // 512B
#define JFS_BLK_SZ() (nfs_super.sz_blks) // 1024B = 2 * 512B
#define JFS_BLKS_SZ(blks) ((blks)*JFS_IO_SZ())
#define JFS_DISK_SZ() (jfs_super.sz_disk) // 4MB
#define JFS_DRIVER() (jfs_super.driver_fd)

#define JFS_ROUND_DOWN(value, round)                                           \
  ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define JFS_ROUND_UP(value, round)                                             \
  ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

// copy filename to dentry
#define JFS_ASSIGN_FNAME(pjfs_dentry, _fname)                                  \
  memcpy(pjfs_dentry->fname, _fname, strlen(_fname))
// offset
#define JFS_INO_OFS(ino) (nfs_super.inode_offset + JFS_BLKS_SZ(ino))
#define JFS_DATA_OFS(dno) (nfs_super.data_offset + JFS_BLKS_SZ(dno))

#define JFS_IS_DIR(pinode) (pinode->dentry->ftype == JFS_DIR)
#define JFS_IS_REG(pinode) (pinode->dentry->ftype == JFS_REG_FILE)
// #define JFS_IS_SYM_LINK(pinode)         (pinode->dentry->ftype ==
// JFS_SYM_LINK)
/******************************************************************************
 * SECTION: FS Specific Structure - In memory structure
 *******************************************************************************/
struct jfs_dentry;
struct jfs_inode;
struct jfs_super;

struct custom_options {
  const char *device;
  boolean show_help;
};
struct jfs_super {
  uint32_t magic;
  int driver_fd;

  int sz_io;
  int sz_blks;
  int sz_disk;
  int sz_usage;

  // inode map
  int max_ino;
  uint8_t *map_inode;
  int map_inode_blks;
  int map_inode_offset;

  // dnode map
  int max_dno;
  uint8_t *map_data;
  int map_data_blks;
  int map_data_offset;

  int inode_offset;
  int data_offset;

  boolean is_mounted;

  struct jfs_dentry *root_dentry; // root directory
};
struct jfs_inode {
  uint32_t ino; // user uint for no ino < 0
  int size;
  int link;                             // link count
  int block_pointer[JFS_DATA_PER_FILE]; // 3
  int dir_cnt;
  struct jfs_dentry *dentry_pa;
  struct jfs_dentry *dentrys_child;
  JFS_FILE_TYPE ftype;
  uint8_t data[JFS_DATA_PER_FILE]; // 3
  int blocks_used_cnt;
};

struct jfs_dentry {
  char fname[JFS_MAX_FILE_NAME]; // filename dentry pointed
  struct jfs_dentry *parent;     /* 父亲Inode的dentry */
  struct jfs_dentry *brother;    /* 兄弟 */
  int ino;                       // inode number
  struct jfs_inode *inode;       /* 指向inode */
  JFS_FILE_TYPE ftype;
};

// create  a new dentry
static inline struct jfs_dentry *new_dentry(char *fname, JFS_FILE_TYPE ftype) {
  struct jfs_dentry *dentry =
      (struct jfs_dentry *)malloc(sizeof(struct jfs_dentry)); // allocate memory
  memset(dentry, 0, sizeof(struct jfs_dentry));
  JFS_ASSIGN_FNAME(dentry, fname); // copy filename to dentry
  dentry->ftype = ftype;
  dentry->ino = -1;
  dentry->inode = NULL;
  dentry->parent = NULL;
  dentry->brother = NULL;
  return dentry;
}
/******************************************************************************
 * SECTION: FS Specific Structure - Disk structure
 *******************************************************************************/
 // d means disk
struct jfs_super_d {
  uint32_t magic;

  int sz_usage;

  // inode map
  int max_ino;
  int map_inode_blks;
  int map_inode_offset;

  // dnode map
  int max_dno;
  int map_data_blks;
  int map_data_offset;

  int inode_offset;
  int data_offset;
};

struct jfs_inode_d {
  uint32_t ino;
  int size;
  int link;
  int block_pointer[JFS_DATA_PER_FILE];
  int dir_cnt;
  JFS_FILE_TYPE ftype;
  int block_allocted;
};

struct jfs_dentry_d {
  char fname[JFS_MAX_FILE_NAME];
  JFS_FILE_TYPE ftype;
  uint32_t ino; /* 指向的ino号 */
};

#endif /* _TYPES_H_ */