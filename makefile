BUILD=../build
SRC=.
CONFIG=./configure
GRUBCFG=./configure
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
LFLAGS=-m elf_i386 -static -Ttext $(KERNELSTARTPOINT) --section-start=multiboot2=$(MULTIBOOT2)

FILE_KERNEL:=$(wildcard $(SRC)/kernel/*.c $(SRC)/fs/*.c $(SRC)/fs/minix1/*.c $(SRC)/buildin/*.c $(SRC)/hd/usb3.0/*.c)
FILE:=$(notdir $(FILE_KERNEL))
OBJ:=$(patsubst %.c, $(BUILD)/%.o, $(FILE))

all: vmdk $(BUILD)/master.img $(BUILD)/rdix.iso install_grub

.PHONY: test
test:
	$(shell echo $(TEST) > readme.txt)

$(BUILD)/%.bin: $(SRC)/boot/%.asm
	mkdir -p $(dir $@)
	nasm -f bin $^ -o $@

$(BUILD)/%.o: $(SRC)/kernel/%.asm
	nasm -f elf32 -g $^ -o $@ 

$(BUILD)/%.o: $(SRC)/kernel/%.c 
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@ 

$(BUILD)/%.o: $(SRC)/fs/%.c 
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@

$(BUILD)/%.o: $(SRC)/fs/minix1/%.c 
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@
	
$(BUILD)/%.o: $(SRC)/buildin/%.c 
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ -o $@

$(BUILD)/%.o: $(SRC)/hd/usb3.0/%.c
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
	dd if=$(BUILD)/loader.bin of=$@ bs=512 count=2 seek=15 conv=notrunc
	dd if=$(BUILD)/kernel.bin of=$@ bs=512 count=200 seek=20 conv=notrunc

	sfdisk $@ < $(CONFIG)/master.sfdisk

	sudo losetup $(FREELOOP) --partscan $@
	sudo mkfs.minix -1 -n 14 $(FREELOOPPT)
	sudo losetup -d $(FREELOOP)

$(BUILD)/rdix.vmdk: $(BUILD)/master.img
	qemu-img convert -pO vmdk $< $@

$(BUILD)/rdixiso.vmdk: $(BUILD)/rdix.iso
	qemu-img convert -pO vmdk $< $@

.PHONY: vmdk
vmdk: $(BUILD)/rdix.vmdk $(BUILD)/rdixiso.vmdk

.PHONY: bochs
bochs: $(BUILD)/master.img
	bochs -q -f ./bochsrc -unlock

.PHONY: bochsb
bochsb: $(BUILD)/rdix.iso
	bochs -q -f ./bochsrc.grub -unlock

AHCI_DISK=-drive id=disk,file=./test,if=none \
-device ahci,id=ahci \
-device ide-hd,drive=disk,bus=ahci.0 \
-usb \
-device qemu-xhci

QEMU=-monitor stdio

.PHONY: qemu
qemu: $(BUILD)/master.img
	qemu-system-i386 -m 32M -boot c -hda $< -s -S -nographic $(AHCI_DISK)

.PHONY: qemub
qemub: $(BUILD)/rdix.iso
	qemu-system-i386 -m 32M -boot c -hda $< -s -S $(AHCI_DISK)

.PHONY: qemu-g
qemu-g: $(BUILD)/master.img
	qemu-system-i386 -m 32M -boot c -hda $< -s -S $(AHCI_DISK)

FREELOOP=$(shell sudo losetup -f)
FREELOOPPT=$(FREELOOP)p1
GRUBFLAG=--force --removable --no-floppy --target=i386-pc

$(BUILD)/rdix.iso:
	mkdir -p $(dir $@)
	touch $@
	dd if=/dev/zero of=$@ bs=1M count=32
	sfdisk $@ < $(CONFIG)/master.sfdisk

.PHONY: install_grub
install_grub: $(BUILD)/rdix.iso $(BUILD)/rdix.bin $(GRUBCFG)/grub.cfg
	@echo $(FREELOOP)
	sudo losetup $(FREELOOP) --partscan $<
	sudo mkfs.vfat -F 32 -n MULTIBOOT $(FREELOOPPT)
	mkdir -p $(BUILD)/mnt && sudo mount $(FREELOOPPT) $(BUILD)/mnt
	sudo grub-install $(GRUBFLAG) --boot-directory=$(BUILD)/mnt/boot $(FREELOOP)
	sudo cp $(GRUBCFG)/grub.cfg $(BUILD)/mnt/boot/grub
	sudo cp $(BUILD)/rdix.bin $(BUILD)/mnt/boot
	sudo umount $(FREELOOPPT)
	sudo losetup -d $(FREELOOP)
	rm -r $(BUILD)/mnt

.PHONY: mount
mount: ./test
	sudo losetup /dev/loop20 --partscan $<
	sudo mount /dev/loop20p1 ./mnt
	sudo chmod 777 ./mnt

.PHONY: umount
umount: 
	sudo umount ./mnt
	sudo losetup -d /dev/loop20

.PHONY: mkfs
mkfs: ./test
	sudo losetup /dev/loop20 --partscan $<
	sudo mkfs.minix -1 -n 14 /dev/loop20p1
	sudo mount /dev/loop20p1 ./mnt
	sudo chmod 777 ./mnt

.PHONY: refresh
refresh:
	make umount
	make mount

.PHONY: usb_make
usb_make:
	sudo dd if=../build/master.img of=/dev/sdb bs=32M

.PHONY: clean
clean:
	rm -rf $(BUILD)