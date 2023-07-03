#include <fs/fs.h>
#include <rdix/kernel.h>
#include <common/assert.h>
#include <common/string.h>
#include <common/interrupt.h>
#include <rdix/task.h>

#define BLOCK_DENTRYS (BLOCK_SIZE / sizeof(dir_entry))

#define IS_SEPARATOR(ch) ((ch) == '/' || (ch) == '\\')

static bool _match(const char *name, const char *entry_name){
    while (*name && *entry_name && *name == *entry_name){
        name++;
        entry_name++;
    }

    if (*entry_name)
        return false;
    if (!(*name) || IS_SEPARATOR(*name))
        return true;
    return false;
}

buffer_t *find_entry(m_inode *dir, const char *name, dir_entry **res){
    assert(ISDIR(dir->desc->mode));

    if (!name)
        goto _fail;

    buffer_t *buf = NULL;
    idx_t block = 0;
    dir_entry *dentry = NULL;
    int entry_cnt = dir->desc->size / sizeof(dir_entry);
    int zone_idx = 0;

/* 没有查找一级索引和二级索引，只是吧直接索引块给找了一下 */
    for (int i = 0; i < entry_cnt; ++i){
        if (i >= (zone_idx + 1) * BLOCK_DENTRYS){
            assert(++zone_idx < 7);

            assert(buf);
            brelse(buf);
            buf = NULL;

            block = 0;
        }

        if (block == 0)
            block = bmap(dir, i / BLOCK_DENTRYS, false);
        
        assert(block);
        
        if (buf == NULL)
            buf = bread(dir->dev, block);
        
        assert(buf);

        dentry = (dir_entry *)buf->b_data;

        if (dentry[i].inode == 0)
            continue;
        
        if (_match(name, dentry[i].name)){
            *res = dentry + i;
            return buf;
        }
    }
_fail:
    *res = NULL;
    return NULL;
}

static bool vaild_file_name(const char *fname){
    while (*fname){
        if (IS_SEPARATOR(*fname))
            return false;
        ++fname;
    }
    return true;
}

/* 在 dir 目录中添加 name 目录项 */
buffer_t *add_entry(m_inode *dir, const char *name, dir_entry **res){
    assert(ISDIR(dir->desc->mode));
    assert(vaild_file_name(name));

    buffer_t *bf = NULL;
    if (bf = find_entry(dir, name, res))
        return bf;

    dir_entry *dentry = NULL;
    int entry_cnt = dir->desc->size / sizeof(dir_entry);
    int zone_idx = 0;
    bool create = false;
    idx_t block = 0;    // 文件块索引号

    int i = 0;
    for (; i < entry_cnt + 1; ++i){
        if (i == entry_cnt)
            create = true;

        if (bf)
            brelse(bf);

        if (zone_idx <= i / ZONES_IDX_PER_BLOCK){
            block = bmap(dir, i / ZONES_IDX_PER_BLOCK, create);
            bf = bread(dir->dev, block);
            zone_idx += 1;
            dentry = (dir_entry *)bf->b_data;
        }
        assert(block);

        if (dentry[i % ZONES_IDX_PER_BLOCK].inode == 0){
            *res = dentry + i;
            break;
        }
    }

    ATOMIC_OPS(
    strncpy(dentry[i % ZONES_IDX_PER_BLOCK].name, name, NAME_LEN);
    bf->b_dirty = true;
    dir->desc->size = (i + 1) * sizeof(dir_entry);
    dir->buf->b_dirty = true;)

    return bf;
}

static char *next_name(const char *path){
    if (!(*path))
        return NULL;
    if (IS_SEPARATOR(*path))
        ++path;
    
    while (*path && !IS_SEPARATOR(*path))
        ++path;
    
    if (*path)
        ++path;

    return path;
}

