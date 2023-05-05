#include <rdix/device.h>
#include <common/string.h>
#include <rdix/kernel.h>
#include <common/assert.h>
#include <common/interrupt.h>

#define DEVICE_LOG_INFO __LOG("[device log]")
#define DEVICE_WARNING_INFO __WARNING("[device warning]")

#define DEVICE_NR 64

static int32 BLK_DEV_CNT;

static device_t devices[DEVICE_NR];

void get_disk_name(char *name){
    assert(sprintf(name, "hd%c", 'a' + BLK_DEV_CNT) < 4);
    ++BLK_DEV_CNT;
}

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
        dev->request_list = NULL;
        dev->do_request = false;
        dev->req_direct = DIRECT_FORE;

        /* 是块设备并且是磁盘设备才初始化块设备请求链表 */
        if (type == DEV_BLOCK && subtype == DEV_SATA_DISK)
            dev->request_list = new_list();

        return dev->dev;
    }

/* 查找第 idx 个类型为 subtype */
device_t *device_find(int subtype, idx_t idx){
    idx_t x = 0;

    for (int i = 0; i < DEVICE_NR; ++i){
        if (devices[i].subtype != subtype)
            continue;
        if (x == idx){
            return &devices[i];
        }
        ++x;
    }
    return NULL;
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

static void do_request(request_t *req){
    int status = 0;
    switch (req->type)
    {
    case REQ_READ:
        status = device_read(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    case REQ_WRITE:
        status = device_write(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    
    default:
        break;
    }

    if (status == EOF){
        device_ioctl(req->dev, DEV_ERROR_REPORT, NULL, 0);
        block(NULL, NULL, TASK_BLOCKED);
    }
        
    
}

static request_t * get_next_req(device_t *device, request_t *req){
    List_t *list = req->node.container;

    assert(list == device->request_list);

    if (device->req_direct == DIRECT_FORE && req->node.next == &list->end)
        device->req_direct = DIRECT_BACK;
    else if (device->req_direct == DIRECT_BACK && req->node.previous == &list->end)
        device->req_direct = DIRECT_FORE;

    request_t *next = NULL;

    if (device->req_direct == DIRECT_FORE)
        next = (request_t *)req->node.next->owner;
    if (device->req_direct == DIRECT_BACK)
        next = (request_t *)req->node.previous->owner;

    return next;
}

void device_request(buffer_t *bf, u8 count, idx_t idx, int flags, u32 type){
    device_t * device = device_get(bf->b_dev);
    assert(device->type == DEV_BLOCK);

    idx_t offset = idx + device_ioctl(bf->b_dev, DEV_CMD_SECTOR_START, NULL, 0);

    if (device->parent)
        device = device_get(device->parent);
    
    assert(device->request_list);

    request_t *req = (request_t *)malloc(sizeof(request_t));

    req->dev = device->dev;
    req->buf = bf->b_data;
    req->count = count;
    req->idx = offset;
    req->flags = flags;
    req->type = type;
    req->task = current_task();
    node_init(&req->node, req, (u32)req->idx);

    bool st = get_and_disable_IF();

    ATOMIC_OPS(list_insert(device->request_list, &req->node, greater);)

    while (true){
        if (!device->do_request){   //这里为原子操作
            ATOMIC_OPS(device->do_request = true;)
            buffer_lock(bf);

            do_request(req);

            buffer_unlock(bf);
            ATOMIC_OPS(device->do_request = false;)
            
            request_t *next = NULL;

            ATOMIC_OPS(next = get_next_req(device, req);
                        remove_node(&req->node);)

            if (next)
                unblock(next->task);

            break;
        }
        else{
            //printk("------------in block\n");
            block(NULL, NULL, TASK_BLOCKED);
        }
    }

    set_IF(st);

    free(req);
}

void device_init(){
    BLK_DEV_CNT = 0;

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