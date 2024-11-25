#ifndef _TYPES_H_
#define _TYPES_H_

/******************************************************************************
* SECTION: Type def
*******************************************************************************/
typedef int          boolean;
typedef uint16_t     flag16;

typedef enum jfs_file_type {
    JFS_REG_FILE,
    JFS_DIR,
    // JFS_SYM_LINK
} JFS_FILE_TYPE;
/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define JFS_MAGIC_NUM           0x52415453  
#define JFS_SUPER_OFS           0
#define JFS_ROOT_INO            0



#define JFS_ERROR_NONE          0
#define JFS_ERROR_ACCESS        EACCES
#define JFS_ERROR_SEEK          ESPIPE     
#define JFS_ERROR_ISDIR         EISDIR
#define JFS_ERROR_NOSPACE       ENOSPC
#define JFS_ERROR_EXISTS        EEXIST
#define JFS_ERROR_NOTFOUND      ENOENT
#define JFS_ERROR_UNSUPPORTED   ENXIO
#define JFS_ERROR_IO            EIO     /* Error Input/Output */
#define JFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define JFS_MAX_FILE_NAME       128
#define JFS_INODE_PER_FILE      1
#define JFS_DATA_PER_FILE       16
#define JFS_DEFAULT_PERM        0777

#define JFS_IOC_MAGIC           'S'
#define JFS_IOC_SEEK            _IO(JFS_IOC_MAGIC, 0)

#define JFS_FLAG_BUF_DIRTY      0x1
#define JFS_FLAG_BUF_OCCUPY     0x2
/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define JFS_IO_SZ()                     (jfs_super.sz_io)
#define JFS_DISK_SZ()                   (jfs_super.sz_disk)
#define JFS_DRIVER()                    (jfs_super.driver_fd)

#define JFS_ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define JFS_ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

#define JFS_BLKS_SZ(blks)               ((blks) * JFS_IO_SZ())
#define JFS_ASSIGN_FNAME(pjfs_dentry, _fname)\ 
                                        memcpy(pjfs_dentry->fname, _fname, strlen(_fname))
#define JFS_INO_OFS(ino)                (jfs_super.data_offset + (ino) * JFS_BLKS_SZ((\
                                        JFS_INODE_PER_FILE + JFS_DATA_PER_FILE)))
#define JFS_DATA_OFS(ino)               (JFS_INO_OFS(ino) + JFS_BLKS_SZ(JFS_INODE_PER_FILE))

#define JFS_IS_DIR(pinode)              (pinode->dentry->ftype == JFS_DIR)
#define JFS_IS_REG(pinode)              (pinode->dentry->ftype == JFS_REG_FILE)
#define JFS_IS_SYM_LINK(pinode)         (pinode->dentry->ftype == JFS_SYM_LINK)
/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct jfs_dentry;
struct jfs_inode;
struct jfs_super;

struct custom_options {
	const char*        device;
	boolean            show_help;
};

struct jfs_inode
{
    int                ino;                           /* 在inode位图中的下标 */
    int                size;                          /* 文件已占用空间 */
    char               target_path[JFS_MAX_FILE_NAME];/* store traget path when it is a symlink */
    int                dir_cnt;
    struct jfs_dentry* dentry;                        /* 指向该inode的dentry */
    struct jfs_dentry* dentrys;                       /* 所有目录项 */
    uint8_t*           data;           
};  

struct jfs_dentry
{
    char               fname[JFS_MAX_FILE_NAME];
    struct jfs_dentry* parent;                        /* 父亲Inode的dentry */
    struct jfs_dentry* brother;                       /* 兄弟 */
    int                ino;
    struct jfs_inode*  inode;                         /* 指向inode */
    JFS_FILE_TYPE      ftype;
};

struct jfs_super
{
    int                driver_fd;
    
    int                sz_io;
    int                sz_disk;
    int                sz_usage;
    
    int                max_ino;
    uint8_t*           map_inode;
    int                map_inode_blks;
    int                map_inode_offset;
    
    int                data_offset;

    boolean            is_mounted;

    struct jfs_dentry* root_dentry;
};

static inline struct jfs_dentry* new_dentry(char * fname, JFS_FILE_TYPE ftype) {
    struct jfs_dentry * dentry = (struct jfs_dentry *)malloc(sizeof(struct jfs_dentry));
    memset(dentry, 0, sizeof(struct jfs_dentry));
    JFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;                                            
}
/******************************************************************************
* SECTION: FS Specific Structure - Disk structure
*******************************************************************************/
struct jfs_super_d
{
    uint32_t           magic_num;
    uint32_t           sz_usage;
    
    uint32_t           max_ino;
    uint32_t           map_inode_blks;
    uint32_t           map_inode_offset;
    uint32_t           data_offset;
};

struct jfs_inode_d
{
    uint32_t           ino;                           /* 在inode位图中的下标 */
    uint32_t           size;                          /* 文件已占用空间 */
    char               target_path[JFS_MAX_FILE_NAME];/* store traget path when it is a symlink */
    uint32_t           dir_cnt;
    JFS_FILE_TYPE      ftype;   
};  

struct jfs_dentry_d
{
    char               fname[JFS_MAX_FILE_NAME];
    JFS_FILE_TYPE      ftype;
    uint32_t           ino;                           /* 指向的ino号 */
};  


#endif /* _TYPES_H_ */