// 获取 pathname 对应的 inode，需要使用 iput 释放
// dorc == false 时获取父节点
static m_inode *_namei(const char *pathname, bool dorc){
    const char* ptr = pathname;
    m_inode *node = NULL;
    TCB_t *task = (TCB_t *)current_task()->owner;

    if (IS_SEPARATOR(ptr[0])){
        node = task->i_root;
        ++ptr;
    }
    else if (!ptr[0])
        return NULL;
    else
        node = task->i_pwd;

    ++node->count;   // 需要操作该节点的时候就要标记，并且可以对应下面的 iput
    
    dir_entry *dentry = NULL;
    buffer_t *bf = NULL;

    while(ptr && *ptr){
        assert(ISDIR(node->desc->mode));
        bf = find_entry(node, ptr, &dentry);
        ptr = next_name(ptr);

        if (!(*ptr) && !dorc)
            break;

        if (!bf){
            iput(node);
            return NULL;
        }

        dev_t dev = node->dev;
        iput(node);
        node = iget(dev, dentry->inode);
        brelse(bf);
    }

    return node;
}

m_inode *namei(const char *pathname){
    return _namei(pathname, true);
}

static char *filename(const char *path){
    const char *prev = path, *next = next_name(path);

    while (*next){
        prev = next;
        next = next_name(next);
    }

    if (IS_SEPARATOR(*prev))
        ++prev;
    
    if (!(*prev))
        return NULL;

    return prev;
}

int sys_mkdir(const char *pathname, int mode){
    char *childname = filename(pathname);
    m_inode *dir = _namei(pathname, false);

    if (!dir)
        PANIC("path not exist\n");

    assert(ISDIR(dir->desc->mode));

    buffer_t *bf = NULL;
    dir_entry *res = NULL;

    if (find_entry(dir, childname, &res))
        PANIC("file or directory is existing\n");
    
    bf = add_entry(dir, childname, &res);
    res->inode = ialloc(dir->dev);
    bf->b_dirty = true;

    m_inode *childnode = iget(dir->dev, res->inode);
    TCB_t *task = (TCB_t *)current_task()->owner;

    childnode->buf->b_dirty = true;
    childnode->desc->gid = task->gid;
    childnode->desc->uid = task->uid;
    childnode->desc->mode = (mode & 0777 & ~task->umask) | IFDIR;
    childnode->desc->size = sizeof(dir_entry) * 2;
    childnode->desc->nlinks = 2;

    dir->buf->b_dirty = true;
    dir->desc->nlinks++;

    brelse(bf);

    bf = bread(childnode->dev, bmap(childnode, 0, true));
    bf->b_dirty = true;

    res = (dir_entry *)bf->b_data;

    strcpy(res[0].name, ".");
    res[0].inode = childnode->nr;

    strcpy(res[1].name, "..");
    res[1].inode = dir->nr;

    iput(dir);
    iput(childnode);

    sync_dev(dir->dev);

    brelse(bf);

    return 0;
}

bool dir_is_empty(m_inode *dir){
    assert(ISDIR(dir->desc->mode));

    int entries = dir->desc->size / sizeof(dir_entry);
    if (entries < 2 || !dir->desc->zone[0])
    {
        PANIC("bad directory\n");
    }

    idx_t block = 0;
    idx_t zone_cnt = 0;
    buffer_t *bf = NULL;
    dir_entry *entry = NULL;
    int count = 0;

    for (int i = 0; i < entries; ++i){
        if (i >= zone_cnt * (BLOCK_SIZE / sizeof(dir_entry))){
            block = bmap(dir, zone_cnt, false);
            ++zone_cnt;

            if (bf)
                brelse(bf);
            
            bf = bread(dir->dev, block);
            assert(bf);
            entry = (dir_entry *)bf->b_data;
        }

        if (entry[i % DIR_ENTRY_PER_BLOCK].inode){
            ++count;
            continue;
        }

        if(i == 0 || i == 1)
            PANIC("bad directory\n");
    }
    assert(bf);
    brelse(bf);

    return count == 2;
}

