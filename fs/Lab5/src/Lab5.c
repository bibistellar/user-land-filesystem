#include "../include/Lab5.h"

/******************************************************************************
* SECTION: 宏定义
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }

/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
	OPTION("--device=%s", device),
	FUSE_OPT_END
};


/******************************************************************************
* SECTION: FUSE操作定义
*******************************************************************************/
static struct fuse_operations operations = {
	.init = Lab5_init,						 /* mount文件系统 */		
	.destroy = Lab5_destroy,				 /* umount文件系统 */
	.mkdir = Lab5_mkdir,					 /* 建目录，mkdir */
	.getattr = Lab5_getattr,				 /* 获取文件属性，类似stat，必须完成 */
	.readdir = Lab5_readdir,				 /* 填充dentrys */
	.mknod = Lab5_mknod,					 /* 创建文件，touch相关 */
	.write = NULL,								  	 /* 写入文件 */
	.read = NULL,								  	 /* 读文件 */
	.utimens = Lab5_utimens,				 /* 修改时间，忽略，避免touch报错 */
	.truncate = NULL,						  		 /* 改变文件大小 */
	.unlink = NULL,							  		 /* 删除文件 */
	.rmdir	= NULL,							  		 /* 删除目录， rm -r */
	.rename = NULL,							  		 /* 重命名，mv */

	.open = NULL,							
	.opendir = NULL,
	.access = NULL
};
/******************************************************************************
* SECTION: 必做函数实现
*******************************************************************************/
/**
 * @brief 挂载（mount）文件系统
 * 
 * @param conn_info 可忽略，一些建立连接相关的信息 
 * @return void*
 */
void* Lab5_init(struct fuse_conn_info * conn_info) {
	/* TODO: 在这里进行挂载 */
	Lab5_mount(Lab5_options);
	if (super.fd<0) {
		printf("mount error!\n");
		return NULL;
	} 
	
	return NULL;
}

/**
 * @brief 卸载（umount）文件系统
 * 
 * @param p 可忽略
 * @return void
 */
void Lab5_destroy(void* p) {
	/* TODO: 在这里进行卸载 */
	//写回超级块
	char buf[1025];
	memset(buf,0,sizeof(buf));
	memcpy(buf,&super,sizeof(super_d));
	write_one_block_to_ddriver(super.fd,buf,0);
	ddriver_close(super.fd);
	return;
}

/**
 * @brief 创建目录
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建模式（只读？只写？），可忽略
 * @return int 0成功，否则失败
 */
