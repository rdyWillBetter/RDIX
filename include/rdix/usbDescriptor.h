#ifndef __USB_Descriptor_H__
#define __USB_Descriptor_H__

#include <common/type.h>
#include <common/list.h>

/* 描述符类型 */
enum usb_descriptor_type{
    DESC_DEVICE = 1,
    DESC_CONFIGURATION,
    DESC_STRING,
    DESC_INTERFACE,
    DESC_ENDPOINT,
    DESC_HID_DESCRIPTOR = 0x21,
};

enum usb_class{
    USB_CLASS_HID_DEVICE = 3,
    USB_CLASS_MASS_STORAGE_DEVICE = 8,
};

typedef struct general_desc_t{
    u8 bLength;
    u8 bDescriptorType;
} _packed general_desc_t;

typedef struct device_descriptor_t{
    u8 bLength;
    u8 bDescriptorType;
    u16 bsdUSB;
    u8 bDeviceClass;
    u8 bDeviceSubClass;
    u8 bDeviceProtocol;
    u8 bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8 iManufacturer;
    u8 iProduct;
    u8 iSerialNumber;
    u8 bNumConfiguration;
} _packed device_descriptor_t;

typedef struct configuration_descriptor_t{
    u8 bLength;
    u8 bDescriptorType;   //描述符类型编号，为0x02 
    u16 wTotalLength;     //配置所返回的所有数量的大小 
    u8 bNumInterface;     //此配置所支持的接口数量 
    u8 bConfigurationVale;   //Set_Configuration命令需要的参数值 
    u8 iConfiguration;       //描述该配置的字符串的索引值 
    u8 bmAttribute;           //供电模式的选择 
    u8 MaxPower;             //设备从总线提取的最大电流
} _packed cfig_desc_t;

typedef struct interface_descriptor_t{
    u8 bLength;           //设备描述符的字节数大小，为0x09 
    u8 bDescriptorType;   //描述符类型编号，为0x04
    u8 bInterfaceNumber; //接口的编号 
    u8 bAlternateSetting;//备用的接口描述符编号 
    u8 bNumEndpoints;     //该接口使用端点数，不包括端点0 
    u8 bInterfaceClass;   //接口类型 
    u8 bInterfaceSubClass;//接口子类型 
    u8 bInterfaceProtocol;//接口所遵循的协议 
    u8 iInterface;         //描述该接口的字符串索引值
} _packed interface_desc_t;

typedef struct endpoint_descriptor_t{ 
    u8 bLength;           //设备描述符的字节数大小，为0x7 
    u8 bDescriptorType;   //描述符类型编号，为0x05
    u8 bEndpointAddress; //端点地址及输入输出属性 
    u8 bmAttribute;       //端点的传输类型属性 
    u16 wMaxPacketSize;   //端点收、发的最大包的大小 
    u8 bInterval;         //主机查询端点的时间间隔 
} _packed ep_desc_t;

typedef struct HID_descriptor_t{
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdHID;
    u8 bCountryCode;
    u8 bNumDescriptors;
} HID_desc_t;

/* 描述符的内存模型 */
typedef struct device_descriptor_m{
    device_descriptor_t desc;
    List_t configs;
} device_descriptor_m;

typedef struct configuration_descriptor_m{
    void *desc; //包含配置，接口，HID，端点描述符
    ListNode_t node;
} cfig_desc_m;

#endif