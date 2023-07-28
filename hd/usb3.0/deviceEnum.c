#include <rdix/xhci.h>
#include <rdix/task.h>
#include <rdix/usbDescriptor.h>
#include <common/interrupt.h>
#include <common/string.h>
#include <common/stdlib.h>

#define ENUM_LOG_INFO __LOG("[xhc enum]")

/* address device指令执行成功后加入设备枚举链表 */
extern List_t *dev_enum_list;

cfig_desc_t *get_configuration_desc(device_descriptor_m *tree, int config_number){
    assert(!list_isempty(&tree->configs));
    assert(config_number > 0);

    List_t *list = &tree->configs;
    ListNode_t *node = &list->end;

    for (int i = 0; i < config_number; ++i){
        node = node->next;
        if (node == &list->end)
            return NULL;
    }

    return ((cfig_desc_m *)node->owner)->desc;
}

interface_desc_t *get_interface_desc(void *configuration, int interface_number){
    cfig_desc_t *cfig_desc = configuration;
    assert(cfig_desc->bDescriptorType == DESC_CONFIGURATION);
    assert(cfig_desc->bNumInterface >= interface_number);
    assert(interface_number > 0);   //从 1 开始计数

    general_desc_t *desc = configuration;
    int count = 0;
    while ((u32)desc < cfig_desc->wTotalLength + (u32)cfig_desc){
        if (desc->bDescriptorType == DESC_INTERFACE){
            ++count;
            if (count == interface_number)
                return (interface_desc_t *)desc;
        }
        desc = (general_desc_t *)((u32)desc + desc->bLength);
    }

    return NULL;
}

ep_desc_t *get_endpoint_desc(cfig_desc_t *cfig_desc, void *interface, int ep_number){
    interface_desc_t *interface_desc = interface;
    assert(interface_desc->bDescriptorType == DESC_INTERFACE);
    assert(interface_desc->bNumEndpoints >= ep_number);
    assert(ep_number > 0);  //从 1 开始计数

    general_desc_t *desc = interface;
    int count = 0;
    while ((u32)desc < cfig_desc->wTotalLength + (u32)cfig_desc){
        if (desc->bDescriptorType == DESC_ENDPOINT){
            ++count;
            if (count == ep_number)
                return (ep_desc_t *)desc;
        }
        desc = (general_desc_t *)((u32)desc + desc->bLength);
    }

    return NULL;
}

static device_descriptor_m *build_decriptor_tree(general_usb_dev_t *dev){
    list_init(&dev->desc_tree.configs);
    /* 获取设备描述符 */
    xhc_transfer_get_descriptor(dev, DESC_DEVICE, 0, &dev->desc_tree.desc, sizeof(device_descriptor_t));

    device_descriptor_t *dev_desc = &dev->desc_tree.desc;
    for (int i = 0; i < dev_desc->bNumConfiguration; ++i){
        cfig_desc_m *cfig = (cfig_desc_m *)malloc(sizeof(cfig_desc_m));
        node_init(&cfig->node, cfig, 0);
        list_push(&dev->desc_tree.configs, &cfig->node);

        /* 获取配置描述符 */
        cfig_desc_t *buf = (cfig_desc_t *)malloc(sizeof(cfig_desc_t));
        xhc_transfer_get_descriptor(dev, DESC_CONFIGURATION, i, buf, sizeof(cfig_desc_t));
        printk("ok\n");
        /* 获取全部描述符 */
        cfig->desc = malloc(buf->wTotalLength);
        /* if (buf->wTotalLength == 0x3b)
            buf->wTotalLength = sizeof(cfig_desc_t); */
        xhc_transfer_get_descriptor(dev, DESC_CONFIGURATION, i, cfig->desc, buf->wTotalLength);
    }
}

