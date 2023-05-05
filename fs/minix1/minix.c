#include <fs/fs.h>

void super_init();
void inode_init();

void minix_init(){
    super_init();
    inode_init();
}