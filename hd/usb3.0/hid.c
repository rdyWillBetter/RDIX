#include <rdix/usbDescriptor.h>
#include <rdix/xhci.h>
#include <common/interrupt.h>
#include <rdix/memory.h>

general_usb_dev_t *test_dev = NULL;

void USB_HID_interface_init(general_usb_dev_t *dev, void *interface_desc){

    interface_desc_t *desc = interface_desc;
    assert(desc->bInterfaceClass == USB_CLASS_HID_DEVICE);

    cfig_desc_t *cfig_desc = get_configuration_desc(&dev->desc_tree, 1);
    //interface_desc_t *interface = get_interface_desc(cfig_desc, cfig_desc->bNumInterface);

    HID_desc_t *hid_desc = (u32)interface_desc + desc->bLength;
    printk("hid_desc bDescriptorType_%d| bCountryCode_%d| bNumDescriptors_%d\n",
        hid_desc->bDescriptorType, hid_desc->bCountryCode, hid_desc->bNumDescriptors);

    int ep_idx = 0;
    input_context_t *input_context = (input_context_t *)malloc(sizeof(input_context_t));
    for (int i = 1; i <= desc->bNumEndpoints; ++i){
        ep_desc_t *ep_desc = get_endpoint_desc(cfig_desc, interface_desc, i);

        printk("ep_%d bLength_%d| bDescriptorType_%d| bEndpointAddress_%d| wMaxPacketSize_%d\n",
        i, ep_desc->bLength, ep_desc->bDescriptorType,
        ep_desc->bEndpointAddress, ep_desc->wMaxPacketSize);

        memset(input_context, 0, sizeof(input_context_t));

        u8 device_context_num = ((ep_desc->bEndpointAddress & 0xf) << 1) + (ep_desc->bEndpointAddress >> 7);
        assert(device_context_num < 32);

        ep_idx = device_context_num - 1;

        input_context->input_control.add_flag = (1 << device_context_num) | 1;
        printk("device_context_num_%d\n", device_context_num);
        printk("input_context->input_control.add_flag %x\n", input_context->input_control.add_flag);

        input_context->device_context.slot_control.root_hub_port_number = dev->root_port;
        input_context->device_context.slot_control.route_string = 0;
        input_context->device_context.slot_control.contex_entries = device_context_num;

        u32 *tb_base = alloc_kpage(1);
        if (tb_base == NULL)
            PANIC("out of memory\n");
        
        memset(tb_base, 0, PAGE_SIZE);
        u32 *tail_trb = (u32 *)((u32)tb_base + PAGE_SIZE - TRB_SIZE);
        tail_trb[0] = (u32)tb_base;
        tail_trb[3] = (LinkTRB << 10) | 2 | 1;
        
        u8 ep_num = device_context_num - 1;
        dev->ep[ep_num].ring = malloc(sizeof(struct ring_t));
        dev->ep[ep_num].ring->trb = tb_base;
        dev->ep[ep_num].ring->cycle_flag = 1;

        struct endpoint_context *ep = &input_context->device_context.EP[ep_num];
        ep->EP_type = ep_desc->bmAttribute | ((ep_desc->bEndpointAddress >> 7) << 2);
        printk("ep type %d\n", ep->EP_type);
        ep->max_packet_size = ep_desc->wMaxPacketSize & 0x7ff;

        ep->max_burst_size = (ep_desc->wMaxPacketSize & 0x1800) >> 11;
        ep->TR_dequeue_point_lo = (u32)tb_base | 1; // CSS 初始化为 1
        ep->TR_dequeue_point_hi = 0;
        ep->interval = 8;
        printk("interval %d\n", ep_desc->bInterval);
        ep->mult = 0;
        ep->CErr = 3;

        //ep->max_ESIT_payload_lo = (ep->max_burst_size + 1) * ep->max_packet_size;
        //ep->max_ESIT_payload_lo = 16;
        printk("ep->max_ESIT_payload_lo %d\n", ep->max_ESIT_payload_lo);
        //ep->areverage_TRB_length = 8;

        xhc_cmd_configure_endpoint(dev, input_context);
    }

    free(input_context);

    u32 config_value = cfig_desc->bConfigurationVale;
    xhc_transfer_set_configuration(dev, config_value);
    printk("device config success\n");

    //xhc_transfer_set_interface(dev, 0, 0);

    u32 *device_context = ((u64 *)dev->ctrl_dev->op_base[DCBAAP])[dev->slot_id];
    //mdebug((u32)device_context + 32 * 3 + 8, 16);

    dev->state.slot_state == slot_Configured;
    printk("set configuration success\n");


    //test in trb
    u32 *tb_base = dev->ep[ep_idx].ring->trb;
    printk("tb_base %x\n", tb_base);

    for (int i = 0; i < 20; ++i){
        void *test = malloc(8);
        if (i == 0)
            printk("data start %x\n", test);
        tb_base[0] = test;
        tb_base[1] = 0;
        memset(test, 0, 8);
        tb_base[2] = 8;
        tb_base[3] = (Normal << 10) | (1 << 5) | 1;
        tb_base += 4;
    }

    void *test = malloc(8);
    tb_base[0] = test;
    tb_base[1] = 0;
    memset(test, 0, 8);
    tb_base[2] = 8;
    tb_base[3] = (Normal << 10) | (1 << 5) | 1;
    
    test_dev = dev;

    /* void *buf = malloc(1);
    memset(buf, 0, 8);
    get_protocol(dev, buf, 8); */

    //mdebug(buf, 8);

    u32 t = 0xffffff;
    while(t--);
    dev->ctrl_dev->doorbell[dev->slot_id] = ep_idx + 1;

    printk("b\n");
    /* ATOMIC_OPS(
        dev->wait_trb_type = Normal;
        dev->ctrl_dev->doorbell[dev->slot_id] = 3;
        dev->wait_task = current_task();) */
}