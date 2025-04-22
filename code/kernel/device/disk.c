#include "../include/disk.h"
#include "../include/stdint.h"
#include "../include/list.h"
#include "../include/io.h"
#include "../include/memory.h"
#include "../include/print_kernel.h"
#include "../include/interrupt.h"

static volatile int disk_flags = 0;

struct request_queue disk_request;

extern intr_handler idt_func_table[256];

int64_t cmd_out()
{
    struct block_buffer_node *node = disk_request.in_using = container_of(list_next(&disk_request.queue_list), struct block_buffer_node, list);
    list_delete(&disk_request.in_using->list);
    disk_request.block_request_count--;

    while (inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_BUSY)
        nop();

    switch (node->cmd)
    {
        case ATA_WRITE_CMD:

            outb(PORT_DISK0_DEVICE, 0x40);

            outb(PORT_DISK0_ERR_FEATURE, 0);
            outb(PORT_DISK0_SECTOR_CNT, (node->count >> 8) & 0xff);
            outb(PORT_DISK0_SECTOR_LOW, (node->LBA >> 24) & 0xff);
            outb(PORT_DISK0_SECTOR_MID, (node->LBA >> 32) & 0xff);
            outb(PORT_DISK0_SECTOR_HIGH, (node->LBA >> 40) & 0xff);

            outb(PORT_DISK0_ERR_FEATURE, 0);
            outb(PORT_DISK0_SECTOR_CNT, node->count & 0xff);
            outb(PORT_DISK0_SECTOR_LOW, node->LBA & 0xff);
            outb(PORT_DISK0_SECTOR_MID, (node->LBA >> 8) & 0xff);
            outb(PORT_DISK0_SECTOR_HIGH, (node->LBA >> 16) & 0xff);

            while (!(inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_READY))
                nop();
            outb(PORT_DISK0_STATUS_CMD, node->cmd);

            while (!(inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_REQ))
                nop();
            outsw(PORT_DISK0_DATA, node->buffer, 256);
            break;

        case ATA_READ_CMD:

            outb(PORT_DISK0_DEVICE, 0x40);

            outb(PORT_DISK0_ERR_FEATURE, 0);
            outb(PORT_DISK0_SECTOR_CNT, (node->count >> 8) & 0xff);
            outb(PORT_DISK0_SECTOR_LOW, (node->LBA >> 24) & 0xff);
            outb(PORT_DISK0_SECTOR_MID, (node->LBA >> 32) & 0xff);
            outb(PORT_DISK0_SECTOR_HIGH, (node->LBA >> 40) & 0xff);


            outb(PORT_DISK0_ERR_FEATURE, 0);
            outb(PORT_DISK0_SECTOR_CNT, node->count & 0xff);
            outb(PORT_DISK0_SECTOR_LOW, node->LBA & 0xff);
            outb(PORT_DISK0_SECTOR_MID, (node->LBA >> 8) & 0xff);
            outb(PORT_DISK0_SECTOR_HIGH, (node->LBA >> 16) & 0xff);

            while (!(inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_READY))
                nop();
            outb(PORT_DISK0_STATUS_CMD, node->cmd);
            break;

        case GET_IDENTIFY_DISK_CMD:

            outb(PORT_DISK0_DEVICE, 0xe0);

            outb(PORT_DISK0_ERR_FEATURE, 0);
            outb(PORT_DISK0_SECTOR_CNT, node->count & 0xff);
            outb(PORT_DISK0_SECTOR_LOW, node->LBA & 0xff);
            outb(PORT_DISK0_SECTOR_MID, (node->LBA >> 8) & 0xff);
            outb(PORT_DISK0_SECTOR_HIGH, (node->LBA >> 16) & 0xff);

            while (!(inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_READY))
                nop();
            outb(PORT_DISK0_STATUS_CMD, node->cmd);
            break;

        default:
            printf("ATA CMD Error\n");
            break;
        }
    return 1;
}

void end_request()
{
    kfree((uint64_t *)disk_request.in_using);
    disk_request.in_using = NULL;

    disk_flags = 0;

    if (disk_request.block_request_count)
    {
        cmd_out();
    }
}

