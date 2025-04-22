#ifndef _DISK_H
#define _DISK_H

#include "list.h"

#define PORT_DISK0_DATA 0x1f0           // 数据端口0-7位
#define PORT_DISK0_ERR_FEATURE 0x1f1    // 数据端口8-15位，有错误发生时会将错误状态保存到该端口
#define PORT_DISK0_SECTOR_CNT 0x1f2     // 要读取的扇区数量
#define PORT_DISK0_SECTOR_LOW 0x1f3     // LBA扇区号0-7位
#define PORT_DISK0_SECTOR_MID 0x1f4     // LBA扇区号8-15位
#define PORT_DISK0_SECTOR_HIGH 0x1f5    // LBA扇区号16-23位
#define PORT_DISK0_DEVICE 0x1f6         // LBA扇区号23-28位，位4用于指示硬盘号，0表示主盘，1表示从盘，位6为1时表示LBA模式,剩余两位一般设置为1
#define PORT_DISK0_STATUS_CMD 0x1f7     // 硬盘状态/命令端口，写入操作命令后，该端口又表示硬盘状态

#define PORT_DISK0_ALT_STA_CTL 0x3f6    // 功能完全与0x1f7相同

// 上面是LBA28的操作，如果想要使用LBA48，那么需要将48位地址分成两个24位地址，高24位写入到0x1f3-0x1f5,然后再将第24位写入到0x1f3-0x1f5，也就是说分两次写入
// 同理，LBA48每次操作的扇区数也用16位表示，高8位先写入到0x1f2，然后再将低8位写入到0x1f2

#define PORT_DISK1_DATA 0x170
#define PORT_DISK1_ERR_FEATURE 0x171
#define PORT_DISK1_SECTOR_CNT 0x172
#define PORT_DISK1_SECTOR_LOW 0x173
#define PORT_DISK1_SECTOR_MID 0x174
#define PORT_DISK1_SECTOR_HIGH 0x175
#define PORT_DISK1_DEVICE 0x176
#define PORT_DISK1_STATUS_CMD 0x177

#define PORT_DISK1_ALT_STA_CTL 0x376    // 功能完全与0x177相同

// 硬盘的控制与状态端口中的位的含义
#define DISK_STATUS_BUSY (1 << 7)       // 硬盘忙
#define DISK_STATUS_READY (1 << 6)      // 硬盘准备就绪，等待指令
#define DISK_STATUS_SEEK (1 << 4)       // 硬盘寻道完成（磁头定位结束）
#define DISK_STATUS_REQ (1 << 3)        // 硬盘已经准备好数据，可以传输
#define DISK_STATUS_ERROR (1 << 0)      // 为1时表示硬盘发生错误

enum disk_op
{
    ATA_READ_CMD = 0x24,         // LBA28读命令:0x20，LBA48读命令：0x24
    ATA_WRITE_CMD = 0x34,        // LBA28写命令:0x30，LBA48写命令：0x34
    GET_IDENTIFY_DISK_CMD = 0xEC // 向硬盘的控制端口写入该命令会获得硬盘的识别信息
};

// 磁盘操作任务队列的结点
struct block_buffer_node
{
	unsigned int count;
	unsigned char cmd;
	uint64_t LBA;
	unsigned char * buffer;
	void(* end_handler)(uint64_t number, uint64_t parameter);

	struct List list;
};

// 磁盘操作任务队列
struct request_queue
{
    struct List queue_list;
    struct block_buffer_node *in_using;     // 正在处理的任务结点
    long block_request_count;
};

int64_t IDE_transfer(enum disk_op cmd, uint64_t start_block, int64_t count, unsigned char *buffer);
void disk_init();


#endif