int sys_rmdir(const char *pathname){
    char *childname = filename(pathname);
    m_inode *dir = _namei(pathname, false);
    m_inode *node = NULL;
    int status = EOF;
    buffer_t *bf = NULL;
    dir_entry *res = NULL;

    if (childname)
        goto rollback;

    if (!dir)
        goto rollback;

    bf = find_entry(dir, childname, &res);

    if (!bf)
        return EOF;
    
    node = iget(dir->dev, res->inode);
    assert(node);

    if (!ISDIR(node->desc->mode))
        return EOF;

    TCB_t *task = (TCB_t *)current_task()->owner;
    if((dir->desc->mode & ISVTX) && task->uid != node->desc->uid)
        return EOF;
    
    if (dir->dev != node->dev || node->count > 1)
        return EOF;

    if (!dir_is_empty(node))
        return EOF;
    
    assert(node->desc->nlinks == 2);

    inode_truncate(node);
    ifree(node->dev, node->nr);

    node->desc->nlinks = 0;
    node->buf->b_dirty = true;
    node->nr = 0;

    dir->desc->nlinks--;
    //dir->ctime = dir->atime = dir->desc->mtime = time();
    dir->buf->b_dirty = true;
    assert(dir->desc->nlinks >= 2);

    res->inode = 0;
    bf->b_dirty = true;

    status = 0;

rollback:
    if(dir) iput(dir);
    if(node) iput(node);
    if(bf) brelse(bf);
    return status;
}

int sys_link(char *oldname, char *newname){
    int ret = EOF;
    buffer_t *buf = NULL;
    m_inode *dir = NULL;
    m_inode *inode = namei(oldname);
    if (!inode)
        goto rollback;

    if (ISDIR(inode->desc->mode))
        goto rollback;

    char *fname = filename(newname);
    dir = _namei(newname, false);
    if (!dir)
        goto rollback;

    if (!fname)
        goto rollback;

    if (dir->dev != inode->dev)
        goto rollback;

    /* if (!permission(dir, P_WRITE))
        goto rollback; */

    dir_entry *entry = NULL;

    buf = find_entry(dir, fname, &entry);
    if (buf) // 目录项存在
        goto rollback;

    buf = add_entry(dir, fname, &entry);
    entry->inode = inode->nr;
    buf->b_dirty = true;

    inode->desc->nlinks++;
    //inode->ctime = time();
    inode->buf->b_dirty = true;
    ret = 0;

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}

int sys_unlink(char *pathname){
    int ret = EOF;
    m_inode *inode = NULL;
    buffer_t *buf = NULL;
    m_inode *dir = _namei(pathname, false);
    if (!dir)
        goto rollback;

    /* if (!permission(dir, P_WRITE))
        goto rollback; */

    char *fname = filename(pathname);
    dir_entry *entry = NULL;
    buf = find_entry(dir, fname, &entry);
    if (!buf) // 目录项不存在
        goto rollback;

    inode = iget(dir->dev, entry->inode);
    if (ISDIR(inode->desc->mode))
        goto rollback;

    TCB_t *task = (TCB_t *)current_task()->owner;
    if ((inode->desc->mode & ISVTX) && task->uid != inode->desc->uid)
        goto rollback;

    if (!inode->desc->nlinks)
    {
        PANIC("deleting non exists file (%04x:%d)\n",
             inode->dev, inode->nr);
    }

    entry->inode = 0;
    buf->b_dirty = true;

    if (inode->desc->nlinks)
        inode->desc->nlinks--;
    inode->buf->b_dirty = true;

    if (inode->desc->nlinks == 0)
    {
        inode_truncate(inode);
        ifree(inode->dev, inode->nr);
    }

    ret = 0;

rollback:
    brelse(buf);
    iput(inode);
    iput(dir);
    return ret;
}