void read_handler(uint64_t number, uint64_t parameter)
{
    printf("read handler start\n");
    struct block_buffer_node *node = ((struct request_queue *)parameter)->in_using;

    if (inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_ERROR)
    {
        printf("read_handler:%x\n", inb(PORT_DISK0_ERR_FEATURE));
    }
    else
    {
        printf("insw(PORT_DISK0_DATA, node->buffer, 256);\n");
        insw(PORT_DISK0_DATA, node->buffer, 256);
    }
    printf("end_request();\n");
    end_request();
}

void write_handler(uint64_t number, uint64_t parameter)
{
    if (inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_ERROR)
    {
        printf("write_handler:%x\n", inb(PORT_DISK0_ERR_FEATURE));
    }
    end_request();
}

void other_handler(uint64_t number, uint64_t parameter)
{
    struct block_buffer_node *node = ((struct request_queue *)parameter)->in_using;

    if (inb(PORT_DISK0_STATUS_CMD) & DISK_STATUS_ERROR)
    {
        printf("other_handler:%x\n", inb(PORT_DISK0_ERR_FEATURE));
    }
    else
    {
        insw(PORT_DISK0_DATA, node->buffer, 256);
    }
    end_request();
}


// 添加任务队列
void add_request(struct block_buffer_node *node)
{
    list_insert_before(&disk_request.queue_list, &node->list);
    disk_request.block_request_count++;
}

// 递送任务结点
void submit(struct block_buffer_node *node)
{
    add_request(node);

    // 如果正在处理的任务结点为空，那么说明可以立即处理当前添加进来的任务结点
    if (disk_request.in_using == NULL)
    {
        cmd_out();
    }
}

// 生成一个磁盘操作任务队列
struct block_buffer_node *make_request(enum disk_op cmd, uint64_t start_block, int64_t count, unsigned char *buffer)
{
    struct block_buffer_node *node = (struct block_buffer_node *)kmalloc(sizeof(struct block_buffer_node), 0);
    list_init(&node->list);

    switch (cmd)
    {
        case ATA_READ_CMD:
            node->end_handler = read_handler;
            node->cmd = ATA_READ_CMD;
            break;

        case ATA_WRITE_CMD:
            node->end_handler = write_handler;
            node->cmd = ATA_WRITE_CMD;
            break;

        default:
            node->end_handler = other_handler;
            node->cmd = cmd;
            break;
    }

    node->LBA = start_block;
    node->count = count;
    node->buffer = buffer;
    return node;
}

void wait_for_finish()
{
    disk_flags = 1;
    while (disk_flags)
    {
        nop();
    }
}

// 磁盘调动
int64_t IDE_transfer(enum disk_op cmd, uint64_t start_block, int64_t count, unsigned char *buffer)
{
    struct block_buffer_node *node = NULL;
    if (cmd == ATA_READ_CMD || cmd == ATA_WRITE_CMD)
    {
        node = make_request(cmd, start_block, count, buffer);
        submit(node);
        wait_for_finish();
    }
    else
    {
        return 0;
    }

    return 1;
}

// 这个中断函数是存在bug的，原因是我们没有传递parameter参数，却使用到了该参数，其它函数中也是如此
void disk_handler(uint64_t number, uint64_t parameter, struct pt_regs *regs)
{
    printf("interrupt occur:disk handler\n");
    struct block_buffer_node *node = ((struct request_queue *)parameter)->in_using;
    node->end_handler(number, parameter);

    // 发送中断结束命令
    outb(0xa0, 0x20);
    outb(0x20, 0x20);
}

void disk_init()
{
    // 允许磁盘在操作完成（如数据传输结束或错误发生）时触发中断。
    outb(PORT_DISK0_ALT_STA_CTL, 0x00);

    list_init(&disk_request.queue_list);
    disk_request.in_using = NULL;
    disk_request.block_request_count = 0;
    disk_flags = 0;

    idt_func_table[46] = disk_handler;

    printf("disk init done\n");
}