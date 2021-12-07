#ifndef _LAB5_H_
#define _LAB5_H_

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

#define LAB5_MAGIC  0x90111010                /* TODO: Define by yourself */
#define LAB5_DEFAULT_PERM    0777   /* 全权限打开 */

#define TYPE_FILE 0
#define TYPE_DIR  1

struct custom_options Lab5_options;			 /* 全局选项 */
struct Lab5_super super; 					//超级块结构体定义
struct super_block_d super_d;				//超级块在外存上的表示

static int ext2_blk_size = 1024;					//ext2文件系统一块的字节数
Dentry* dentry_ret;

/******************************************************************************
* SECTION: Lab5_myfunction.c
*******************************************************************************/
int Lab5_mount(struct custom_options option);

/******************************************************************************
* SECTION: Lab5.c
*******************************************************************************/
void* 			   Lab5_init(struct fuse_conn_info *);
void  			   Lab5_destroy(void *);
int   			   Lab5_mkdir(const char *, mode_t);
int   			   Lab5_getattr(const char *, struct stat *);
int   			   Lab5_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   Lab5_mknod(const char *, mode_t, dev_t);
int   			   Lab5_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   Lab5_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   Lab5_access(const char *, int);
int   			   Lab5_unlink(const char *);
int   			   Lab5_rmdir(const char *);
int   			   Lab5_rename(const char *, const char *);
int   			   Lab5_utimens(const char *, const struct timespec tv[2]);
int   			   Lab5_truncate(const char *, off_t);
			
int   			   Lab5_open(const char *, struct fuse_file_info *);
int   			   Lab5_opendir(const char *, struct fuse_file_info *);

#endif  /* _Lab5_H_ */