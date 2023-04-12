#include <rdix/device.h>
#include <common/string.h>
#include <rdix/kernel.h>
#include <common/assert.h>

#define DEVICE_LOG_INFO __LOG("[device log]")
#define DEVICE_WARNING_INFO __WARNING("[device warning]")

#define DEVICE_NR 64

static device_t devices[DEVICE_NR];

static device_t *get_null_device(){
    for (int i = 0; i < DEVICE_NR; ++i){
        if (devices[i].type == DEV_NULL)
            return &devices[i];
    }
    PANIC("could not install more device\n");
}

/* 注意竞态问题 */
dev_t device_install(
    int type, int subtype,
    void *ptr, char *name, dev_t parent,
    void *ioctl, void *read, void *write){

        device_t *dev = get_null_device();

        dev->ptr = ptr;
        dev->parent = parent;
        dev->type = type;
        dev->subtype = subtype;
        strncpy(dev->name, name, NAMELEN);
        dev->ioctl = ioctl;
        dev->read = read;
        dev->write = write;

        return dev->dev;
    }

/* 查找第 idx 个类型为 subtype */
device_t *device_find(int subtype, idx_t idx){
    idx_t x = 0;

    for (int i = 0; i < DEVICE_NR; ++i){
        if (devices[i].subtype != subtype)
            continue;
        if (x == idx)
            return &devices[i];
        ++x;
    }
    PANIC("no such device\n");
}

device_t *device_get(dev_t dev){
    assert(dev < DEVICE_NR);
    return &devices[dev];
}

int device_ioctl(dev_t dev, int cmd, void *args, int flags){
    device_t *device = device_get(dev);
    if (device->ioctl)
    {
        return device->ioctl(device->ptr, cmd, args, flags);
    }
    printk(DEVICE_WARNING_INFO "ioctl of device %d not implemented!!!\n", dev);
    return EOF;
}

int device_read(dev_t dev, void *buf, size_t count, idx_t idx, int flags)
{
    device_t *device = device_get(dev);
    if (device->read)
    {
        return device->read(device->ptr, buf, count, idx, flags);
    }
    printk(DEVICE_WARNING_INFO "read of device %d not implemented!!!\n", dev);
    return EOF;
}

int device_write(dev_t dev, void *buf, size_t count, idx_t idx, int flags)
{
    device_t *device = device_get(dev);
    if (device->write)
    {
        return device->write(device->ptr, buf, count, idx, flags);
    }
    printk(DEVICE_WARNING_INFO "write of device %d not implemented!!!\n", dev);
    return EOF;
}

void device_init(){
    for (int i = 0; i < DEVICE_NR; ++i){
        device_t *dev = &devices[i];

        strcpy((char *)dev->name, "null");
        dev->type = DEV_NULL;
        dev->subtype = DEV_NULL;
        dev->dev = i;
        dev->parent = 0;
        dev->ioctl = NULL;
        dev->read = NULL;
        dev->write = NULL;

        list_init(&dev->request_list);
    }
}