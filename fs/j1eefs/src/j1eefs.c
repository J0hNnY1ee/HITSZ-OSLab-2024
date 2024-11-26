#define _XOPEN_SOURCE 700
#include "../include/j1eefs.h"
/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }
/******************************************************************************
* SECTION: global region
*******************************************************************************/
struct jfs_super      jfs_super; 
struct custom_options jfs_options;
/******************************************************************************
* SECTION: Global Static Var
*******************************************************************************/
static const struct fuse_opt option_spec[] = {
	OPTION("--device=%s", device),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

static struct fuse_operations operations = {
	.init = jfs_init,						          /* mount文件系统 */		
	.destroy = jfs_destroy,							  /* umount文件系统 */
	.mkdir = jfs_mkdir,								  /* 建目录，mkdir */
	.getattr = jfs_getattr,							  /* 获取文件属性，类似stat，必须完成 */
	.readdir = jfs_readdir,							  /* 填充dentrys */
	.mknod = jfs_mknod,							      /* 创建文件，touch相关 */
	.write = jfs_write,								  /* 写入文件 */
	.read = jfs_read,								  /* 读文件 */
	.utimens = jfs_utimens,							  /* 修改时间，忽略，避免touch报错 */
	.truncate = jfs_truncate,						  /* 改变文件大小 */
	.unlink = jfs_unlink,							  /* 删除文件 */
	.rmdir	= jfs_rmdir,							  /* 删除目录， rm -r */
	.rename = jfs_rename,							  /* 重命名，mv */
	.readlink = jfs_readlink,						  /* 读链接 */
	.symlink = jfs_symlink,							  /* 软链接 */

	.open = jfs_open,							
	.opendir = jfs_opendir,
	.access = jfs_access
};
/******************************************************************************
* SECTION: Function Implementation
*******************************************************************************/
void* jfs_init(struct fuse_conn_info * conn_info) {
	if (jfs_mount(jfs_options) != JFS_ERROR_NONE) {
        JFS_DBG("[%s] mount error\n", __func__);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	} 
	return NULL;
}

void jfs_destroy(void* p) {
	if (jfs_umount() != JFS_ERROR_NONE) {
		JFS_DBG("[%s] unmount error\n", __func__);
		fuse_exit(fuse_get_context()->fuse);
		return;
	}
	return;
}
/**
 * @brief 
 * 
 * @param path 
 * @param mode 
 * @return int 
 */
int jfs_mkdir(const char* path, mode_t mode) {
	(void)mode;
	boolean is_find, is_root;
	char* fname;
	struct jfs_dentry* last_dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_dentry* dentry;
	struct jfs_inode*  inode;

	if (is_find) {
		return -JFS_ERROR_EXISTS;
	}

	if (JFS_IS_REG(last_dentry->inode)) {
		return -JFS_ERROR_UNSUPPORTED;
	}

	fname  = jfs_get_fname(path);
	dentry = new_dentry(fname, JFS_DIR); 
	dentry->parent = last_dentry;
	inode  = jfs_alloc_inode(dentry);
	jfs_alloc_dentry(last_dentry->inode, dentry);
	
	return JFS_ERROR_NONE;
}
/**
 * @brief 获取文件属性
 * 
 * @param path 相对于挂载点的路径
 * @param jfs_stat 返回状态
 * @return int 
 */
int jfs_getattr(const char* path, struct stat * jfs_stat) {
	boolean	is_find, is_root;
	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
	if (is_find == FALSE) {
		return -JFS_ERROR_NOTFOUND;
	}

	if (JFS_IS_DIR(dentry->inode)) {
		jfs_stat->st_mode = S_IFDIR | JFS_DEFAULT_PERM;
		jfs_stat->st_size = dentry->inode->dir_cnt * sizeof(struct jfs_dentry_d);
	}
	else if (JFS_IS_REG(dentry->inode)) {
		jfs_stat->st_mode = S_IFREG | JFS_DEFAULT_PERM;
		jfs_stat->st_size = dentry->inode->size;
	}
	else if (JFS_IS_SYM_LINK(dentry->inode)) {
		jfs_stat->st_mode = S_IFLNK | JFS_DEFAULT_PERM;
		jfs_stat->st_size = dentry->inode->size;
	}

	jfs_stat->st_nlink = 1;
	jfs_stat->st_uid 	 = getuid();
	jfs_stat->st_gid 	 = getgid();
	jfs_stat->st_atime   = time(NULL);
	jfs_stat->st_mtime   = time(NULL);
	jfs_stat->st_blksize = JFS_IO_SZ();

	if (is_root) {
		jfs_stat->st_size	= jfs_super.sz_usage; 
		jfs_stat->st_blocks = JFS_DISK_SZ() / JFS_IO_SZ();
		jfs_stat->st_nlink  = 2;		/* !特殊，根目录link数为2 */
	}
	return JFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param path 
 * @param buf 
 * @param filler 参数讲解:
 * 
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry
 * 
 * @param offset 
 * @param fi 
 * @return int 
 */
int jfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
			    struct fuse_file_info * fi) {
    boolean	is_find, is_root;
	int		cur_dir = offset;

	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_dentry* sub_dentry;
	struct jfs_inode* inode;
	if (is_find) {
		inode = dentry->inode;
		sub_dentry = jfs_get_dentry(inode, cur_dir);
		if (sub_dentry) {
			filler(buf, sub_dentry->fname, NULL, ++offset);
		}
		return JFS_ERROR_NONE;
	}
	return -JFS_ERROR_NOTFOUND;
}
/**
 * @brief 
 * 
 * @param path 
 * @param mode 
 * @param fi 
 * @return int 
 */
int jfs_mknod(const char* path, mode_t mode, dev_t dev) {
	boolean	is_find, is_root;
	
	struct jfs_dentry* last_dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_dentry* dentry;
	struct jfs_inode* inode;
	char* fname;
	
	if (is_find == TRUE) {
		return -JFS_ERROR_EXISTS;
	}

	fname = jfs_get_fname(path);
	
	if (S_ISREG(mode)) {
		dentry = new_dentry(fname, JFS_REG_FILE);
	}
	else if (S_ISDIR(mode)) {
		dentry = new_dentry(fname, JFS_DIR);
	}
	else {
		dentry = new_dentry(fname, JFS_REG_FILE);
	}
	dentry->parent = last_dentry;
	inode = jfs_alloc_inode(dentry);
	jfs_alloc_dentry(last_dentry->inode, dentry);

	return JFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param path 
 * @param buf 
 * @param size 
 * @param offset 
 * @param fi 
 * @return int 
 */
int jfs_write(const char* path, const char* buf, size_t size, off_t offset,
		        struct fuse_file_info* fi) {
    boolean	is_find, is_root;
	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_inode*  inode;
	
	if (is_find == FALSE) {
		return -JFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;
	
	if (JFS_IS_DIR(inode)) {
		return -JFS_ERROR_ISDIR;	
	}

	if (inode->size < offset) {
		return -JFS_ERROR_SEEK;
	}

	memcpy(inode->data + offset, buf, size);
	inode->size = offset + size > inode->size ? offset + size : inode->size;
	
	return size;
}
/**
 * @brief 
 * 
 * @param path 
 * @param buf 
 * @param size 
 * @param offset 
 * @param fi 
 * @return int 
 */
int jfs_read(const char* path, char* buf, size_t size, off_t offset,
		       struct fuse_file_info* fi) {
	boolean	is_find, is_root;
	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_inode*  inode;

	if (is_find == FALSE) {
		return -JFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;
	
	if (JFS_IS_DIR(inode)) {
		return -JFS_ERROR_ISDIR;	
	}

	if (inode->size < offset) {
		return -JFS_ERROR_SEEK;
	}

	memcpy(buf, inode->data + offset, size);

	return size;			   
}
/**
 * @brief 
 * 
 * @param path 
 * @return int 
 */
int jfs_unlink(const char* path) {
	boolean	is_find, is_root;
	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_inode*  inode;

	if (is_find == FALSE) {
		return -JFS_ERROR_NOTFOUND;
	}

	inode = dentry->inode;

	jfs_drop_inode(inode);
	jfs_drop_dentry(dentry->parent->inode, dentry);
	return JFS_ERROR_NONE;
}
/**
 * @brief 删除路径时的步骤
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * @param path 
 * @return int 
 */
int jfs_rmdir(const char* path) {
	return jfs_unlink(path);
}
/**
 * @brief 
 * 
 * @param from 
 * @param to 
 * @return int 
 */
int jfs_rename(const char* from, const char* to) {
	int ret = JFS_ERROR_NONE;
	boolean	is_find, is_root;
	struct jfs_dentry* from_dentry = jfs_lookup(from, &is_find, &is_root);
	struct jfs_inode*  from_inode;
	struct jfs_dentry* to_dentry;
	mode_t mode = 0;
	if (is_find == FALSE) {
		return -JFS_ERROR_NOTFOUND;
	}

	if (strcmp(from, to) == 0) {
		return JFS_ERROR_NONE;
	}

	from_inode = from_dentry->inode;
	
	if (JFS_IS_DIR(from_inode)) {
		mode = S_IFDIR;
	}
	else if (JFS_IS_REG(from_inode)) {
		mode = S_IFREG;
	}
	
	ret = jfs_mknod(to, mode, NULL);
	if (ret != JFS_ERROR_NONE) {					  /* 保证目的文件不存在 */
		return ret;
	}
	
	to_dentry = jfs_lookup(to, &is_find, &is_root);	  
	jfs_drop_inode(to_dentry->inode);				  /* 保证生成的inode被释放 */	
	to_dentry->ino = from_inode->ino;				  /* 指向新的inode */
	to_dentry->inode = from_inode;
	
	jfs_drop_dentry(from_dentry->parent->inode, from_dentry);
	return ret;
}
/**
 * @brief 
 * 
 * @param path - Where the link points
 * @param link - The link itself
 * @return int 
 */
// int jfs_symlink(const char* path, const char* link){
// 	int ret = JFS_ERROR_NONE;
// 	boolean	is_find, is_root;
// 	ret = jfs_mknod(link, S_IFREG, NULL);
// 	struct jfs_dentry* dentry = jfs_lookup(link, &is_find, &is_root);
// 	if (is_find == FALSE) {
// 		return -JFS_ERROR_NOTFOUND;
// 	}
// 	dentry->ftype = JFS_SYM_LINK;
// 	struct jfs_inode* inode = dentry->inode;
// 	memcpy(inode->target_path, path, JFS_MAX_FILE_NAME);
// 	return ret;
// }
/**
 * @brief 
 * 
 * @param path 
 * @param buf
 * @param size 
 * @return int 
 */
// int jfs_readlink (const char *path, char *buf, size_t size){
// 	/* JFS 暂未实现硬链接，只支持软链接 */
// 	boolean	is_find, is_root;
// 	ssize_t llen;
// 	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
// 	if (is_find == FALSE) {
// 		return -JFS_ERROR_NOTFOUND;
// 	}
// 	if (dentry->ftype != JFS_SYM_LINK){
// 		return -JFS_ERROR_INVAL;
// 	}
// 	struct jfs_inode* inode = dentry->inode;
// 	llen = strlen(inode->target_path);
// 	if(size < 0){
// 		return -JFS_ERROR_INVAL;
// 	}else{
// 		if(llen > size){
// 			strncpy(buf, inode->target_path, size);
// 			buf[size] = '\0';
// 		}else{
// 			strncpy(buf, inode->target_path, llen);
// 			buf[llen] = '\0';
// 		}
// 	}
// 	return JFS_ERROR_NONE;
// }
/**
 * @brief 
 * 
 * @param path 
 * @param fi 
 * @return int 
 */
int jfs_open(const char* path, struct fuse_file_info* fi) {
	return JFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param path 
 * @param fi 
 * @return int 
 */
int jfs_opendir(const char* path, struct fuse_file_info* fi) {
	return JFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param path 
 * @param type 
 * @return boolean 
 */
boolean jfs_access(const char* path, int type) {
	boolean	is_find, is_root;
	boolean is_access_ok = FALSE;
	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_inode*  inode;

	switch (type)
	{
	case R_OK:
		is_access_ok = TRUE;
		break;
	case F_OK:
		if (is_find) {
			is_access_ok = TRUE;
		}
		break;
	case W_OK:
		is_access_ok = TRUE;
		break;
	case X_OK:
		is_access_ok = TRUE;
		break;
	default:
		break;
	}
	return is_access_ok ? JFS_ERROR_NONE : -JFS_ERROR_ACCESS;
}	
/**
 * @brief 修改时间，为了不让touch报错
 * 
 * @param path 
 * @param tv 
 * @return int 
 */
int jfs_utimens(const char* path, const struct timespec tv[2]) {
	(void)path;
	return JFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param path 
 * @param offset 
 * @return int 
 */
int jfs_truncate(const char* path, off_t offset) {
	boolean	is_find, is_root;
	struct jfs_dentry* dentry = jfs_lookup(path, &is_find, &is_root);
	struct jfs_inode*  inode;
	
	if (is_find == FALSE) {
		return -JFS_ERROR_NOTFOUND;
	}
	
	inode = dentry->inode;

	if (JFS_IS_DIR(inode)) {
		return -JFS_ERROR_ISDIR;
	}

	inode->size = offset;

	return JFS_ERROR_NONE;
}
/**
 * @brief 展示jfs用法
 * 
 */
void jfs_usage() {
	printf("Sample File System (JFS)\n");
	printf("=================================================================\n");
	printf("Author: Deadpool <deadpoolmine@qq.com>\n");
	printf("Description: A Filesystem in UserSpacE (FUSE) sample file system \n");
	printf("\n");
	printf("Usage: ./jfs-fuse --device=[device path] mntpoint\n");
	printf("mount device to mntpoint with JFS\n");
	printf("=================================================================\n");
	printf("FUSE general options\n");
	return;
}
/******************************************************************************
* SECTION: FS Specific Structure
*******************************************************************************/
int main(int argc, char **argv)
{
    int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	jfs_options.device = strdup("/dev/ddriver");

	if (fuse_opt_parse(&args, &jfs_options, option_spec, NULL) == -1)
		return -JFS_ERROR_INVAL;
	
	if (jfs_options.show_help) {
		jfs_usage();
		fuse_opt_add_arg(&args, "--help");
		args.argv[0][0] = '\0';
	}
	
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}
