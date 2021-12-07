#include "../include/Lab5.h"

int read_one_block_from_ddriver(int fd,char* buf,int blk_number){
    int blk_seek_start = 0;
    //计算指针位置
    blk_seek_start = blk_number*2;
    //移动指针位置
    ddriver_seek(fd,blk_seek_start*super.io_size,SEEK_SET);
    //读取一块数据
    ddriver_read(fd,buf,super.io_size);
    ddriver_read(fd,buf+super.io_size,super.io_size);

    return 0;
}

int write_one_block_to_ddriver(int fd,char* buf,int blk_number){
    int blk_seek_start = 0;
    char blk1[super.io_size];//方便debug
    char blk2[super.io_size];

    memcpy(blk1,buf,super.io_size);
    memcpy(blk2,buf+super.io_size,super.io_size);

    //计算指针位置
    blk_seek_start = blk_number*2;
    //移动指针位置
    ddriver_seek(fd,blk_seek_start*super.io_size,SEEK_SET);
    //写入一块数据
    ddriver_write(fd,blk1,super.io_size);
    ddriver_write(fd,blk2,super.io_size);
    return 0;
}

//逻辑格式化ddriver
int init_fs(){
    char *block_d;
    Inode root_inode;
    int inode_blk_offset;

    super_d.magic = LAB5_MAGIC;

    //布局格式:假设文件数目上限为块总数,1个超级块,1个索引位图块,1个数据位图块,索引块组,数据块组
    ddriver_ioctl(super.fd,IOC_REQ_DEVICE_SIZE,&super.device_size);
    ddriver_ioctl(super.fd,IOC_REQ_DEVICE_IO_SZ,&super.io_size);
    super_d.max_ino = super.device_size/ext2_blk_size;//4MB磁盘可以划分为4*1024个块
    super_d.map_inode_blks = 1;//1个1024x8位的索引位图块
    super_d.map_data_blks = 1;//1个1024x8位的数据位图块
    super_d.map_inode_offset = 1;//索引位图块跟在超级块后面
    super_d.map_data_offset = super_d.map_inode_offset + super_d.map_inode_blks;
    super_d.sz_usage = 0;

    //将超级块写回外存中
    block_d = (char*)malloc(ext2_blk_size);
    memset(block_d,0,ext2_blk_size);
    memcpy(block_d,&super_d,sizeof(super_d));
    write_one_block_to_ddriver(super.fd,block_d,0);
    
    //建立根目录inode并写回ddriver
    memset(block_d,0,ext2_blk_size);
    root_inode.ino = 0;
    root_inode.size = 0;
    root_inode.link = 2;
    root_inode.ftype = TYPE_DIR;
    root_inode.dir_cnt = 0;
    memset(root_inode.block_pointer,-1,sizeof(root_inode.block_pointer));
    root_inode.block_pointer[0] = 2;
    memcpy(block_d,&root_inode,sizeof(root_inode));
    inode_blk_offset = super_d.map_data_offset+super_d.map_data_blks;
    write_one_block_to_ddriver(super.fd,block_d,inode_blk_offset);

    //更新索引节点位图和数据块位图
    memset(block_d,0,ext2_blk_size);
    block_d[0] = 0x1;
    write_one_block_to_ddriver(super.fd,block_d,super_d.map_inode_offset);
    
    memset(block_d,0,ext2_blk_size);
    block_d[0] = 0b00000100;
    write_one_block_to_ddriver(super.fd,block_d,super_d.map_data_offset);


    free(block_d);
    return 0;
}

//解析磁盘中超级块的内容,并填写到超级快的内存表示上
int super_read(char* super_block_d){
    memcpy((char*)(&super),super_block_d,sizeof(struct super_block_d));
    super.block_inode_offset = super.map_data_offset+super.map_data_blks;
    super.block_data_offset = super.block_inode_offset + super.map_inode_blks*1024*8/16;
    return 0;
}