int Lab5_mkdir(const char* path, mode_t mode) {
	/* TODO: 解析路径，创建目录 */
	char buf[1025];
	int is_find,is_root;
	int new_dir_name_start = 0;
	char new_dir_name[MAX_NAME_LEN];
	char dir_path[MAX_NAME_LEN];
	Dentry* dir_dentry;
	Inode new_inode;
	Dentry_d new_dentry_d;
	int data_block_found = 0;
	int dir_count;
	int data_block_no;
	int i;
	//解析路径,将path分为上级路径和要创建目录名
	for(i = 0;path[i];i++){
		if(path[i] == '/'){
			new_dir_name_start = i+ 1;
		}
	}
	memset(new_dir_name,0,sizeof(new_dir_name));
	memset(dir_path,0,sizeof(dir_path));
	memcpy(new_dir_name,path+new_dir_name_start,i-new_dir_name_start);
	memcpy(dir_path,path,new_dir_name_start);
	if(new_dir_name_start!=1){
		dir_path[new_dir_name_start-1] = '\0';
	}
	dir_dentry = dentry_lookup(dir_path,&is_find,&is_root);
	dir_dentry = dentry_ret;
	
	//创建目录,在目录下新建一个目录项,该目录项指向一个新建inode项,并更新inode位图
	//并给该inode分配一个初始数据块

	//分配一个未使用的inode节点
	new_inode.ino = inode_allocate();
	new_inode.ftype = TYPE_DIR;
	new_inode.link = 1;
	new_inode.dir_cnt = 0;
	new_inode.size = 0;

	//为该inode分配一个未使用data块
	memset(new_inode.block_pointer,-1,sizeof(new_inode.block_pointer));
	new_inode.block_pointer[0] = data_block_allocate();

	//在上级目录的数据块中新建一个dentry,并将其指向new_inode
	dir_count = dir_dentry->inode->dir_cnt++;//上级目录下的第几个目录项
	dir_dentry->inode->size +=sizeof(Dentry_d);
	//判断上级目录存储数据的数据是否已满
	if(dir_count%3 == 0 && dir_dentry->inode->block_pointer[dir_count/3] == -1){
		//需要新分配一个数据块
		dir_dentry->inode->block_pointer[dir_count/3] = data_block_allocate();
	}
	//同步对上级目录inode的更改
	write_inode_to_ddriver(*dir_dentry->inode);
	//获取存储当前目录的目录项的数据块
	data_block_no = dir_dentry->inode->block_pointer[dir_count/3];
	read_one_block_from_ddriver(super.fd,buf,super.block_data_offset+data_block_no);//读取目录项所在的数据块
	
	new_dentry_d.ino = new_inode.ino;
	memcpy(new_dentry_d.name,new_dir_name,sizeof(new_dir_name));
	new_dentry_d.type = TYPE_DIR;
	new_dentry_d.valid = 1;

	//将更新后的数据块写回磁盘
	memcpy(buf+(dir_count%3)*sizeof(new_dentry_d),&new_dentry_d,sizeof(new_dentry_d));
	write_one_block_to_ddriver(super.fd,buf,super.block_data_offset+data_block_no);

	//将更新后当前目录的新inode节点写回磁盘中
	write_inode_to_ddriver(new_inode);
	//清除上级目录缓存信息
	dir_dentry->inode = NULL;
	return 0;
}

/**
 * @brief 获取文件或目录的属性，该函数非常重要
 * 
 * @param path 相对于挂载点的路径
 * @param Lab5_stat 返回状态
 * @return int 0成功，否则失败
 */
int Lab5_getattr(const char* path, struct stat * Lab5_stat) {
	/* TODO: 解析路径，获取Inode，填充Lab5_stat，可参考/fs/simplefs/sfs.c的sfs_getattr()函数实现 */
	int is_root = 0;
	int is_find = 0;
	char path_buf[1025];
	memset(path_buf,0,sizeof(path_buf));

	memcpy(path_buf,path,sizeof(path_buf));
	Dentry* dentry;

	//为什么这个地方这么奇怪呢?因为dentry_lookup函数在允许fuse多线程的情况下,函数内部返回值与函数外部返回值不一致.....
	//我不知道要怎么解决这个问题,我只能猜测是多线程的问题,所以我只好把返回值声明为全局变量,执行函数之后直接使用这个全局变量,这样起码不会出错
	dentry = dentry_lookup(path_buf, &is_find, &is_root);
	dentry = dentry_ret;

	if (is_find == 0) {
		printf("path error!\n");
		return -2;
	}

	if (dentry->type == TYPE_DIR) {
		Lab5_stat->st_mode = S_IFDIR | LAB5_DEFAULT_PERM;
		Lab5_stat->st_size = dentry->inode->dir_cnt * sizeof(Dentry_d);
	}
	else if (dentry->type == TYPE_FILE) {
		Lab5_stat->st_mode = S_IFREG | LAB5_DEFAULT_PERM;
		Lab5_stat->st_size = dentry->inode->size;
	}

	Lab5_stat->st_nlink = 1;
	Lab5_stat->st_uid 	 = getuid();
	Lab5_stat->st_gid 	 = getgid();
	Lab5_stat->st_atime   = time(NULL);
	Lab5_stat->st_mtime   = time(NULL);
	Lab5_stat->st_blksize = super.io_size;
	if (is_root) {
		Lab5_stat->st_size	= super.sz_usage; 
		Lab5_stat->st_blocks = super.device_size / super.io_size;
		Lab5_stat->st_nlink  = 2;		/* !特殊，根目录link数为2 */
	}
	return 0;
}