m_inode *inode_open(char *pathname, int flag, int mode)
{
    m_inode *dir = NULL;
    m_inode *inode = NULL;
    buffer_t *buf = NULL;
    dir_entry *entry = NULL;
    char *fname = filename(pathname);
    dir = _namei(pathname, false);
    if (!dir)
        goto rollback;

    if (!fname)
        return dir;

    /* if ((flag & O_TRUNC) && ((flag & O_ACCMODE) == O_RDONLY))
        flag |= O_RDWR; */

    buf = find_entry(dir, fname, &entry);
    if (buf)
    {
        inode = iget(dir->dev, entry->inode);
        goto makeup;
    }

    if (!(flag & O_CREAT))
        goto rollback;

    /* if (!permission(dir, P_WRITE))
        goto rollback; */

    buf = add_entry(dir, fname, &entry);
    entry->inode = ialloc(dir->dev);
    inode = iget(dir->dev, entry->inode);
    buf->b_dirty = true;

    TCB_t *task = (TCB_t *)current_task()->owner;

    mode &= (0777 & ~task->umask);
    mode |= IFREG;

    inode->desc->uid = task->uid;
    inode->desc->gid = task->gid;
    inode->desc->mode = mode;
    //inode->desc->mtime = time();
    inode->desc->size = 0;
    inode->desc->nlinks = 1;
    inode->buf->b_dirty = true;

makeup:
    /* if (!permission(inode, flag & O_ACCMODE))
        goto rollback; */

    if (ISDIR(inode->desc->mode) && ((flag & O_ACCMODE) != O_RDONLY))
        goto rollback;

    //inode->atime = time();

    if (flag & O_TRUNC)
        inode_truncate(inode);

    brelse(buf);
    iput(dir);
    return inode;

rollback:
    brelse(buf);
    iput(dir);
    if (inode)
        iput(inode);
    return NULL;
}

/* 获得最后一个分隔符位置，不包含末尾 */
char *strrsep(const char *str){
    char *prev = NULL;

    while(*str){
        if (IS_SEPARATOR(*str) && str[1])
            prev = str;
        ++str;
    }
    return prev;
}

/* 获得第一个分隔符位置 */
char *strsep(const char *str){
    while(*str){
        if (IS_SEPARATOR(*str))
            return str;
        ++str;
    }
    return NULL;
}

/* 在尾部添加分隔符 */
void addsep(char *str){
    size_t len = length(str);
    if (IS_SEPARATOR(str[len - 1]))
        return;

    str[len] = '/';
    str[len + 1] = '\0';
}

void abspath(char *oldpath, char *newpath){
    addsep(newpath);
        
    if (IS_SEPARATOR(*newpath)){
        strcpy(oldpath, newpath);
        return;
    }
        
    if (strcmp(newpath, "..", 2)){
        char *old = NULL, *new = NULL;
        if (length(oldpath) != 1)
            old = strrsep(oldpath) + 1;
        else
            old = oldpath + 1;
        if (length(newpath) > 3)
            new = newpath + 3;
        if (new)
            strcpy(old, new);
        else
            *old = '\0';
        return;
    }

    if (strcmp(newpath, ".", 1)){
        char *new = NULL;
        if (length(newpath) > 2)
            new = newpath + 2;
        if (new)
            strcpy(oldpath[length(oldpath)], new);
        return;
    }

    strcpy(oldpath + length(oldpath), newpath);
    addsep(newpath);
}

int sys_chdir(char *pathname)
{
    TCB_t *task = (TCB_t *)current_task()->owner;
    m_inode *inode = namei(pathname);
    if (!inode)
        return EOF;
    if (!ISDIR(inode->desc->mode) || inode == task->i_pwd)
        goto rollback;

    abspath(task->pwd, pathname);

    iput(task->i_pwd);
    task->i_pwd = inode;
    return 0;

rollback:
    iput(inode);
    return EOF;
}

void sys_getpwd(char *buf, size_t len){
    TCB_t *task = (TCB_t *)current_task()->owner;

    strncpy(buf, task->pwd, len);
}