[bits 32]
[section .text]

global port_inb,port_inw,port_outb,port_outw

port_inb:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    in al,dx

    call delay

    leave
    ret

port_inw:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    in ax,dx

    call delay

    leave
    ret

port_outb:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    mov eax,[ss:ebp + 12]
    out dx,al

    call delay

    leave
    ret

port_outw:
    push ebp
    mov ebp,esp

    mov edx,[ss:ebp + 8]
    mov eax,[ss:ebp + 12]
    out dx,ax

    call delay

    leave
    ret

delay:
    nop
    nop
    nop
    
    ret