/**
 * @brief 遍历目录项，填充至buf，并交给FUSE输出
 * 
 * @param path 相对于挂载点的路径
 * @param buf 输出buffer
 * @param filler 参数讲解:
 * 
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry(从0开始?)
 * 
 * @param offset 第几个目录项？
 * @param fi 可忽略
 * @return int 0成功，否则失败
 */
int Lab5_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
			    		 struct fuse_file_info * fi) {
    /* TODO: 解析路径，获取目录的Inode，并读取目录项，利用filler填充到buf，可参考/fs/simplefs/sfs.c的sfs_readdir()函数实现 */
	int	is_find, is_root;
	int cur_dir = offset;

	Dentry* dentry = dentry_lookup(path, &is_find, &is_root);
	dentry = dentry_ret;
	Dentry* sub_dentry = dentry->child;
	Inode* inode;
	if (is_find) {
		inode = dentry->inode;
		//遍历子目录
		for(int i = 0;sub_dentry&&i<cur_dir;i++){
			sub_dentry = sub_dentry->brother;
		}
		if (sub_dentry) {
			filler(buf, sub_dentry->name, NULL, ++offset);
		}
	}
    return 0;
}

/**
 * @brief 创建文件
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建文件的模式，可忽略
 * @param dev 设备类型，可忽略
 * @return int 0成功，否则失败
 */
int Lab5_mknod(const char* path, mode_t mode, dev_t dev) {
	/* TODO: 解析路径，并创建相应的文件 */
	char buf[1025];
	int is_find,is_root;
	int file_name_start = 0;
	char file_name[MAX_NAME_LEN];
	char dir_path[MAX_NAME_LEN];
	Dentry* dir_dentry;
	Inode new_inode;
	Dentry_d new_dentry_d;
	int data_block_found = 0;
	int dir_count;
	int data_block_no;
	int i;
	//解析路径,将path分为目录和文件名
	for(i = 0;path[i];i++){
		if(path[i] == '/'){
			file_name_start = i+ 1;
		}
	}
	memset(file_name,0,sizeof(file_name));
	memset(dir_path,0,sizeof(dir_path));
	memcpy(file_name,path+file_name_start,i-file_name_start);
	memcpy(dir_path,path,file_name_start);
	//分割完之后如果不是根目录,那么得到的文件夹路径是/xxx/xxx/,但要改成/xxx/xxx的形式才能被dentry_lookup识别
	if(file_name_start!=1){
		dir_path[file_name_start-1] = '\0';
	}
	dir_dentry = dentry_lookup(dir_path,&is_find,&is_root);
	dir_dentry = dentry_ret;
	
	//创建文件,在目录下新建一个目录项,该目录项指向一个新建inode项,并更新inode位图
	//并给该inode分配一个初始数据块

	//分配一个未使用的inode节点
	new_inode.ino = inode_allocate();
	new_inode.ftype = TYPE_FILE;
	new_inode.link = 1;
	new_inode.dir_cnt = 0;
	new_inode.size = 0;

	//为该inode分配一个未使用data块
	memset(new_inode.block_pointer,-1,sizeof(new_inode.block_pointer));
	new_inode.block_pointer[0] = data_block_allocate();

	//在上级目录的数据块中新建一个dentry,并将其指向new_inode
	dir_count = dir_dentry->inode->dir_cnt++;//当前目录下的第几个目录项
	dir_dentry->inode->size +=sizeof(Dentry_d);
	if(dir_count%3 == 0 && dir_dentry->inode->block_pointer[dir_count/3] == -1){
		//需要新分配一个数据块
		dir_dentry->inode->block_pointer[dir_count/3] = data_block_allocate();
	}
	write_inode_to_ddriver(*dir_dentry->inode);
	data_block_no = dir_dentry->inode->block_pointer[dir_count/3];
	read_one_block_from_ddriver(super.fd,buf,super.block_data_offset+data_block_no);//读取目录项所在的数据块
	new_dentry_d.ino = new_inode.ino;
	memcpy(new_dentry_d.name,file_name,sizeof(file_name));
	new_dentry_d.type = TYPE_FILE;
	new_dentry_d.valid = 1;
	memcpy(buf+(dir_count%3)*sizeof(new_dentry_d),&new_dentry_d,sizeof(new_dentry_d));
	write_one_block_to_ddriver(super.fd,buf,super.block_data_offset+data_block_no);//将更新后的dentry数据块写回磁盘

	//将更新后当前文件的新inode节点写回磁盘中
	write_inode_to_ddriver(new_inode);
	//清除上级目录缓存信息
	free(dir_dentry->inode);
	dir_dentry->inode = NULL;

	
	return 0;
}

