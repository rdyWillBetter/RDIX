[bits 16]
[section text]
;文件中 虚拟起始地址（vstart） 均为0。16位代码中ds设置位0x1000，
;所以不需要加偏移量0x10000
;32位代码中ds选择的段描述符起始地址为 0 ，所以需要加偏移量0x10000


Set_G equ 0x8000;粒度位，0——单位为B，1——单位为4KB
Set_D equ 0x4000;0——16位代码段，1——32位代码段
Set_L equ 0x2000;1——64位
Set_P equ 0x0080;该段是否存在，1——存在
Set_S equ 0x0010;0——系统段，1——数据段，代码段
Set_X equ 0x0008;0——数据段，1——代码段
Set_E equ 0x0004;0——该段向上增长，1——该段向下增长
Set_W equ 0x0002;是否可写，1——可写

;输入顺序为，dd 段基址，dw 段界限 (段界限长度只有 20 位)，dd 段描述符
%macro Descriptor 3
dw %2 & 0xffff
dw %1 & 0xffff
db (%1 >> 16) & 0xff
dw ((%2 >> 8) & 0x0f00) | (%3 & 0xf0ff)
db (%1 >> 24) & 0xff
%endmacro

code_des equ (1 << 3)
data_des equ (2 << 3)


_start:
    mov ax,cs;cs=0x1000
    mov ds,ax
    mov si,loader_message
    call print

    ;call Read_INIT_FAT32
    call Read_Disk

memory_detect:
    mov ax,cs
    mov es,ax
    mov di,mem_info
    xor ebx,ebx
    mov edx,0x534d4150

m_next:
    mov eax,0xe820
    mov ecx,20
    int 0x15
    jc error
    add di,cx
    add dword [mem_info_count], 1
    cmp ebx, 0
    jne m_next
    jmp prepare_protected_mode

error:
    mov si, error_message
    call print
    
    cli
    hlt

prepare_protected_mode:
    cli

    in al,0x92
    or al,2
    out 0x92,al

    lgdt [gdtr]

    mov eax,cr0
    or eax,1
    mov cr0,eax

    ;当前 cs 描述符缓存中的值没有改变，因此当前代码段还是实模式下的 16 位代码段
    ;并且当前段有 [bits 16] 前缀，所以必须加上 dword
    jmp dword code_des:(protected_mode + 0x10000)

Read_INIT_FAT32:
    xor ax, ax
    mov es, ax
    mov di, 0x7e00
    mov ebx, [es:0x7c03]
    seek_the_initial_bin:
        cmp dword [es:di], 'KERN'
        jne next_file
        cmp dword [es:di + 4], 'EL  '
        jne next_file
        cmp dword [es:di + 8], 'BIN '
        jne next_file
        jmp initial_bin_found
        next_file:
            cmp di, 0x8e00
            jne boot_loader_not_end
            mov byte [error_loader + 16], '1'
            jmp Load_End
            boot_loader_not_end:
            add di, 32
            jmp seek_the_initial_bin
    initial_bin_found:
        mov ax, [es:di + 0x1c]     ;文件大小低16位
        mov dx, [es:di + 0x1e]     ;文件大小高16位
        mov cx, 512
        div cx
        cmp dx, 0
        je no_remainder
        inc ax                  ;文件大小，单位扇区
        mov [BlockCount], ax
        no_remainder:
            mov ax, [es:di + 0x1a];起始簇号低16位
            sub ax, 2
            mov cx, 4;每簇扇区数
            mul cx
            and eax, 0x0000ffff
            add ebx, eax
            mov [BlockNum], ebx
            mov word [BufferOffset], 0
            mov word [BufferSeg], 0x2000
            ;xchg bx, bx;=====================
            mov dx, 0x80;设置硬盘读取设备号
            call Read_Disk
    ret

Load_End:
    mov si,error_loader
    call print

    cli
    hlt

Read_Disk:
    push si
    push ax

    mov si,DiskAddressPacket
    mov ah,0x42
    int 0x13

    pop ax
    pop si
    ret

print:
    mov ah,0xe
    .next:
        mov al,[si]
        cmp al,0
        je .end
        int 0x10
        inc si
        jmp .next
    .end:
        ret

[bits 32]

LOADER_OS_MAGIC equ 0x01011017  ;系统魔数，用来辨别 rdix 是通过 grub 启动还是这个 loader 文件启动

protected_mode:
    ;平坦模型
    ;警告
    ;遇到需要将 es 压栈出栈的场景时，因为 es 的值是实模式下设置的值 0x1000，
    ;而保护模式下没有设置该段选择子，导致在出栈设置时触发 GP 异常
    mov ax, data_des
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, 0x20000

    mov ebx, mem_info_count + 0x10000   ;ebx 指向 int 0x13 返回的内存检测结果
    mov eax, LOADER_OS_MAGIC

    jmp code_des: 0x20040

loader_message db "loader success, by int 0x13",0x0d,0x0a,0
error_message db "memory detecting error",0x0d,0x0a,0
error_loader db "[loader error]  ", 0

gdt_base:
    Descriptor 0,0,0
    Descriptor 0,0xfffff,Set_D | Set_P | Set_S | Set_X | Set_G;代码段
    Descriptor 0,0xfffff,Set_D | Set_P | Set_S | Set_W | Set_G;数据段
gdt_end:

gdtr:
    gdtr_size dw ((gdt_end-gdt_base)-1)
	gdtr_base dd (gdt_base+0x10000)

DiskAddressPacket:
	PacketSize db 0x10
	Reserved db 0
	BlockCount dw 0x40
	BufferOffset dw 0
	BufferSeg dw 0x2000
	BlockNum dq 20
	dq 0

mem_info_count:
    dd 0
mem_info: