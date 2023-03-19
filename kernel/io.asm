[bits 32]
[section .text]

;别忘记导出，asm 文件是需要将函数导出后其他文件才能链接到的
global port_inb,port_inw,port_ind,port_outb,port_outw,port_outd
global iodelay

port_inb:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    in al,dx

    call iodelay

    leave
    ret

port_inw:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    in ax,dx

    call iodelay

    leave
    ret

port_ind:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    in eax,dx

    call iodelay

    leave
    ret

port_outb:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    mov eax,[ss:ebp + 12]
    out dx,al

    call iodelay

    leave
    ret

port_outw:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    mov eax,[ss:ebp + 12]
    out dx,ax

    call iodelay

    leave
    ret

port_outd:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    mov eax,[ss:ebp + 12]
    out dx,eax

    call iodelay

    leave
    ret

iodelay:
    nop
    nop
    nop
    
    ret