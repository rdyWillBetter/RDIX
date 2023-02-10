[bits 32]
[section .text]

global port_inb,port_inw,port_outb,port_outw

port_inb:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    in al,dx

    leave
    ret

port_inw:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    in ax,dx

    leave
    ret

port_outb:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    mov eax,[ss:ebp + 12]
    out dx,al

    leave
    ret

port_outw:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    mov eax,[ss:ebp + 12]
    out dx,ax

    leave
    ret