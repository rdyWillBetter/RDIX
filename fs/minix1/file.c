#include <fs/fs.h>
#include <common/assert.h>
#include <rdix/kernel.h>

fd_t sys_open(char *filename, int flags, int mode)
{
    m_inode *inode = inode_open(filename, flags, mode);
    if (!inode)
        return EOF;

    TCB_t *task = (TCB_t *)current_task()->owner;
    fd_t fd = 3; // 0 1 2，分别是标准输入输出和错误流

    for (; fd < TASK_FILE_NR; ++fd){
        if (!task->files[fd])
            break;
    }

    if (fd == TASK_FILE_NR){
        iput(inode);
        return EOF;
    }
        
    task->files[fd] = (file_t *)malloc(sizeof(file_t));

    file_t *file = task->files[fd];

    file->inode = inode;
    file->flags = flags;
    //file->count = 1;
    file->mode = inode->desc->mode;
    file->offset = 0;

    if (flags & O_APPEND)
    {
        file->offset = file->inode->desc->size;
    }
    return fd;
}

fd_t sys_create(char *filename, int mode)
{
    return sys_open(filename, O_CREAT | O_TRUNC, mode);
}

void sys_close(fd_t fd)
{
    assert(fd < TASK_FILE_NR && fd > 2);
    TCB_t *task = (TCB_t *)current_task()->owner;
    file_t *file = task->files[fd];
    if (!file)
        return;

    assert(file->inode);
    iput(file->inode);
    free(file);
    task->files[fd] = NULL;
}

int sys_read(fd_t fd, char *buf, int count)
{
    if (fd == stdin)
    {
        device_t *device = device_find(DEV_KEYBOARD, 0);
        return device_read(device->dev, buf, count, 0, 0);
    }

    TCB_t *task = (TCB_t *)current_task()->owner;
    file_t *file = task->files[fd];
    assert(file);

    if ((file->flags & O_ACCMODE) == O_WRONLY)
        return EOF;

    m_inode *inode = file->inode;
    int len = inode_read(inode, buf, count, file->offset);
    if (len != EOF)
    {
        file->offset += len;
    }
    return len;
}

int sys_write(fd_t fd, char *buf, int count)
{
    if (fd == stdout || fd == stderr)
    {
        device_t *device = device_find(DEV_CONSOLE, 0);
        return device_write(device->dev, buf, 0, 0, 0);
    }

    TCB_t *task = (TCB_t *)current_task()->owner;
    file_t *file = task->files[fd];
    assert(file);

    if ((file->flags & O_ACCMODE) == O_RDONLY)
        return EOF;

    m_inode *inode = file->inode;
    int len = inode_write(inode, buf, count, file->offset);
    if (len != EOF)
    {
        file->offset += len;
    }

    return len;
}

int sys_lseek(fd_t fd, idx_t offset, whence_t whence)
{
    assert(fd < TASK_FILE_NR);

    TCB_t *task = (TCB_t *)current_task()->owner;
    file_t *file = task->files[fd];

    assert(file);
    assert(file->inode);

    switch (whence)
    {
    case SEEK_SET:
        assert(offset >= 0);
        file->offset = offset;
        break;
    case SEEK_CUR:
        assert(file->offset + offset >= 0);
        file->offset += offset;
        break;
    case SEEK_END:
        assert(file->inode->desc->size + offset >= 0);
        file->offset = file->inode->desc->size + offset;
        break;
    default:
        PANIC("whence not defined !!!");
        break;
    }
    return file->offset;
}

int sys_readdir(fd_t fd, dir_entry *dir, u32 count)
{
    return sys_read(fd, (char *)dir, sizeof(dir_entry));
}