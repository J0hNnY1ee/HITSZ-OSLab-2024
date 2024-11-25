#ifndef _J1EEFS_H_
#define _J1EEFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"
#include "stdint.h"

#define J1EEFS_MAGIC                  /* TODO: Define by yourself */
#define J1EEFS_DEFAULT_PERM    0777   /* 全权限打开 */

/******************************************************************************
* SECTION: j1eefs.c
*******************************************************************************/
void* 			   j1eefs_init(struct fuse_conn_info *);
void  			   j1eefs_destroy(void *);
int   			   j1eefs_mkdir(const char *, mode_t);
int   			   j1eefs_getattr(const char *, struct stat *);
int   			   j1eefs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   j1eefs_mknod(const char *, mode_t, dev_t);
int   			   j1eefs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   j1eefs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   j1eefs_access(const char *, int);
int   			   j1eefs_unlink(const char *);
int   			   j1eefs_rmdir(const char *);
int   			   j1eefs_rename(const char *, const char *);
int   			   j1eefs_utimens(const char *, const struct timespec tv[2]);
int   			   j1eefs_truncate(const char *, off_t);
			
int   			   j1eefs_open(const char *, struct fuse_file_info *);
int   			   j1eefs_opendir(const char *, struct fuse_file_info *);

#endif  /* _j1eefs_H_ */