int Lab5_mount(struct custom_options option){
    char buf[1025];
    //打开设备,获得其文件描述符
    super.fd = ddriver_open(option.device);
    if(super.fd<0){
        return super.fd;
    }
    ////获取ddriver基本属性
    ddriver_ioctl(super.fd,IOC_REQ_DEVICE_SIZE,&super.device_size);
    ddriver_ioctl(super.fd,IOC_REQ_DEVICE_IO_SZ,&super.io_size);

    //读取魔数,判断是否是Ext2文件系统
    memset(buf,0,sizeof(buf));
    read_one_block_from_ddriver(super.fd,buf,0);    
    memcpy(&super.magic,buf,4*sizeof(char));

    //Lab5文件系统未建立,逻辑格式化ddriver
    if(super.magic != LAB5_MAGIC){
        init_fs();
        read_one_block_from_ddriver(super.fd,buf,0);    
    }
    //文件系统已建立,读取超级块
    //init_fs();
    //read_one_block_from_ddriver(super.fd,buf,0);    
    super_read(buf);

    //建立根目录项
    super.root = (Dentry*)malloc(sizeof(Dentry));
    sprintf(super.root->name,"/");
    super.root->type = TYPE_DIR;
    super.root->valid = 1;
    super.root->ino = 0;
    return 0;
}

//计算路径深度
int path_total_level(char* path){
    char* str = path;
    int level = 0;
    if(!path){
        return 0;
    }
    if(strcmp(path,"/") == 0){
        return level;
    }
    while(*str != NULL){
        if(*str == '/'){
            level++;
        }
        str ++;
    }
    return level;
}

//将dentry指向的inode项的信息从磁盘中读取出来并挂载在dentry的inode指针上,同时将该目录项的子节点全部读取出来
int read_inode(Dentry* dentry, Inode* inode){
    char buf[1025];
    //计算inode所在块
    int inode_block = dentry->ino/16 + super.block_inode_offset;
    int offset = dentry->ino%16;
    int block_count;
    Dentry* dentry_cur = dentry;
    Dentry* dentry_temp; 
    read_one_block_from_ddriver(super.fd,buf,inode_block);
    inode = (Inode*)malloc(sizeof(Inode));
    memcpy(inode,buf+offset*sizeof(Inode),sizeof(Inode));
    //将对应的inode节点挂载在dentry上
    dentry->inode = inode;

    /*如果是目录,读取目录下的所有项,建立连接*/
    //没有目录项.直接返回
    if(inode->dir_cnt == 0){
        return 0;
    }
    //计算目录项有多少块
    block_count = (inode->dir_cnt-1)/3;
    //读取目录项所在的数据块
    for(int i =0; i<=block_count;i++){
        memset(buf,0,sizeof(buf));
        read_one_block_from_ddriver(super.fd,buf,inode->block_pointer[i]+super.block_data_offset);
        if(i!=block_count){
            for(int j=0;j<3;j++){
                dentry_temp = (Dentry*)malloc(sizeof(Dentry)); 
                memcpy(dentry_temp,buf+j*sizeof(Dentry_d),sizeof(Dentry_d));
                if(i == 0 && j == 0){
                    dentry_temp->parent = dentry;
                    dentry_temp->child = NULL;
                    dentry_temp->brother = NULL;
                    dentry_temp->inode = NULL;
                    dentry_cur->child = dentry_temp;
                    dentry_cur = dentry_cur->child;
                }
                else{
                    dentry_temp->parent = dentry;
                    dentry_temp->child = NULL;
                    dentry_temp->brother = NULL;
                    dentry_temp->inode = NULL;
                    dentry_cur->brother = dentry_temp;
                    dentry_cur = dentry_cur->brother;
                }
            }
        }
        //读出剩余的目录项
        else{
            for(int j=0;j <= (inode->dir_cnt-1) % 3;j++){
                dentry_temp = (Dentry*)malloc(sizeof(Dentry)); 
                memcpy(dentry_temp,buf+j*sizeof(Dentry_d),sizeof(Dentry_d));
                if(i == 0 && j == 0){
                    dentry_temp->parent = dentry;
                    dentry_temp->child = NULL;
                    dentry_temp->brother = NULL;
                    dentry_temp->inode = NULL;
                    dentry_cur->child = dentry_temp;
                    dentry_cur = dentry_cur->child;
                }
                else{
                    dentry_temp->parent = dentry;
                    dentry_temp->child = NULL;
                    dentry_temp->brother = NULL;
                    dentry_temp->inode = NULL;
                    dentry_cur->brother = dentry_temp;
                    dentry_cur = dentry_cur->brother;
                }
            }
        }
    }
    return 0;
}

