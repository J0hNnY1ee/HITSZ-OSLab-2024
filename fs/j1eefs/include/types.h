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
    //JFS_SYM_LINK  // 不用实现链接
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
#define JFS_DATA_PER_FILE       7
#define JFS_DEFAULT_PERM        0777

#define JFS_IOC_MAGIC           'S'
#define JFS_IOC_SEEK            _IO(JFS_IOC_MAGIC, 0)

#define JFS_FLAG_BUF_DIRTY      0x1
#define JFS_FLAG_BUF_OCCUPY     0x2

// 磁盘布局设计,一个逻辑块能放8个inode
#define JFS_INODE_PER_BLK       8     // 一个逻辑块能放8个inode
#define JFS_SUPER_BLKS          1
#define JFS_INODE_MAP_BLKS      1
#define JFS_DATA_MAP_BLKS       1
#define JFS_INODE_BLKS          64    // 4096/8/8(磁盘大小为4096个逻辑块，维护一个文件需要8个逻辑块即一个索引块+七个数据块)
#define JFS_DATA_BLKS           4029  // 4096-64-1-1-1

/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define JFS_IO_SZ()                     (jfs_super.sz_io)    // 512B
#define JFS_BLK_SZ()                    (jfs_super.sz_blks)  // 1KB
#define JFS_DISK_SZ()                   (jfs_super.sz_disk)  // 4MB
#define JFS_DRIVER()                    (jfs_super.fd)
#define JFS_BLKS_SZ(blks)               ((blks) * JFS_BLK_SZ())
#define JFS_DENTRY_PER_BLK()            (JFS_BLK_SZ() / sizeof(struct jfs_dentry))

// 向下取整以及向上取整
#define JFS_ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define JFS_ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

// 复制文件名到某个dentry中
#define JFS_ASSIGN_FNAME(pjfs_dentry, _fname) memcpy(pjfs_dentry->fname, _fname, strlen(_fname))

// 计算inode和data的偏移量                                     
#define JFS_INO_OFS(ino)                (jfs_super.inode_offset + JFS_BLKS_SZ(ino))   // inode基地址初始偏移+前面的inode占用的空间
#define JFS_DATA_OFS(dno)               (jfs_super.data_offset + JFS_BLKS_SZ(dno))     // data基地址初始偏移+前面的data占用的空间

// 判断inode指向的是是目录还是普通文件
#define JFS_IS_DIR(pinode)              (pinode->dentry->ftype == JFS_DIR)
#define JFS_IS_REG(pinode)              (pinode->dentry->ftype == JFS_REG_FILE)

/******************************************************************************
* SECTION: FS Specific Structure - In memory structure
*******************************************************************************/
struct custom_options {
	const char*        device;
};

struct jfs_super {
    /* TODO: Define yourself */
    uint32_t           magic;             // 幻数
    int                fd;                // 文件描述符

    int                sz_io;             // 512B
    int                sz_blks;           // 1KB
    int                sz_disk;           // 4MB
    int                sz_usage;          // 已使用空间大小

    int                max_ino;           // 索引节点最大数量
    uint8_t*           map_inode;         // inode位图内存起点
    int                map_inode_blks;    // inode位图占用的逻辑块数量
    int                map_inode_offset;  // inode位图在磁盘中的偏移

    int                max_dno;           // 数据位图最大数量
    uint8_t*           map_data;          // data位图内存起点
    int                map_data_blks;     // data位图占用的逻辑块数量
    int                map_data_offset;   // data位图在磁盘中的偏移

    int                inode_offset;      // inode在磁盘中的偏移
    int                data_offset;       // data在磁盘中的偏移

    boolean            is_mounted;        // 是否挂载
    struct jfs_dentry* root_dentry;       // 根目录dentry
};

struct jfs_inode {
    /* TODO: Define yourself */
    uint32_t           ino;                               // 索引编号                         
    int                size;                              // 文件占用空间
    int                link;                              // 连接数默认为1(不考虑软链接和硬链接)
    int                block_pointer[JFS_DATA_PER_FILE];  // 数据块索引
    int                dir_cnt;                           // 如果是目录型文件，则代表有几个目录项
    struct jfs_dentry  *dentry;                           // 指向该inode的父dentry
    struct jfs_dentry  *dentrys;                          // 指向该inode的所有子dentry
    JFS_FILE_TYPE      ftype;                             // 文件类型
    uint8_t*           data[JFS_DATA_PER_FILE];           // 指向数据块的指针
    int                block_allocted;                    // 已分配数据块数量
};

struct jfs_dentry {
    /* TODO: Define yourself */
    char               fname[JFS_MAX_FILE_NAME];    // dentry指向的文件名
    struct jfs_dentry* parent;                      // 父目录的dentry            
    struct jfs_dentry* brother;                     // 兄弟dentry
    int                ino;                         // 指向的inode编号
    struct jfs_inode*  inode;                       // 指向的inode  
    JFS_FILE_TYPE      ftype;                       // 文件类型

};

// 生成新的dentry
static inline struct jfs_dentry* new_dentry(char * fname, JFS_FILE_TYPE ftype) {
    struct jfs_dentry * dentry = (struct jfs_dentry *)malloc(sizeof(struct jfs_dentry));
    memset(dentry, 0, sizeof(struct jfs_dentry));
    JFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;  
    return dentry;                                          
}

/******************************************************************************
* SECTION: FS Specific Structure - To Disk structure
*******************************************************************************/
struct jfs_super_d {
    uint32_t           magic;             // 幻数
    int                sz_usage;          // 已使用空间大小

    int                max_ino;           // 索引节点最大数量
    int                map_inode_blks;    // inode位图占用的逻辑块数量
    int                map_inode_offset;  // inode位图在磁盘中的偏移

    int                max_dno;           // 数据位图最大数量
    int                map_data_blks;     // data位图占用的逻辑块数量
    int                map_data_offset;   // data位图在磁盘中的偏移

    int                inode_offset;      // inode在磁盘中的偏移
    int                data_offset;       // data在磁盘中的偏移
};

struct jfs_inode_d {
    uint32_t           ino;                               // 索引编号                         
    int                size;                              // 文件占用空间(用了多少个逻辑块) 
    int                link;                              // 连接数默认为1(不考虑软链接和硬链接)
    int                block_pointer[JFS_DATA_PER_FILE];  // 数据块索引
    int                dir_cnt;                           // 如果是目录型文件，则代表有几个目录项
    JFS_FILE_TYPE      ftype;                             // 文件类型
    int                block_allocted;                    // 已分配数据块数量
};

struct jfs_dentry_d {
    char               fname[JFS_MAX_FILE_NAME];    // dentry指向的文件名
    int                ino;                         // 指向的inode编号
    JFS_FILE_TYPE      ftype;                       // 文件类型
};

#endif /* _TYPES_H_ */