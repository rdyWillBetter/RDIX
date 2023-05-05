#include <rdix/part.h>
#include <rdix/device.h>
#include <rdix/kernel.h>
#include <common/string.h>
#include <common/assert.h>

#define PART_LOG_INFO __LOG("[part init]")
#define PART_WARNNING_INFO __WARNING("[part init]")

static int __part_read(part_t *part, void *buf, size_t count, idx_t idx){
    return device_read(part->disk->dev, buf, count, idx + part->start, 0);
}

static int __part_write(part_t *part, void *buf, size_t count, idx_t idx){
    return device_write(part->disk->dev, buf, count, idx + part->start, 0);
}

static int __part_ioctl(part_t *part, int cmd, void *args, int flags){
    switch (cmd){
        case DEV_CMD_SECTOR_START: return part->start;
        default:
            printk(PART_WARNNING_INFO "device command %d not implament\n", cmd);
            break;
    }
    return 0;
}

void disk_part_install(dev_t disk_idx){
    device_t *disk = device_get(disk_idx);

    assert(disk != NULL);

    void *buf = alloc_kpage(1);
    device_read(disk_idx, buf, 1, 0, 0);

    boot_sector_t *boot = (boot_sector_t *)buf;

    for (size_t i = 0; i < MBR_PART_NR; ++i){
        part_entry_t *entry = &boot->entry[i];  // 磁盘中的分区数据
        part_t *part = (part_t *)malloc(sizeof(part_t));    //内存中的分区数据

        if (!entry->count)
            continue;
        
        /* 请确保不会溢出 */
        assert(sprintf(part->name, "%s%d", disk->name, i + 1) < PART_NAME_LEN);

        part->disk = disk;
        part->count = entry->count;
        part->system = entry->system;
        part->start = entry->start;

        printk(PART_LOG_INFO "part %s\n", part->name);
        printk(PART_LOG_INFO "\tbootable 0x%x\n", entry->bootable);
        printk(PART_LOG_INFO "\tstart %d\n", part->start);
        printk(PART_LOG_INFO "\tcount %d\n", part->count);
        printk(PART_LOG_INFO "\tsystem 0x%x\n", part->system);

        if (entry->system == PART_FS_EXTENDED){
            printk(PART_WARNNING_INFO "Unsupported extended partition\n");
        }

        device_install(DEV_BLOCK, DEV_DISK_PART, part, part->name, disk_idx,
                    __part_ioctl, __part_read, __part_write);
        
    }

    free_kpage(buf, 1);
}