#include <rdix/syscall.h>
#include <common/string.h>
#include <common/stdlib.h>
#include <common/assert.h>
#include <fs/fs.h>
#include <rdix/kernel.h>

#define MAX_CMD_LEN 256
#define MAX_ARG_NR 16
#define MAX_PATH_LEN 1024
#define BUFLEN 1024

static char cwd[MAX_PATH_LEN];
static char cmd[MAX_CMD_LEN];
static char *argv[MAX_ARG_NR];
static char buf[BUFLEN];

static char onix_logo[][52] = {
    "                                ____       _       \n",
    "                               / __ \\___  (_)_ __  \n",
    "                              / /_/ / _ \\/ /\\ \\ /  \n",
    "                              \\____/_//_/_//_\\_\\  \n\0",
};

extern char *strsep(const char *str);
extern char *strrsep(const char *str);

void print_prompt()
{
    getpwd(cwd, MAX_PATH_LEN);
    printf("\033[0;37;44]" "[root]" "\033[0;30;45]" "%s>" "\033[0]" " ", cwd);
}

void builtin_logo()
{
    //clear();
    printf((char *)onix_logo);
}

void builtin_test(int argc, char *argv[])
{
    test();
}

void builtin_pwd()
{
    getpwd(cwd, MAX_PATH_LEN);
    printf("%s\n", cwd);
}

void builtin_clear()
{
    //clear();
}

void builtin_ls()
{
    fd_t fd = open(cwd, O_RDONLY, 0);
    if (fd == EOF)
        return;
    lseek(fd, 0, SEEK_SET);
    dir_entry entry;
    while (true)
    {
        int len = readdir(fd, &entry, 1);
        if (len == EOF)
            break;
        if (!entry.inode)
            continue;
        /* if (!strcmp(entry.name, ".") || !strcmp(entry.name, ".."))
        {
            continue;
        } */
        printf("%s ", entry.name);
    }
    printf("\n");
    close(fd);
}

void builtin_cd(int argc, char *argv[])
{
    if (chdir(argv[1]) == EOF)
        printf("directory %s not exist\n", argv[1]);
}

void builtin_cat(int argc, char *argv[])
{
    fd_t fd = open(argv[1], O_RDONLY, 0);
    if (fd == EOF)
    {
        printf("file %s not exists.\n", argv[1]);
        return;
    }

    while (true)
    {
        int len = read(fd, buf, BUFLEN);
        if (len == EOF)
        {
            break;
        }
        write(stdout, buf, len);
    }
    close(fd);
}

void builtin_mkdir(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    mkdir(argv[1], 0755);
}

void builtin_rmdir(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    rmdir(argv[1]);
}

void builtin_rm(int argc, char *argv[])
{
    if (argc < 2)
    {
        return;
    }
    unlink(argv[1]);
}

static void execute(int argc, char *argv[])
{
    char *line = argv[0];
    if (strcmp(line, "test", 10))
    {
        return builtin_test(argc, argv);
    }
    if (strcmp(line, "logo", 10))
    {
        return builtin_logo();
    }
    if (strcmp(line, "pwd", 10))
    {
        return builtin_pwd();
    }
    if (strcmp(line, "clear", 10))
    {
        return builtin_clear();
    }
    if (strcmp(line, "exit", 10))
    {
        int code = 0;
        if (argc == 2)
        {
            //code = atoi(argv[1]);
        }
        exit(code);
    }
    if (strcmp(line, "ls", 10))
    {
        return builtin_ls();
    }
    if (strcmp(line, "cd", 10))
    {
        return builtin_cd(argc, argv);
    }
    if (strcmp(line, "cat", 10))
    {
        return builtin_cat(argc, argv);
    }
    if (strcmp(line, "mkdir", 10))
    {
        return builtin_mkdir(argc, argv);
    }
    if (strcmp(line, "rmdir", 10))
    {
        return builtin_rmdir(argc, argv);
    }
    if (strcmp(line, "rm", 10))
    {
        return builtin_rm(argc, argv);
    }
    printf("osh: command not found: %s\n", argv[0]);
}

void readline(char *buf, u32 count)
{
    assert(buf != NULL);
    char *ptr = buf;
    u32 idx = 0;
    char ch = 0;
    while (idx < count)
    {
        ptr = buf + idx;
        int ret = read(stdin, ptr, 1);
        if (ret == -1)
        {
            *ptr = 0;
            return;
        }
        switch (*ptr)
        {
        case '\n':
        case '\r':
            *ptr = 0;
            ch = '\n';
            write(stdout, &ch, 1);
            return;
        case '\b':
            if (buf[0] != '\b')
            {
                idx--;
                ch = '\b';
                write(stdout, &ch, 1);
            }
            break;
        case '\t':
            continue;
        default:
            write(stdout, ptr, 1);
            idx++;
            break;
        }
    }
    buf[idx] = '\0';
}

static int cmd_parse(char *cmd, char *argv[], char token)
{
    assert(cmd != NULL);

    char *next = cmd;
    int argc = 0;
    while (*next && argc < MAX_ARG_NR)
    {
        while (*next == token)
        {
            next++;
        }
        if (*next == 0)
        {
            break;
        }
        argv[argc++] = next;
        while (*next && *next != token)
        {
            next++;
        }
        if (*next)
        {
            *next = 0;
            next++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

int osh_main()
{
    memset(cmd, 0, sizeof(cmd));
    memset(cwd, 0, sizeof(cwd));

    builtin_logo();

    while (true)
    {
        print_prompt();
        readline(cmd, sizeof(cmd));
        if (cmd[0] == 0)
        {
            continue;
        }
        int argc = cmd_parse(cmd, argv, ' ');
        if (argc < 0 || argc >= MAX_ARG_NR)
        {
            continue;
        }
        execute(argc, argv);
    }
    return 0;
}