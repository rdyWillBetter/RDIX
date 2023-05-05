#ifndef __MINIX1_H__
#define __MINIX1_H__

#include <fs/fs.h>
#include <common/type.h>

#define MINIX1_MAGIC 0x137F // MINIX 1.0 魔数
#define NAME_LEN 14         // MINIX 1.0 文件名长度

#define IMAP_NR 8 // inode 位图块，最大值
#define ZMAP_NR 8 // 块位图块，最大值

#define BLOCK_BITS (BLOCK_SIZE * 8) // 长度为一个块的位图能管理多少个块
#define ZONES_IDX_PER_BLOCK (BLOCK_SIZE / sizeof(u16))
#define DIR_ENTRY_PER_BLOCK (BLOCK_SIZE / sizeof(dir_entry))

// 文件类型
#define IFMT  0170000  // 文件类型掩码（8 进制表示）
#define IFREG 0100000  // 常规文件
#define IFBLK 0060000  // 块特殊（设备）文件，如磁盘 dev/fd0
#define IFDIR 0040000  // 目录文件
#define IFCHR 0020000  // 字符设备文件
#define IFIFO 0010000  // FIFO 特殊文件
#define IFLNK 0120000  // 符号连接
#define IFSOCK 0140000 // 套接字

// 文件属性位：
// ISUID 用于测试文件的 set-user-ID 标志是否置位
// 若该标志置位，则当执行该文件时，
// 进程的有效用户 ID 将被设置为该文件宿主的用户 ID
// ISGID 则是针对组 ID 进行相同处理
#define ISUID 0004000 // 执行时设置用户 ID（set-user-ID）
#define ISGID 0002000 // 执行时设置组 ID（set-group-ID）

// 若该为置位，则表示非文件用户没有删除权限
#define ISVTX 0001000 // 对于目录，受限删除标志

#define ISREG(m) (((m)&IFMT) == IFREG)   // 是常规文件
#define ISDIR(m) (((m)&IFMT) == IFDIR)   // 是目录文件
#define ISCHR(m) (((m)&IFMT) == IFCHR)   // 是字符设备文件
#define ISBLK(m) (((m)&IFMT) == IFBLK)   // 是块设备文件
#define ISFIFO(m) (((m)&IFMT) == IFIFO)  // 是 FIFO 特殊文件
#define ISLNK(m) (((m)&IFMT) == ISLNK)   // 是符号连接文件
#define ISSOCK(m) (((m)&IFMT) == ISSOCK) // 是套接字文件
#define ISFILE(m) ISREG(m)               // 更直观的一个宏

// 文件访问权限
#define IRWXU 00700 // 宿主可以读、写、执行/搜索
#define IRUSR 00400 // 宿主读许可
#define IWUSR 00200 // 宿主写许可
#define IXUSR 00100 // 宿主执行/搜索许可

#define IRWXG 00070 // 组成员可以读、写、执行/搜索
#define IRGRP 00040 // 组成员读许可
#define IWGRP 00020 // 组成员写许可
#define IXGRP 00010 // 组成员执行/搜索许可

#define IRWXO 00007 // 其他人读、写、执行/搜索许可
#define IROTH 00004 // 其他人读许可
#define IWOTH 00002 // 其他人写许可
#define IXOTH 00001 // 其他人执行/搜索许可

typedef int32 inode_t;

enum file_flag
{
    O_RDONLY = 00,      // 只读方式
    O_WRONLY = 01,      // 只写方式
    O_RDWR = 02,        // 读写方式
    O_ACCMODE = 03,     // 文件访问模式屏蔽码
    O_CREAT = 00100,    // 如果文件不存在就创建
    O_EXCL = 00200,     // 独占使用文件标志
    O_NOCTTY = 00400,   // 不分配控制终端
    O_TRUNC = 01000,    // 若文件已存在且是写操作，则长度截为 0
    O_APPEND = 02000,   // 以添加方式打开，文件指针置为文件尾
    O_NONBLOCK = 04000, // 非阻塞方式打开和操作文件
};

