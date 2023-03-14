BUILD=../build
SRC=.
GRUBCFG=.
ISODIR=../iosdir

MULTIBOOT2=0x20000
KERNELSTARTPOINT=0x20040

CC=gcc
CFLAGS=-m32 \
-fno-builtin \
-nostdinc \
-fno-pic \
-fno-pie \
-nostdlib \
-nostdinc \
-fno-stack-protector \
-g \
-ffreestanding

#	nostdinc
#项目中的头文件可能会和开发环境中的系统头文件文件名称发生冲突，
#采用这个编译选项时不检索系统默认的头文件目录，
#但之后需要手动添加需检索的提供独立环境头文件的目录。

INCLUDES=-I ./include

LD=ld
LFLAGS=-m elf_i386 -static -Ttext $(KERNELSTARTPOINT) --section-start=multiboot2=$(MULTIBOOT2) -e start

FILE:=$(wildcard $(SRC)/kernel/*.c)
FILE:=$(notdir $(FILE))
OBJ:=$(patsubst %.c, $(BUILD)/%.o, $(FILE))

all: vmdk $(BUILD)/master.img $(BUILD)/rdix.iso

.PHONY: test
test:
	$(shell echo $(TEST) > readme.txt)

$(BUILD)/%.bin: $(SRC)/boot/%.asm
	mkdir -p $(dir $@)
	nasm -f bin $^ -o $@

$(BUILD)/%.o: $(SRC)/kernel/%.asm
	nasm -f  elf32 $^ -o $@ -g

$(BUILD)/%.o: $(SRC)/kernel/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@

$(BUILD)/rdix.bin: $(BUILD)/start.o \
					$(BUILD)/io.o \
					$(BUILD)/handlers.o \
					$(OBJ)

	$(LD) $(LFLAGS) $^ -o $@
	nm -n $(BUILD)/rdix.bin > $(BUILD)/sympol.txt

$(BUILD)/kernel.bin: $(BUILD)/rdix.bin
	objcopy -O binary $(BUILD)/rdix.bin $@

$(BUILD)/master.img: $(BUILD)/boot.bin \
					$(BUILD)/loader.bin \
					$(BUILD)/kernel.bin

	yes | bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $@

	test -n "$$(find $(BUILD)/kernel.bin -size -100k)"

	dd if=$(BUILD)/boot.bin of=$@ bs=512 count=2 conv=notrunc
	dd if=$(BUILD)/loader.bin of=$@ bs=512 count=1 seek=15 conv=notrunc
	dd if=$(BUILD)/kernel.bin of=$@ bs=512 count=200 seek=20 conv=notrunc

$(BUILD)/rdix.iso: $(BUILD)/rdix.bin $(GRUBCFG)/grub.cfg
	grub-file --is-x86-multiboot2 $<
	mkdir -p $(ISODIR)/boot/grub
	cp $(BUILD)/rdix.bin $(ISODIR)/boot
	cp $(GRUBCFG)/grub.cfg $(ISODIR)/boot/grub
	grub-mkrescue -o $@ $(ISODIR)

$(BUILD)/rdix.vmdk: $(BUILD)/master.img
	qemu-img convert -pO vmdk $< $@

.PHONY: vmdk
vmdk: $(BUILD)/rdix.vmdk

.PHONY: bochs
bochs: $(BUILD)/master.img
	bochs -q -f ./bochsrc -unlock

.PHONY: bochsb
bochsb: $(BUILD)/rdix.iso
	bochs -q -f ./bochsrc.grub -unlock

AHCI_DISK=-drive id=disk,file=../test.c,if=none \
-device ahci,id=ahci \
-device ide-hd,drive=disk,bus=ahci.0

QEMU=-monitor stdio

.PHONY: qemu
qemu: $(BUILD)/master.img
	qemu-system-i386 -m 32M -boot c -hda $< -s -S -nographic $(AHCI_DISK)

.PHONY: qemub
qemub: $(BUILD)/rdix.iso
	qemu-system-i386 -m 32M -boot c -hda $< -s -S -nographic $(AHCI_DISK)

.PHONY: qemu-g
qemu-g: $(BUILD)/master.img
	qemu-system-i386 -m 32M -boot c -hda $< -s -S $(AHCI_DISK)

.PHONY: clean
clean:
	rm -rf $(BUILD)