# PCI 配置空间（PCI 3.0）
![PCI 配置空间](./img/PCI%20Configuration%20Space%20Header.png)
* Vendor ID == 0xffff 代表该设备无效
* Header Type 的第七位表示该设备是否为多功能设备
---

# PCI 配置报文(PCI configuration transactions)
![PCI 访问指令](./img/PCI%20access%20command.png)
* type = 00 代表直连主桥（北桥）的 PCI 总线。  
* type = 01 用于传递指令到非直连主桥的总线。  
第 31 位是使能位，只有当这位是一时，数据寄存器中的内容才会被发送到 PCI 总线上

# PCI bridge 配置空间
![PCI bridge 配置空间](./img/PCI%20bridge%20configure%20space.png)
primary bus 和 secondary bus 的含义
![PCI bridge 连接拓扑结构](./img/PCI%20bridge%20connection%20illustrates.png)


# 端口号
```c
#define PCI_CONFIG_ADDRESS 0xcf8 \\地址端口 32位
#define PCI_CONFIG_DATA 0xcfc    \\命令端口 32位