/* 内核线程开中断 */
void usb_device_enumeration(){
    set_IF(true);
    general_usb_dev_t *dev = NULL;
    input_context_t *input_context = (input_context_t *)malloc(sizeof(input_context_t));

    while (true){
        ATOMIC_OPS(
        if (list_isempty(dev_enum_list))
            block(NULL, NULL, TASK_WAITING);)

        printk(ENUM_LOG_INFO "device enum 1\n");
        ATOMIC_OPS(
        dev = (general_usb_dev_t *)list_pop(dev_enum_list)->owner;)

        xhc_t *xhc = dev->ctrl_dev;
        assert(xhc);

        /* 获取slot */
        xhc_cmd_enable_slot(dev);

#pragma region: address device
        /* address device */
        /* 1) 分配 input context 空间，并将所有字段初始化为 0 */
        memset(input_context, 0, sizeof(input_context_t));

        /* 2) 初始化 input control 字段值，将 a0 和 a1 设置为 1 */
        input_context->input_control.add_flag = 3;

        /* 3) 初始化 slot context */
        input_context->device_context.slot_control.root_hub_port_number = dev->root_port;
        input_context->device_context.slot_control.route_string = 0;
        input_context->device_context.slot_control.contex_entries = 1;

        /* 4) 为 EP0 分配和初始化 transfer ring */
        void *tb_base = alloc_kpage(1);
        if (tb_base == NULL)
            PANIC("out of memory\n");
        
        memset(tb_base, 0, PAGE_SIZE);
        /* 初始化 link trb */
        u32 *tail_trb = (u32 *)((u32)tb_base + PAGE_SIZE - TRB_SIZE);
        tail_trb[0] = (u32)tb_base;
        tail_trb[3] = (LinkTRB << 10) | 2 | 1;

        /* 加载 EP0 的 transfer ring */
        dev->ep[0].ring = malloc(sizeof(struct ring_t));
        dev->ep[0].ring->trb = tb_base;
        dev->ep[0].ring->cycle_flag = 1;
        
        /* 5) 初始化 EP0 context */
        struct endpoint_context *ep = &input_context->device_context.EP[0];
        ep->EP_type = EP_TYPE_CONTROL;
        //u8 speed_id = PORT_SPEED(xhc, dev->root_port);
        switch(PORT_SPEED(xhc, dev->root_port)){
            case Full_speed: printk("Full speed device\n");
                ep->max_packet_size = 64;
                break;
            case Low_speed: printk("Low speed device\n");
                ep->max_packet_size = 8;
                break;
            case High_speed: printk("High speed device\n");
                ep->max_packet_size = 64;
                break;
            case SuperSpeed: printk("Super speed device\n");
                ep->max_packet_size = 512;
                break;
            case SuperSpeedPlus: PANIC("Not currently support super speed plus device\n");
                break;
            default: PANIC("Unable to prase device speed\n");
        }

        ep->max_burst_size = 0;
        ep->TR_dequeue_point_lo = (u32)tb_base | 1; // CSS 初始化为 1
        ep->TR_dequeue_point_hi = 0;
        ep->interval = 0;
        ep->maxPstreams = 0;
        ep->mult = 0;
        ep->CErr = 3;

        /* 6) 初始化 output device context，提供给 xhc 使用 */
        device_context_t *device_context = (device_context_t *)malloc(sizeof(device_context_t));
        memset(device_context, 0, sizeof(device_context_t));

        /* 7) 在 DCBAA 中加载 output device context */
        u64 *dcbaap = xhc->op_base[DCBAAP];
        dcbaap[dev->slot_id] = (u64)device_context;

        /* 8) 发送 address device command */
        xhc_cmd_address_device(dev, input_context, 0);

#pragma endregion

        printk("address success\n");
        assert(dev->state.slot_state == slot_Addressed);

        /* 获取设备描述符 */
        build_decriptor_tree(dev);

        device_descriptor_t *dev_desc = &dev->desc_tree.desc;
        cfig_desc_t *cfig_desc = get_configuration_desc(&dev->desc_tree, 1);
        interface_desc_t *interface_desc = get_interface_desc(cfig_desc, 1);

        printk("bLength_%d| bDescriptorType_%d| bMaxPacketSize0_%x| bNumConfiguration_%d\n",
            dev_desc->bLength, dev_desc->bDescriptorType,
            dev_desc->bMaxPacketSize0, dev_desc->bNumConfiguration);

        printk("configration num %d\n", dev->desc_tree.configs.number_of_node);

        printk("cfig bLength_%d| bDescriptorType_%d| wTotalLength_%d| bConfigurationVale_%d\n",
            cfig_desc->bLength, cfig_desc->bDescriptorType,
            cfig_desc->wTotalLength, cfig_desc->bConfigurationVale);
        
        printk("interface num %d\n", cfig_desc->bNumInterface);

        printk("interface bLength_%d| bDescriptorType_%d| bAlternateSetting_%d| bInterfaceNumber_%d\n",
            interface_desc->bLength, interface_desc->bDescriptorType,
            interface_desc->bNumEndpoints, interface_desc->bInterfaceNumber);

        /* for (int i = 1; i <= interface_desc->bNumEndpoints; ++i){
            ep_desc_t *ep_desc = get_endpoint_desc(cfig_desc, interface_desc, i);

            printk("ep_%d bLength_%d| bDescriptorType_%d| bEndpointAddress_%d| wMaxPacketSize_%d\n",
            i, ep_desc->bLength, ep_desc->bDescriptorType,
            ep_desc->bEndpointAddress, ep_desc->wMaxPacketSize);

            memset(input_context, 0, sizeof(input_context_t));

            u8 device_context_num = ((ep_desc->bEndpointAddress & 0xf) << 1) + (ep_desc->bEndpointAddress >> 7);
            assert(device_context_num < 32);
            input_context->input_control.add_flag = (1 << device_context_num);
            printk("device_context_num_%d\n", device_context_num);
            printk("input_context->input_control.add_flag %x\n", input_context->input_control.add_flag);

            input_context->device_context.slot_control.root_hub_port_number = dev->root_port;
            input_context->device_context.slot_control.route_string = 0;
            input_context->device_context.slot_control.contex_entries = device_context_num;

            tb_base = alloc_kpage(1);
            if (tb_base == NULL)
                PANIC("out of memory\n");
            
            memset(tb_base, 0, PAGE_SIZE);
            tail_trb = (u32 *)((u32)tb_base + PAGE_SIZE - TRB_SIZE);
            tail_trb[0] = (u32)tb_base;
            tail_trb[3] = (LinkTRB << 10) | 2;
            
            u8 ep_num = device_context_num - 1;
            dev->ep[ep_num].ring = malloc(sizeof(struct ring_t));
            dev->ep[ep_num].ring->trb = tb_base;
            dev->ep[ep_num].ring->cycle_flag = 1;

            ep = &input_context->device_context.EP[ep_num];
            ep->EP_type = ep_desc->bmAttribute | ((ep_desc->bEndpointAddress >> 7) << 2);
            printk("ep type %d\n", ep->EP_type);
            ep->max_packet_size = ep_desc->wMaxPacketSize & 0x7ff;

            ep->max_burst_size = (ep_desc->wMaxPacketSize & 0x1800) >> 11;
            ep->TR_dequeue_point_lo = (u32)tb_base | 1; // CSS 初始化为 1
            ep->TR_dequeue_point_hi = 0;
            ep->interval = ep_desc->bInterval;
            printk("ep->interval %d\n", ep->interval);
            ep->maxPstreams = 0;
            ep->mult = 0;
            ep->CErr = 3;

            ep->max_ESIT_payload_lo = (ep->max_burst_size + 1) * ep->max_packet_size;
            printk("ep->max_ESIT_payload_lo %d\n", ep->max_ESIT_payload_lo);
            ep->areverage_TRB_length = 1024;

            xhc_cmd_configure_endpoint(dev, input_context);
        }

        u32 config_value = cfig_desc->bConfigurationVale;
        xhc_transfer_set_configuration(dev, config_value);

        xhc_transfer_set_interface(dev, interface_desc->bInterfaceNumber, interface_desc->bInterfaceNumber);

        mdebug((u32)device_context + 32 * 3 + 16, 16);

        // todo: 初始化端点，启用端点

        dev->state.slot_state == slot_Configured;
        printk("set configuration success\n"); */

        /* 接口驱动加载 */
        switch (interface_desc->bInterfaceClass){
            case USB_CLASS_HID_DEVICE:
                USB_HID_interface_init(dev, interface_desc);
                break;
            
            default:
                break;
        }

    }
}