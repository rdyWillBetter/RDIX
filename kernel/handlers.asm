[bits 32]
[section .text]

global handler_table, interrupt_exit
extern interrupt_func_table

%macro SYS_EXCEPTION 2
SYS_INTERRUPT_%1:
%ifn %2
    push 0x01011017 ;RDIX_MAGIC
%endif
    push %1
    jmp interrupt_handler
%endmacro

interrupt_handler:
    push ds
    push es
    push fs
    push gs
    pusha

    mov eax, [ss:esp + 12 * 4]  ;向量号
    mov ebx, [ss:esp + 13 * 4]  ;错误码

    push ebx
    push eax
    call [interrupt_func_table + eax * 4]
    add esp,8

interrupt_exit:
    popa
    pop gs
    pop fs
    pop es
    pop ds

    add esp,8
    ;xchg bx,bx
    iret

SYS_EXCEPTION 0x0, 0
SYS_EXCEPTION 0x1, 0
SYS_EXCEPTION 0x2, 0
SYS_EXCEPTION 0x3, 0
SYS_EXCEPTION 0x4, 0
SYS_EXCEPTION 0x5, 0
SYS_EXCEPTION 0x6, 0
SYS_EXCEPTION 0x7, 0
SYS_EXCEPTION 0x8, 0
SYS_EXCEPTION 0x9, 0
SYS_EXCEPTION 0xa, 1
SYS_EXCEPTION 0xb, 1
SYS_EXCEPTION 0xc, 1
SYS_EXCEPTION 0xd, 1
SYS_EXCEPTION 0xe, 1
SYS_EXCEPTION 0xf, 0
SYS_EXCEPTION 0x10, 0
SYS_EXCEPTION 0x11, 1
SYS_EXCEPTION 0x12, 0
SYS_EXCEPTION 0x13, 0
SYS_EXCEPTION 0x14, 0
SYS_EXCEPTION 0x15, 1
SYS_EXCEPTION 0x16, 0
SYS_EXCEPTION 0x17, 0
SYS_EXCEPTION 0x18, 0
SYS_EXCEPTION 0x19, 0
SYS_EXCEPTION 0x1a, 0
SYS_EXCEPTION 0x1b, 0
SYS_EXCEPTION 0x1c, 0
SYS_EXCEPTION 0x1d, 0
SYS_EXCEPTION 0x1e, 0
SYS_EXCEPTION 0x1f, 0
SYS_EXCEPTION 0x20, 0
SYS_EXCEPTION 0x21, 0
SYS_EXCEPTION 0x22, 0
SYS_EXCEPTION 0x23, 0
SYS_EXCEPTION 0x24, 0
SYS_EXCEPTION 0x25, 0
SYS_EXCEPTION 0x26, 0
SYS_EXCEPTION 0x27, 0
SYS_EXCEPTION 0x28, 0
SYS_EXCEPTION 0x29, 0
SYS_EXCEPTION 0x2a, 0
SYS_EXCEPTION 0x2b, 0
SYS_EXCEPTION 0x2c, 0
SYS_EXCEPTION 0x2d, 0
SYS_EXCEPTION 0x2e, 0
SYS_EXCEPTION 0x2f, 0

[section .data]
handler_table:
    dd SYS_INTERRUPT_0x0,
    dd SYS_INTERRUPT_0x1,
    dd SYS_INTERRUPT_0x2,
    dd SYS_INTERRUPT_0x3,
    dd SYS_INTERRUPT_0x4,
    dd SYS_INTERRUPT_0x5,
    dd SYS_INTERRUPT_0x6,
    dd SYS_INTERRUPT_0x7,
    dd SYS_INTERRUPT_0x8,
    dd SYS_INTERRUPT_0x9,
    dd SYS_INTERRUPT_0xa,
    dd SYS_INTERRUPT_0xb,
    dd SYS_INTERRUPT_0xc,
    dd SYS_INTERRUPT_0xd,
    dd SYS_INTERRUPT_0xe,
    dd SYS_INTERRUPT_0xf,
    dd SYS_INTERRUPT_0x10,
    dd SYS_INTERRUPT_0x11,
    dd SYS_INTERRUPT_0x12,
    dd SYS_INTERRUPT_0x13,
    dd SYS_INTERRUPT_0x14,
    dd SYS_INTERRUPT_0x15,
    dd SYS_INTERRUPT_0x16,
    dd SYS_INTERRUPT_0x17,
    dd SYS_INTERRUPT_0x18,
    dd SYS_INTERRUPT_0x19,
    dd SYS_INTERRUPT_0x1a,
    dd SYS_INTERRUPT_0x1b,
    dd SYS_INTERRUPT_0x1c,
    dd SYS_INTERRUPT_0x1d,
    dd SYS_INTERRUPT_0x1e,
    dd SYS_INTERRUPT_0x1f,
    dd SYS_INTERRUPT_0x20,
    dd SYS_INTERRUPT_0x21,
    dd SYS_INTERRUPT_0x22,
    dd SYS_INTERRUPT_0x23,
    dd SYS_INTERRUPT_0x24,
    dd SYS_INTERRUPT_0x25,
    dd SYS_INTERRUPT_0x26,
    dd SYS_INTERRUPT_0x27,
    dd SYS_INTERRUPT_0x28,
    dd SYS_INTERRUPT_0x29,
    dd SYS_INTERRUPT_0x2a,
    dd SYS_INTERRUPT_0x2b,
    dd SYS_INTERRUPT_0x2c,
    dd SYS_INTERRUPT_0x2d,
    dd SYS_INTERRUPT_0x2e,
    dd SYS_INTERRUPT_0x2f

extern syscall_table;主体方法函数
extern syscall_check;确认调用号 eax 是否正确

;保存在 idt 表中，由 int 指令直接调用
;主要工作是保护现场，调用主体函数
global syscall_handle

;输入为 eax 到 edx 四个寄存器
;eax为系统调用号
;返回值保存在 eax
syscall_handle:
    push eax
    call syscall_check
    add esp, 4

    push ds
    push es
    push fs
    push gs  
    pusha

    ;系统调用号 0x80
    push 0x80;第四个参数
    push edx;第三个参数
    push ecx;第二个参数
    push ebx;第一个参数

    call [syscall_table + eax * 4]
    add esp, 4 * 4

    mov [esp + 7 * 4], eax

    popa
    pop gs
    pop fs
    pop es
    pop ds

    iret
