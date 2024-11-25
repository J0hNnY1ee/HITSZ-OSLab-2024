#include "../include/jfs.h"

extern struct jfs_super      jfs_super; 
extern struct custom_options jfs_options;

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* jfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}
/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int jfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}
/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int jfs_driver_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = JFS_ROUND_DOWN(offset, JFS_IO_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = JFS_ROUND_UP((size + bias), JFS_IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(JFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(JFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(JFS_DRIVER(), cur, JFS_IO_SZ());
        ddriver_read(JFS_DRIVER(), cur, JFS_IO_SZ());
        cur          += JFS_IO_SZ();
        size_aligned -= JFS_IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return JFS_ERROR_NONE;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int jfs_driver_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = JFS_ROUND_DOWN(offset, JFS_IO_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = JFS_ROUND_UP((size + bias), JFS_IO_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    jfs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(JFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(JFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(JFS_DRIVER(), cur, JFS_IO_SZ());
        ddriver_write(JFS_DRIVER(), cur, JFS_IO_SZ());
        cur          += JFS_IO_SZ();
        size_aligned -= JFS_IO_SZ();   
    }

    free(temp_content);
    return JFS_ERROR_NONE;
}
/**
 * @brief 将denry插入到inode中，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int jfs_alloc_dentry(struct jfs_inode* inode, struct jfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将dentry从inode的dentrys中取出
 * 
 * @param inode 一个目录的索引结点
 * @param dentry 该目录下的一个目录项
 * @return int 
 */
int jfs_drop_dentry(struct jfs_inode * inode, struct jfs_dentry * dentry) {
    boolean is_find = FALSE;
    struct jfs_dentry* dentry_cursor;
    dentry_cursor = inode->dentrys;
    
    if (dentry_cursor == dentry) {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry) {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find) {
        return -JFS_ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}
/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return jfs_inode
 */
struct jfs_inode* jfs_alloc_inode(struct jfs_dentry * dentry) {
    struct jfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find_free_entry = FALSE;
    /* 检查位图是否有空位 */
    for (byte_cursor = 0; byte_cursor < JFS_BLKS_SZ(jfs_super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((jfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                jfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor == jfs_super.max_ino)
        return -JFS_ERROR_NOSPACE;

    inode = (struct jfs_inode*)malloc(sizeof(struct jfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    
    if (JFS_IS_REG(inode)) {
        inode->data = (uint8_t *)malloc(JFS_BLKS_SZ(JFS_DATA_PER_FILE));
    }

    return inode;
}
/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int jfs_sync_inode(struct jfs_inode * inode) {
    struct jfs_inode_d  inode_d;
    struct jfs_dentry*  dentry_cursor;
    struct jfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    memcpy(inode_d.target_path, inode->target_path, JFS_MAX_FILE_NAME);
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    int offset;
    /* 先写inode本身 */
    if (jfs_driver_write(JFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct jfs_inode_d)) != JFS_ERROR_NONE) {
        JFS_DBG("[%s] io error\n", __func__);
        return -JFS_ERROR_IO;
    }

    /* 再写inode下方的数据 */
    if (JFS_IS_DIR(inode)) { /* 如果当前inode是目录，那么数据是目录项，且目录项的inode也要写回 */                          
        dentry_cursor = inode->dentrys;
        offset        = JFS_DATA_OFS(ino);
        while (dentry_cursor != NULL)
        {
            memcpy(dentry_d.fname, dentry_cursor->fname, JFS_MAX_FILE_NAME);
            dentry_d.ftype = dentry_cursor->ftype;
            dentry_d.ino = dentry_cursor->ino;
            if (jfs_driver_write(offset, (uint8_t *)&dentry_d, 
                                 sizeof(struct jfs_dentry_d)) != JFS_ERROR_NONE) {
                JFS_DBG("[%s] io error\n", __func__);
                return -JFS_ERROR_IO;                     
            }
            
            if (dentry_cursor->inode != NULL) {
                jfs_sync_inode(dentry_cursor->inode);
            }

            dentry_cursor = dentry_cursor->brother;
            offset += sizeof(struct jfs_dentry_d);
        }
    }
    else if (JFS_IS_REG(inode)) { /* 如果当前inode是文件，那么数据是文件内容，直接写即可 */
        if (jfs_driver_write(JFS_DATA_OFS(ino), inode->data, 
                             JFS_BLKS_SZ(JFS_DATA_PER_FILE)) != JFS_ERROR_NONE) {
            JFS_DBG("[%s] io error\n", __func__);
            return -JFS_ERROR_IO;
        }
    }
    return JFS_ERROR_NONE;
}
/**
 * @brief 删除内存中的一个inode
 * Case 1: Reg File
 * 
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Reg Dentry)
 *                       |
 *                      Inode  (Reg File)
 * 
 *  1) Step 1. Erase Bitmap     
 *  2) Step 2. Free Inode                      (Function of jfs_drop_inode)
 * ------------------------------------------------------------------------
 *  3) *Setp 3. Free Dentry belonging to Inode (Outsider)
 * ========================================================================
 * Case 2: Dir
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Dir Dentry)
 *                       |
 *                      Inode  (Dir)
 *                    /     \
 *                Dentry -> Dentry
 * 
 *   Recursive
 * @param inode 
 * @return int 
 */
int jfs_drop_inode(struct jfs_inode * inode) {
    struct jfs_dentry*  dentry_cursor;
    struct jfs_dentry*  dentry_to_free;
    struct jfs_inode*   inode_cursor;

    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    boolean is_find = FALSE;

    if (inode == jfs_super.root_dentry->inode) {
        return JFS_ERROR_INVAL;
    }

    if (JFS_IS_DIR(inode)) {
        dentry_cursor = inode->dentrys;
                                                      /* 递归向下drop */
        while (dentry_cursor)
        {   
            inode_cursor = dentry_cursor->inode;
            jfs_drop_inode(inode_cursor);
            jfs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        }

        for (byte_cursor = 0; byte_cursor < JFS_BLKS_SZ(jfs_super.map_inode_blks); 
            byte_cursor++)                            /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                     jfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                     is_find = TRUE;
                     break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE) {
                break;
            }
        }
    }
    else if (JFS_IS_REG(inode) || JFS_IS_SYM_LINK(inode)) {
        for (byte_cursor = 0; byte_cursor < JFS_BLKS_SZ(jfs_super.map_inode_blks); 
            byte_cursor++)                            /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                if (ino_cursor == inode->ino) {
                     jfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                     is_find = TRUE;
                     break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE) {
                break;
            }
        }
        if (inode->data)
            free(inode->data);
        free(inode);
    }
    return JFS_ERROR_NONE;
}
/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct jfs_inode* 
 */
struct jfs_inode* jfs_read_inode(struct jfs_dentry * dentry, int ino) {
    struct jfs_inode* inode = (struct jfs_inode*)malloc(sizeof(struct jfs_inode));
    struct jfs_inode_d inode_d;
    struct jfs_dentry* sub_dentry;
    struct jfs_dentry_d dentry_d;
    int    dir_cnt = 0, i;
    /* 从磁盘读索引结点 */
    if (jfs_driver_read(JFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct jfs_inode_d)) != JFS_ERROR_NONE) {
        JFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    memcpy(inode->target_path, inode_d.target_path, JFS_MAX_FILE_NAME);
    inode->dentry = dentry;
    inode->dentrys = NULL;
    /* 内存中的inode的数据或子目录项部分也需要读出 */
    if (JFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        for (i = 0; i < dir_cnt; i++)
        {
            if (jfs_driver_read(JFS_DATA_OFS(ino) + i * sizeof(struct jfs_dentry_d), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct jfs_dentry_d)) != JFS_ERROR_NONE) {
                JFS_DBG("[%s] io error\n", __func__);
                return NULL;
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino    = dentry_d.ino; 
            jfs_alloc_dentry(inode, sub_dentry);
        }
    }
    else if (JFS_IS_REG(inode)) {
        inode->data = (uint8_t *)malloc(JFS_BLKS_SZ(JFS_DATA_PER_FILE));
        if (jfs_driver_read(JFS_DATA_OFS(ino), (uint8_t *)inode->data, 
                            JFS_BLKS_SZ(JFS_DATA_PER_FILE)) != JFS_ERROR_NONE) {
            JFS_DBG("[%s] io error\n", __func__);
            return NULL;                    
        }
    }
    return inode;
}
/**
 * @brief 
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct jfs_dentry* 
 */
struct jfs_dentry* jfs_get_dentry(struct jfs_inode * inode, int dir) {
    struct jfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}
/**
 * @brief 查找文件或目录
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *  
 * 
 * 如果能查找到，返回该目录项
 * 如果查找不到，返回的是上一个有效的路径
 * 
 * path: /a/b/c
 *      1) find /'s inode     lvl = 1
 *      2) find a's dentry 
 *      3) find a's inode     lvl = 2
 *      4) find b's dentry    如果此时找不到了，is_find=FALSE且返回的是a的inode对应的dentry
 * 
 * @param path 
 * @return struct jfs_dentry* 
 */
struct jfs_dentry* jfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct jfs_dentry* dentry_cursor = jfs_super.root_dentry;
    struct jfs_dentry* dentry_ret = NULL;
    struct jfs_inode*  inode; 
    int   total_lvl = jfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = jfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            jfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (JFS_IS_REG(inode) && lvl < total_lvl) {
            JFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (JFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)   /* 遍历子目录项 */
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }
            
            if (!is_hit) {
                *is_find = FALSE;
                JFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }

    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = jfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}
/**
 * @brief 挂载jfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data |
 * 
 * IO_SZ = BLK_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int jfs_mount(struct custom_options options){
    int                 ret = JFS_ERROR_NONE;
    int                 driver_fd;
    struct jfs_super_d  jfs_super_d; 
    struct jfs_dentry*  root_dentry;
    struct jfs_inode*   root_inode;

    int                 inode_num;
    int                 map_inode_blks;
    
    int                 super_blks;
    boolean             is_init = FALSE;

    jfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);

    if (driver_fd < 0) {
        return driver_fd;
    }

    jfs_super.driver_fd = driver_fd;
    ddriver_ioctl(JFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &jfs_super.sz_disk);
    ddriver_ioctl(JFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &jfs_super.sz_io);
    
    root_dentry = new_dentry("/", JFS_DIR);     /* 根目录项每次挂载时新建 */

    if (jfs_driver_read(JFS_SUPER_OFS, (uint8_t *)(&jfs_super_d), 
                        sizeof(struct jfs_super_d)) != JFS_ERROR_NONE) {
        return -JFS_ERROR_IO;
    }   
                                                      /* 读取super */
    if (jfs_super_d.magic_num != JFS_MAGIC_NUM) {     /* 幻数不正确，初始化 */
                                                      /* 估算各部分大小 */
        super_blks = JFS_ROUND_UP(sizeof(struct jfs_super_d), JFS_IO_SZ()) / JFS_IO_SZ();

        inode_num  =  JFS_DISK_SZ() / ((JFS_DATA_PER_FILE + JFS_INODE_PER_FILE) * JFS_IO_SZ());

        map_inode_blks = JFS_ROUND_UP(JFS_ROUND_UP(inode_num, UINT32_BITS), JFS_IO_SZ()) 
                         / JFS_IO_SZ();
                                                      /* 布局layout */
        jfs_super.max_ino = (inode_num - super_blks - map_inode_blks); 
        jfs_super_d.map_inode_offset = JFS_SUPER_OFS + JFS_BLKS_SZ(super_blks);
        jfs_super_d.data_offset = jfs_super_d.map_inode_offset + JFS_BLKS_SZ(map_inode_blks);
        jfs_super_d.map_inode_blks  = map_inode_blks;
        jfs_super_d.sz_usage    = 0;
        JFS_DBG("inode map blocks: %d\n", map_inode_blks);
        is_init = TRUE;
    }
    jfs_super.sz_usage   = jfs_super_d.sz_usage;      /* 建立 in-memory 结构 */
    
    jfs_super.map_inode = (uint8_t *)malloc(JFS_BLKS_SZ(jfs_super_d.map_inode_blks));
    jfs_super.map_inode_blks = jfs_super_d.map_inode_blks;
    jfs_super.map_inode_offset = jfs_super_d.map_inode_offset;
    jfs_super.data_offset = jfs_super_d.data_offset;

    jfs_dump_map();

	printf("\n--------------------------------------------------------------------------------\n\n");

    if (jfs_driver_read(jfs_super_d.map_inode_offset, (uint8_t *)(jfs_super.map_inode), 
                        JFS_BLKS_SZ(jfs_super_d.map_inode_blks)) != JFS_ERROR_NONE) {
        return -JFS_ERROR_IO;
    }

    if (is_init) {                                    /* 分配根节点 */
        root_inode = jfs_alloc_inode(root_dentry);
        jfs_sync_inode(root_inode);
    }
    
    root_inode            = jfs_read_inode(root_dentry, JFS_ROOT_INO);  /* 读取根目录 */
    root_dentry->inode    = root_inode;
    jfs_super.root_dentry = root_dentry;
    jfs_super.is_mounted  = TRUE;

    jfs_dump_map();
    return ret;
}
/**
 * @brief 
 * 
 * @return int 
 */
int jfs_umount() {
    struct jfs_super_d  jfs_super_d; 

    if (!jfs_super.is_mounted) {
        return JFS_ERROR_NONE;
    }

    jfs_sync_inode(jfs_super.root_dentry->inode);     /* 从根节点向下刷写节点 */
                                                    
    jfs_super_d.magic_num           = JFS_MAGIC_NUM;
    jfs_super_d.map_inode_blks      = jfs_super.map_inode_blks;
    jfs_super_d.map_inode_offset    = jfs_super.map_inode_offset;
    jfs_super_d.data_offset         = jfs_super.data_offset;
    jfs_super_d.sz_usage            = jfs_super.sz_usage;

    if (jfs_driver_write(JFS_SUPER_OFS, (uint8_t *)&jfs_super_d, 
                     sizeof(struct jfs_super_d)) != JFS_ERROR_NONE) {
        return -JFS_ERROR_IO;
    }

    if (jfs_driver_write(jfs_super_d.map_inode_offset, (uint8_t *)(jfs_super.map_inode), 
                         JFS_BLKS_SZ(jfs_super_d.map_inode_blks)) != JFS_ERROR_NONE) {
        return -JFS_ERROR_IO;
    }

    free(jfs_super.map_inode);
    ddriver_close(JFS_DRIVER());

    return JFS_ERROR_NONE;
}