/* 由磁盘保存的数据结构 */
typedef struct d_inode
{
    u16 mode;    // 文件类型和属性(rwx 位)
    u16 uid;     // 用户id（文件拥有者标识符）
    u32 size;    // 文件大小（字节数）
    u32 mtime;   // 修改时间戳 这个时间戳应该用 UTC 时间，不然有瑕疵
    u8 gid;      // 组id(文件拥有者所在的组)
    u8 nlinks;   // 链接数（多少个文件目录项指向该i 节点）
    u16 zone[9]; // 直接 (0-6)、间接(7)或双重间接 (8) 逻辑块号
} d_inode;

/* 由内存保存的数据结构 */
typedef struct m_inode
{
    d_inode *desc;   // inode 描述符
    buffer_t *buf; // inode 描述符对应 buffer
    dev_t dev;            // 设备号
    inode_t nr;             // i 节点号
    u32 count;            // 引用计数
    mutex_t lock;
    time_t atime;         // 访问时间
    time_t ctime;         // 创建时间
    dev_t mount;          // 安装设备
} m_inode;

typedef struct d_super_block
{
    u16 inodes;        // 节点数
    u16 zones;         // 文件系统包含的总块数
    u16 imap_blocks;   // i 节点位图所占用的数据块数
    u16 zmap_blocks;   // 逻辑块位图所占用的数据块数
    u16 firstdatazone; // 第一个数据逻辑块号
    u16 log_zone_size; // 块大小 = 2^x * 1K byte
    u32 max_size;      // 文件最大长度
    u16 magic;         // 文件系统魔数
} d_super_block;

typedef struct super_block_t
{
    d_super_block *desc;              // 超级块描述符
    buffer_t *buf;            // 超级块描述符 buffer
    buffer_t *imaps[IMAP_NR]; // inode 位图缓冲
    buffer_t *zmaps[ZMAP_NR]; // 块位图缓冲
    dev_t dev;                       // 设备号
    m_inode *iroot;                  // 根目录 inode
    m_inode *imount;                 // 安装到的 inode
} super_block_t;

typedef struct dir_entry {
	u16 inode;
	char name[NAME_LEN];
} dir_entry;

typedef struct mode_t{
    dev_t dev;    // 含有文件的设备号
    idx_t nr;     // 文件 i 节点号
    u16 mode;     // 文件类型和属性
    u8 nlinks;    // 指定文件的连接数
    u16 uid;      // 文件的用户(标识)号
    u8 gid;       // 文件的组号
    dev_t rdev;   // 设备号(如果文件是特殊的字符文件或块文件)
    size_t size;  // 文件大小（字节数）（如果文件是常规文件）
    time_t atime; // 上次（最后）访问时间
    time_t mtime; // 最后修改时间
    time_t ctime; // 最后节点修改时间
} mode_t;

typedef struct file_t
{
    m_inode *inode; // 文件 inode
    u32 count;      // 引用计数
    idx_t offset;   // 文件偏移
    int flags;      // 文件标记
    int mode;       // 文件模式
} file_t;

super_block_t *get_super(dev_t dev);  // 获得 dev 对应的超级块
super_block_t *read_super(dev_t dev); // 读取 dev 对应的超级块

idx_t balloc(dev_t dev);          // 分配一个文件块
void bfree(dev_t dev, idx_t idx); // 释放一个文件块
inode_t ialloc(dev_t dev);          // 分配一个文件系统 inode
void ifree(dev_t dev, idx_t idx); // 释放一个文件系统 inode

idx_t bmap(m_inode *inode, idx_t block, bool create);

m_inode *iget(dev_t dev, inode_t nr);
void iput(m_inode *inode);
int inode_read(m_inode *inode, char *buf, u32 len, idx_t offset);
int inode_write(m_inode *inode, char *buf, u32 len, idx_t offset);

buffer_t *find_entry(m_inode *dir, const char *name, dir_entry **res);
buffer_t *add_entry(m_inode *dir, const char *name, dir_entry **res);
m_inode *namei(const char *pathname);
void inode_truncate(m_inode *inode);
m_inode *inode_open(char *pathname, int flag, int mode);
m_inode *get_root();

int sys_mkdir(const char *pathname, int mode);
int sys_rmdir(const char *pathname);
int sys_link(char *oldname, char *newname);
int sys_unlink(char *pathname);

#endif