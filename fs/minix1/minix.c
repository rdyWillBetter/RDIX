#include <fs/fs.h>

void super_init();
void inode_init();

void minix_init(){
    super_init();
    inode_init();

    device_t *part0 = device_find(DEV_DISK_PART, 0);
    m_inode *dir = iget(part0->dev, 1);
}