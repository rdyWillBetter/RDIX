#include <rdix/xhci.h>
#include <rdix/task.h>
#include <common/interrupt.h>

extern xhc_t* xhc;

static void next_trb(struct ring_t *ring){
    u32 *current_trb = ring->trb;
    bool current_PCS = ring->cycle_flag;

    u32 *trb = current_trb + 4;
    bool PCS = current_PCS;

    if (((trb[3] >> 10) & 0x3f) == LinkTRB){
        if (PCS)
            trb[3] |= 1;
        else
            trb[3] &= ~1;

        trb = trb[0];
        if ((trb[3] >> 1) & 1)
            PCS = ~PCS;
    }
    
    ring->trb = trb;
    ring->cycle_flag = PCS;
}

/* xhc 命令 */
/* 1) enable slot，获取一个可用的slot id */
void xhc_cmd_enable_slot(general_usb_dev_t *dev){
    u32 *trb = xhc->cmd_ring.trb;
    bool PCS = xhc->cmd_ring.cycle_flag;
    memset(trb, 0, TRB_SIZE);

    trb[3] = (GetEnableSlot << 10) | PCS;

    next_trb(&xhc->cmd_ring);

    dev->wait_trb_type = GetEnableSlot;
    dev->last_event_trb = NULL;
    /* host control command */
    ATOMIC_OPS(
        xhc->doorbell[0] = 0;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
    
    assert(dev->last_event_trb != NULL);
    dev->slot_id = ((u32 *)dev->last_event_trb)[3] >> 24;
    dev->state.slot_state = slot_Enabled;
}

/* 2) address device */
void xhc_cmd_address_device(general_usb_dev_t *dev, u32 *input_context_point, bool BSR){
    u32 slot_id = dev->slot_id;
    assert(slot_id < xhc->max_slot && slot_id > 0);
    printk("in xhc_address_device, slot id %x\n", slot_id);

    u32 *trb = xhc->cmd_ring.trb;
    bool PCS = xhc->cmd_ring.cycle_flag;
    memset(trb, 0, TRB_SIZE);

    assert(((u32)input_context_point & 0xf) == 0);
    trb[0] = input_context_point;
    trb[3] = (slot_id << 24) | (AddressDevice << 10) | (BSR << 9) | PCS;

    next_trb(&xhc->cmd_ring);

    dev->wait_trb_type = AddressDevice;
    dev->last_event_trb = NULL;

    u32 t = 0xffffff;
    while (--t);
    /* host control command(主控制器命令，就是往 0 号 doorbell 寄存器里写 0) */
    ATOMIC_OPS(
        xhc->doorbell[0] = 0;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)

    /* 命令执行成功 */
    dev->state.slot_state = slot_Addressed;
}

/* 3) configure endpoint */
void xhc_cmd_configure_endpoint(general_usb_dev_t *dev, u32 *input_context_point){
    u32 slot_id = dev->slot_id;
    assert(slot_id < xhc->max_slot && slot_id > 0);
    printk("in xhc_cmd_configure_endpoint, slot id %x\n", slot_id);

    u32 *trb = xhc->cmd_ring.trb;
    bool PCS = xhc->cmd_ring.cycle_flag;
    memset(trb, 0, TRB_SIZE);

    assert(((u32)input_context_point & 0xf) == 0);
    trb[0] = input_context_point;
    trb[3] = (slot_id << 24) | (ConfigureEndpoint << 10) | PCS;

    next_trb(&xhc->cmd_ring);

    dev->wait_trb_type = ConfigureEndpoint;
    dev->last_event_trb = NULL;
    /* host control command(主控制器命令，就是往 0 号 doorbell 寄存器里写 0) */
    ATOMIC_OPS(
        xhc->doorbell[0] = 0;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
}

/* 设备标准请求 */
/* 1) GET_DESCRIPTOR 获取描述符请求 */
void xhc_transfer_get_descriptor(general_usb_dev_t *dev, u8 desc_type, u8 desc_index, void *buf, u16 bfsize){
    assert(dev->state.slot_state == slot_Addressed);

    endpoint_m *ep0 = &dev->ep[0];

    /* setup stage */
    u32 *trb = ep0->ring->trb;
    bool PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = (desc_type << 24) | (desc_index << 16) | (GET_DESCRIPORT << 8) | 0x80;
    trb[1] = (bfsize << 16);
    trb[2] = 8;
    trb[3] = (SetupStage << 10) | (INDataStage << 16) | (1 << 6) | PCS;

    next_trb(ep0->ring);

    /* data stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = buf;
    trb[2] = bfsize;
    trb[3] = (DataStage << 10) | (__IN << 16) | PCS;

    next_trb(ep0->ring);

    /* status stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[3] = (StatusStage << 10) | (__OUT << 16) | (1 << 5) | PCS;

    next_trb(ep0->ring);

    //printk("```slot_id = %d\n", dev->slot_id);

    dev->wait_trb_type = StatusStage;
    dev->last_event_trb = NULL;
    /* 原子操作防止 block 前 先执行了 unblock */
    ATOMIC_OPS(
        xhc->doorbell[dev->slot_id] = 1;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
}

/* 2) SET_CONFIGURATION 请求 */
void xhc_transfer_set_configuration(general_usb_dev_t *dev, u32 config_value){
    assert(dev->state.slot_state == slot_Addressed);

    endpoint_m *ep0 = &dev->ep[0];
    xhc_t *xhc = dev->ctrl_dev;

    /* setup stage */
    u32 *trb = ep0->ring->trb;
    bool PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = (config_value << 16) | (SET_CONFIGURATION << 8);
    trb[2] = 8;
    trb[3] = (SetupStage << 10) | (NoDataStage << 16) | (1 << 6) | PCS;

    next_trb(ep0->ring);

    /* status stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[3] = (StatusStage << 10) | (__IN << 16) | (1 << 5) | PCS;

    next_trb(ep0->ring);

    dev->wait_trb_type = StatusStage;
    dev->last_event_trb = NULL;
    ATOMIC_OPS(
        xhc->doorbell[dev->slot_id] = 1;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
}

/* 3) SET_INTERFACE 请求 */
void xhc_transfer_set_interface(general_usb_dev_t *dev, u8 interface, u8 alter_set){
    assert(dev->state.slot_state == slot_Addressed);

    endpoint_m *ep0 = &dev->ep[0];
    xhc_t *xhc = dev->ctrl_dev;

    /* setup stage */
    u32 *trb = ep0->ring->trb;
    bool PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = (alter_set << 16) | (SET_INTERFACE << 8) | 1;
    trb[1] = interface;
    trb[2] = 8;
    trb[3] = (SetupStage << 10) | (NoDataStage << 16) | (1 << 6) | PCS;

    next_trb(ep0->ring);

    /* status stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[3] = (StatusStage << 10) | (__IN << 16) | (1 << 5) | PCS;

    next_trb(ep0->ring);

    dev->wait_trb_type = StatusStage;
    dev->last_event_trb = NULL;
    ATOMIC_OPS(
        xhc->doorbell[dev->slot_id] = 1;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
}


void get_idle(general_usb_dev_t *dev, void *buf, u32 bsize){
    endpoint_m *ep0 = &dev->ep[0];
    xhc_t *xhc = dev->ctrl_dev;

    /* setup stage */
    u32 *trb = ep0->ring->trb;
    bool PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = (2 << 8) | 0b10100001;
    trb[1] = 1 << 16;
    trb[2] = 8;
    trb[3] = (SetupStage << 10) | (NoDataStage << 16) | (1 << 6) | PCS;

    next_trb(ep0->ring);

    /* data stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = buf;
    trb[2] = bsize;
    trb[3] = (DataStage << 10) | (__IN << 16) | PCS;

    next_trb(ep0->ring);

    /* status stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[3] = (StatusStage << 10) | (__OUT << 16) | (1 << 5) | PCS;

    next_trb(ep0->ring);

    dev->wait_trb_type = StatusStage;
    dev->last_event_trb = NULL;
    ATOMIC_OPS(
        xhc->doorbell[dev->slot_id] = 1;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
}

void get_protocol(general_usb_dev_t *dev, void *buf, u32 bsize){
    endpoint_m *ep0 = &dev->ep[0];
    xhc_t *xhc = dev->ctrl_dev;

    /* setup stage */
    u32 *trb = ep0->ring->trb;
    bool PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = (3 << 8) | 0b10100001;
    trb[1] = 1 << 16;
    trb[2] = 8;
    trb[3] = (SetupStage << 10) | (NoDataStage << 16) | (1 << 6) | PCS;

    next_trb(ep0->ring);

    /* data stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = buf;
    trb[2] = bsize;
    trb[3] = (DataStage << 10) | (__IN << 16) | PCS;

    next_trb(ep0->ring);

    /* status stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[3] = (StatusStage << 10) | (__OUT << 16) | (1 << 5) | PCS;

    next_trb(ep0->ring);

    dev->wait_trb_type = StatusStage;
    dev->last_event_trb = NULL;
    ATOMIC_OPS(
        xhc->doorbell[dev->slot_id] = 1;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
}

void get_report(general_usb_dev_t *dev, void *buf, u32 bsize){
    endpoint_m *ep0 = &dev->ep[0];
    xhc_t *xhc = dev->ctrl_dev;

    /* setup stage */
    u32 *trb = ep0->ring->trb;
    bool PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = (1 << 16) | (1 << 8) | 0b10100001;
    trb[1] = 8 << 16;
    trb[2] = 8;
    trb[3] = (SetupStage << 10) | (NoDataStage << 16) | (1 << 6) | PCS;

    next_trb(ep0->ring);

    /* data stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[0] = buf;
    trb[2] = bsize;
    trb[3] = (DataStage << 10) | (__IN << 16) | PCS;

    next_trb(ep0->ring);

    /* status stage */
    trb = ep0->ring->trb;
    PCS = ep0->ring->cycle_flag;
    memset(trb, 0, TRB_SIZE);
    trb[3] = (StatusStage << 10) | (__OUT << 16) | (1 << 5) | PCS;

    next_trb(ep0->ring);

    dev->wait_trb_type = StatusStage;
    dev->last_event_trb = NULL;
    ATOMIC_OPS(
        xhc->doorbell[dev->slot_id] = 1;
        dev->wait_task = current_task();
        block(NULL, NULL, TASK_BLOCKED);
        dev->wait_task = NULL;)
}