/**
 * @brief 修改时间，为了不让touch报错 
 * 
 * @param path 相对于挂载点的路径
 * @param tv 实践
 * @return int 0成功，否则失败
 */
int Lab5_utimens(const char* path, const struct timespec tv[2]) {
	(void)path;
	return 0;
}
/******************************************************************************
* SECTION: 选做函数实现
*******************************************************************************/
/**
 * @brief 写入文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 写入的内容
 * @param size 写入的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 写入大小
 */
int Lab5_write(const char* path, const char* buf, size_t size, off_t offset,
		        struct fuse_file_info* fi) {
	/* 选做 */
	return size;
}

/**
 * @brief 读取文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 读取的内容
 * @param size 读取的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 读取大小
 */
int Lab5_read(const char* path, char* buf, size_t size, off_t offset,
		       struct fuse_file_info* fi) {
	/* 选做 */
	return size;			   
}

/**
 * @brief 删除文件
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则失败
 */
int Lab5_unlink(const char* path) {
	/* 选做 */
	return 0;
}

/**
 * @brief 删除目录
 * 
 * 一个可能的删除目录操作如下：
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * 即，先删除最深层的文件，再删除目录文件本身
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则失败
 */
int Lab5_rmdir(const char* path) {
	/* 选做 */
	return 0;
}

/**
 * @brief 重命名文件 
 * 
 * @param from 源文件路径
 * @param to 目标文件路径
 * @return int 0成功，否则失败
 */
int Lab5_rename(const char* from, const char* to) {
	/* 选做 */
	return 0;
}

/**
 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
 * 保存在fh中
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则失败
 */
int Lab5_open(const char* path, struct fuse_file_info* fi) {
	/* 选做 */
	return 0;
}

/**
 * @brief 打开目录文件
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则失败
 */
int Lab5_opendir(const char* path, struct fuse_file_info* fi) {
	/* 选做 */
	return 0;
}

/**
 * @brief 改变文件大小
 * 
 * @param path 相对于挂载点的路径
 * @param offset 改变后文件大小
 * @return int 0成功，否则失败
 */
int Lab5_truncate(const char* path, off_t offset) {
	/* 选做 */
	return 0;
}


/**
 * @brief 访问文件，因为读写文件时需要查看权限
 * 
 * @param path 相对于挂载点的路径
 * @param type 访问类别
 * R_OK: Test for read permission. 
 * W_OK: Test for write permission.
 * X_OK: Test for execute permission.
 * F_OK: Test for existence. 
 * 
 * @return int 0成功，否则失败
 */
int Lab5_access(const char* path, int type) {
	/* 选做: 解析路径，判断是否存在 */
	return 0;
}	
/******************************************************************************
* SECTION: FUSE入口
*******************************************************************************/
int main(int argc, char **argv)
{
    int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	Lab5_options.device = strdup("/home/stellaris/ddriver");

	if (fuse_opt_parse(&args, &Lab5_options, option_spec, NULL) == -1)
		return -1;
	
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}