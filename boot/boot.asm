[bits 16]
[section .text vstart=0x7c00]

loader_begin_in_memery_seg equ 0x1000
loader_begin_in_memery_off equ 0
loader_begin_in_disk equ 33

%macro DPT_t 8
db %1;AcPartition
db %2;Start_H
dw %3;Start_C_S
db %4;Type
db %5;End_H
dw %6;End_C_S
dd %7;UsedSector
dd %8;TotalSector
%endmacro

jmp near _start
;------------------------
;BS_OMEName db 'FreeRDY',0;厂商名，8Byte
;BPB_BytsPerSec dw 512;每扇区字节数，2Byte
;BPB_SecPerClus db 1;每簇扇区数，1Byte
;BPB_RsvdSecCnt dw 1;Boot记录占用多少扇区，2Byte
;BPB_NumFATs db 2;总共有多少FAT表，1Byte
;BPB_RootEntCnt dw 224;根目录文件最大数，2Byte
;BPB_TotSec16 dw 2880;扇区总数，2Byte
;BPB_Media db 0xf0;介质描述符，1Byte
;BPB_FATSz16 dw 0x9;每FAT扇区数，2Byte
;BPB_SecPerTrk dw 0x18;每磁道扇区数，2Byte
;BPB_NumHeads dw 0x2;磁头数，2Byte
;BPB_HiddSec dd 0;隐藏扇区数，4Byte
;BPB_TotSec32 dd 0;
;BS_DrvNum db 0;中断13的驱动器号，1Byte
;BS_Reserved1 db 0;
;BS_BootSig db 0x29;扩展引导标记，1Byte
;BS_VolD dd 0;卷序列号，4Byte
;BS_VolLab db 'RDYOS0.01',0,0;卷标，11Byte
;BS_FileSysType db 'FAT12',0,0,0;文件系统类型，8Byte
;-----------------------------------------------------
data_addr dd 0
_start:
    mov ax,cs
    mov ds,ax
    mov es,ax
    mov ss,ax
    mov sp,0x7c00

    mov si,boot_message
    call print

    call Read_INIT_RAW
    ;call Read_INIT_FAT32

    mov ax, loader_begin_in_memery_seg
    mov es, ax
    mov ax, 0xc88c
    cmp ax, word [es:0]
    jnz @t

    mov si,int13_message
    call print

@t:
    jmp loader_begin_in_memery_seg:loader_begin_in_memery_off

Read_Disk:
    push si
    push ax

    mov si,DiskAddressPacket
    mov ah,0x42
    int 0x13

    pop ax
    pop si
    ret

Read_INIT_RAW:
    mov word [BlockCount], 2
    mov dword [BlockNum], 15
    mov word [BufferOffset], 0
    mov word [BufferSeg], 0x1000
    mov dx, 0x80;设置硬盘读取设备号
    call Read_Disk
    ret

Read_INIT_FAT32:
    seek_the_activate_partition:
        ;xchg bx,bx;=========================
        mov di, DPT
        mov cx, 4
        is_activate_partition:
            mov bl, [di]
            cmp bl, 0x80
            je activate_partition_found
            add di, 16
            loop is_activate_partition
        activate_partition_not_found:
            mov byte [error_message + 8], '1'
            jmp Boot_End
    activate_partition_found:
        mov ebx, [di + 8]
        mov [BlockNum], ebx
        call Read_Disk
    get_first_fat:
        mov di, 0x7e00
        xor ebx, ebx
        mov bx, [di + 0x0e]     ;fat32保留扇区数
        mov eax, [di + 0x1c]    ;mbr保留扇区数
        add ebx, eax            ;ebx为fat表起始地址
    get_data_area_base:
        mov eax, [di + 0x24]    ;fat表占用扇区数
        xor cx, cx
        mov cl, [di + 0x10]     ;fat表个数
        get_fat_total_size:
            add ebx, eax
            loop get_fat_total_size
    read_root_director:
        ;xchg bx, bx;===============
        mov [data_addr], ebx    ;保存数据区起始地址，方便loader访问
        mov [BlockNum], ebx     ;数据区起始地址
        mov word [BlockCount], 8
        call Read_Disk
    seek_the_initial_bin:
        cmp dword [di], 'LOAD'
        jne next_file
        cmp dword [di + 4], 'ER  '
        jne next_file
        cmp dword [di + 8], 'BIN '
        jne next_file
        jmp initial_bin_found
        next_file:
            cmp di, 0x8e00
            jne boot_loader_not_end
            mov byte [error_message + 8], '2'
            jmp Boot_End
            boot_loader_not_end:
            add di, 32
            jmp seek_the_initial_bin
    initial_bin_found:
        mov ax, [di + 0x1c]     ;文件大小低16位
        mov dx, [di + 0x1e]     ;文件大小高16位
        mov cx, 512
        div cx
        cmp dx, 0
        je no_remainder
        inc ax                  ;文件大小，单位扇区
        mov [BlockCount], ax
        no_remainder:
            mov ax, [di + 0x1a];起始簇号低16位
            sub ax, 2
            mov cx, 4;每簇扇区数
            mul cx
            and eax, 0x0000ffff
            add ebx, eax
            mov [BlockNum], ebx
            mov word [BufferOffset], 0
            mov word [BufferSeg], 0x1000
            ;xchg bx, bx;=====================
            mov dx, 0x80;设置硬盘读取设备号
            call Read_Disk
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

Boot_End:
    mov si, error_message
    call print

    cli
    hlt

boot_message db "booting...",0x0d,0x0a,0
int13_message db "read success, ah = 0x42, sq = 15...",0x0d,0x0a,0
error_message db "[error] 0",0x0d,0x0a,0

DiskAddressPacket:
	PacketSize db 0x10
	Reserved db 0
	BlockCount dw 2 ; 要加载的扇区个数
	BufferOffset dw 0x0
	BufferSeg dw 0x1000
	BlockNum dd 15 ; 要加载的扇区逻辑起始位置
	dd 0

times 440-($-$$) db 0
DiskCode dd 0x28BE434D
Rev dw 0
DPT:
    DPT_t 0x80, 0, 2, 0xc, 0x81, 0x2022, 1, 0x7F800
    DPT_t 0, 0, 0, 0, 0, 0, 0, 0
    DPT_t 0, 0, 0, 0, 0, 0, 0, 0
    DPT_t 0, 0, 0, 0, 0, 0, 0, 0

mbr_label dw 0xaa55

;FAT32 头

;jmp temp
;nop
;OEMName db "MSDOS5.0"
;BytsPerSec dw 512
;SecPerClus db 4
;RsvdSecCnt dw 0x1826
;NumFATs db 2
;RootEntCnt dw 0
;TotSec16 dw 0
;Media db 0xf8
;FATSz16 dw 0
;SecPerTrk dw 0x3f
;NumHeads dw 0xff
;HiddSec dd 1
;TotSec32 dd 0x7f800
;FATSz32 dd 0x3ed
;ExtFlags dw 0
;FSVers dw 0
;RootClus dd 2
;FSInfo dw 1
;BkBootSec dw 6
;Reserved1 dd 0, 0, 0
;DrvNum db 0x80
;Reserved2 db 0
;BootSig db 0x29
;VolID dd 0x8446500c
;VolLab db "NO NAME    "
;FileSysType db "FAT32   "
;temp

;imes 1022-($-$$) db 0
;Dbr_label dw 0xaa55
