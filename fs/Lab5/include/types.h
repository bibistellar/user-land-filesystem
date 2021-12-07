#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128     

struct custom_options {
	char*        device;
};

//一个索引节点的大小是64B,一个索引块可以装下16个索引节点
typedef struct Lab5_inode {
    uint32_t ino;
    /* TODO: Define yourself */
    uint32_t size;
    uint32_t link;
    uint32_t ftype;
    uint32_t dir_cnt;
    uint32_t block_pointer[6];
    uint32_t temp[5];//保留空间
}Inode;

typedef struct Lab5_dentry {
    char     name[MAX_NAME_LEN];
    int ino;
    int type;
    int valid;
    /* TODO: Define yourself */
    Inode*   inode;//这个目录项指向的inode节点
    struct Lab5_dentry*  parent;
    struct Lab5_dentry*  brother;
    struct Lab5_dentry*  child;
}Dentry;

//140字节一个,一个数据块最多可以含有三个目录项
typedef struct Lab5_dentry_d{
    char     name[MAX_NAME_LEN];
    int ino;
    int type;
    int valid;
}Dentry_d;

struct super_block_d
{
    uint32_t           magic;                   // 幻数

    int                max_ino;                     // 最多支持的文件数

    int                map_inode_blks;              // inode位图占用的块数
    int                map_inode_offset;            // inode位图在磁盘上的偏移

    int                map_data_blks;               // data位图占用的块数
    int                map_data_offset;             // data位图在磁盘上的偏移
    int                sz_usage;                    //当前磁盘已经使用的大小
};

struct Lab5_super {
    uint32_t           magic;
    int                max_ino;                     // 最多支持的文件数

    int                map_inode_blks;              // inode位图占用的块数
    int                map_inode_offset;            // inode位图在磁盘上的偏移

    int                map_data_blks;               // data位图占用的块数
    int                map_data_offset;             // data位图在磁盘上的偏移 
    int                sz_usage;                    //当前磁盘已经使用的大小 
    /* TODO: Define yourself */

    int                fd;
    int                device_size;
    int                io_size;//一次io操作的字节数 512
    int                block_inode_offset;//存储inode的块的起始偏移
    int                block_data_offset;//data数据块的起始偏移
    Dentry*            root;//根目录项
};


#endif /* _TYPES_H_ */