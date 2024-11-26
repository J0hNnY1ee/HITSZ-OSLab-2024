#ifndef _JFS_H_
#define _JFS_H_

#define FUSE_USE_VERSION 26
#include "ddriver.h"
#include "errno.h"
#include "fcntl.h"
#include "fuse.h"
#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "types.h"
#include <stddef.h>
#include <unistd.h>

/******************************************************************************
 * SECTION: macro debug
 *******************************************************************************/
#define JFS_DBG(fmt, ...)                                                      \
  do {                                                                         \
    printf("JFS_DBG: " fmt, ##__VA_ARGS__);                                    \
  } while (0)
/******************************************************************************
 * SECTION: j1eefs_utils.c
 *******************************************************************************/
char *jfs_get_fname(const char *path);
int jfs_calc_lvl(const char *path);
int jfs_driver_read(int offset, uint8_t *out_content, int size);
int jfs_driver_write(int offset, uint8_t *in_content, int size);

int jfs_mount(struct custom_options options);
int jfs_umount();

int jfs_alloc_data();
int jfs_alloc_dentry(struct jfs_inode *inode, struct jfs_dentry *dentry);
int jfs_drop_dentry(struct jfs_inode *inode, struct jfs_dentry *dentry);
struct jfs_inode *jfs_alloc_inode(struct jfs_dentry *dentry);
int jfs_sync_inode(struct jfs_inode *inode);
int jfs_drop_inode(struct jfs_inode *inode);
struct jfs_inode *jfs_read_inode(struct jfs_dentry *dentry, int ino);
struct jfs_dentry *jfs_get_dentry(struct jfs_inode *inode, int dir);

struct jfs_dentry *jfs_lookup(const char *path, boolean *is_find,
                              boolean *is_root);
/******************************************************************************
 * SECTION: j1eefs.c
 *******************************************************************************/
void *jfs_init(struct fuse_conn_info *);
void jfs_destroy(void *);
int jfs_mkdir(const char *, mode_t);
int jfs_getattr(const char *, struct stat *);
int jfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
                struct fuse_file_info *);
int jfs_mknod(const char *, mode_t, dev_t);
int jfs_write(const char *, const char *, size_t, off_t,
              struct fuse_file_info *);
int jfs_read(const char *, char *, size_t, off_t, struct fuse_file_info *);
int jfs_unlink(const char *);
int jfs_rmdir(const char *);
int jfs_rename(const char *, const char *);
int jfs_utimens(const char *, const struct timespec tv[2]);
int jfs_truncate(const char *, off_t);

int 			   jfs_symlink(const char *, const char *);
int 			   jfs_readlink(const char *, char *, size_t);

int jfs_open(const char *, struct fuse_file_info *);
int jfs_opendir(const char *, struct fuse_file_info *);
int   			   jfs_access(const char *, int);
/******************************************************************************
 * SECTION: j1eefs_debug.c
 *******************************************************************************/
void jfs_dump_map();
#endif
