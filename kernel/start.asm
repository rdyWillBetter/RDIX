[bits 32]
;这个魔数是 os 用来给 grub 识别的
;当进入 os 后，grub 会从 eax 传入另外一个魔数让 os 识别
OS_MULTIBOOT_MAGIC equ 0xE85250D6

[section multiboot2]

magic dd OS_MULTIBOOT_MAGIC
architecture dd 0
header_length dd 64
checksum dd -(OS_MULTIBOOT_MAGIC + 0 + 64)

;终止标签
type dw 0
_flags db 0
size dw 8

times 64-($-$$) db 0

[section .text]
extern kernel_init

start:
    mov esp, 0x20000
    push ebx
    push eax;系统魔数，目前没有用上
    call kernel_init
    
    jmp $ 