//根据路径查找一个目录/文件项并以指针的形式返回
Dentry* dentry_lookup(char* path,int* is_find,int* is_root){
    Dentry* dentry_cursor = super.root;
    dentry_ret = super.root;
    int   total_lvl = path_total_level(path);
    int   lvl = 0;
    int   error = 0;
    char* fname = NULL;
    char  path_cpy[MAX_NAME_LEN];
    *is_root = 0;
    *is_find = 0;
    strcpy(path_cpy, path);

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = 1;
        *is_root = 1;
        if (dentry_ret->inode == NULL) {
            read_inode(dentry_cursor, dentry_cursor->inode); 
        }
        return dentry_ret;
    }

    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            read_inode(dentry_cursor, dentry_cursor->inode);
        }

        //遍历当前目录的子项
        dentry_cursor = dentry_cursor->child;

        while(dentry_cursor){
            //找到符合名字的项
            if(strcmp(dentry_cursor->name,fname) == 0){
                if(dentry_cursor->type == TYPE_FILE){
                    //该文件夹其实是一个文件,报错
                    if(lvl<total_lvl){
                        printf("%s is not a valid path,%s is a file.\n",path,fname);
                        error = 1;
                    }
                    //找到了要找的文件
                    else if(lvl == total_lvl){
                        //把该文件的inode信息挂在dentry的内存表示上
                        *is_find = 1;
                    }
                }
                else if(dentry_cursor->type == TYPE_DIR){
                    //找到了文件夹,但是路径还没结束,继续找
                    if(lvl<total_lvl){
                        break;
                    }
                    //找到了要找的文件夹.路径已经结束
                    else if(lvl == total_lvl){
                        *is_find = 1;
                    }
                }
            }
            if(!*is_find){
                //没找到,指针指向下一个节点
                dentry_cursor = dentry_cursor->brother;
            }
            else{
                break;
            }
        }

        //没找到要找的文件夹/文件,报错
        if(dentry_cursor == NULL){
            printf("%s is not a valid path,%s is not existed.\n",path,fname);
            error = 1;
        }
        
        if(error == 1 || *is_find == 1){
            break;
        }

        fname = strtok(NULL, "/"); 
    }

    //把该文件夹的inode信息挂在dentry的内存表示上
    if (*is_find && dentry_cursor->inode == NULL) {
        read_inode(dentry_cursor, dentry_cursor->inode); 
    }
    dentry_ret = dentry_cursor;

    return dentry_ret;
}

//分配一个空闲的数据块,返回被分配的数据块编号,并更新位图
int data_block_allocate(){
    char buf[1025];
    int data_block_found = 0;
    int block_no;
    read_one_block_from_ddriver(super.fd,buf,super.map_data_offset);
	for(int j = 0;j<ext2_blk_size&&!data_block_found;j++){
		//一个字节有8位
		for(int i =0;i<8&&!data_block_found;i++){
			if((buf[j] & (1<<i)) == 0){
				data_block_found = 1;
				block_no = j*8+i;
                buf[j] = buf[j] | (1<<i);
			}
		}
	}
	write_one_block_to_ddriver(super.fd,buf,super.map_data_offset);
    return block_no;
}

//分配一个空闲的inode节点,返回被分配的inode节点编号,并更新位图
int inode_allocate(){
    char buf[1025];
    int inode_found = 0;
    int inode_no;
    read_one_block_from_ddriver(super.fd,buf,super.map_inode_offset);
	for(int j = 0;j<ext2_blk_size&&!inode_found;j++){
		//一个字节有8位
		for(int i =0;i<8&&!inode_found;i++){
			if((buf[j] & (1<<i)) == 0){
				inode_no = j*8+i;
				buf[j] = buf[j] | 1<<i;
                inode_found = 1;
			}
		}
	}
	write_one_block_to_ddriver(super.fd,buf,super.map_inode_offset);
    return inode_no;
}

//将一个inode索引节点写回磁盘上
int write_inode_to_ddriver(Inode inode){
    char buf[1025];
    int block_in = inode.ino/16;
    int offset_in_block = inode.ino%16; 
    read_one_block_from_ddriver(super.fd,buf,super.block_inode_offset+block_in);
    memcpy(buf+offset_in_block*sizeof(Inode),&inode,sizeof(Inode));
    write_one_block_to_ddriver(super.fd,buf,super.block_inode_offset+block_in);
    return 0;
}