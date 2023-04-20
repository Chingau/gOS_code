BUILD:=./build

CFLAGS := -m32	#32位程序
CFLAGS += -masm=intel
CFLAGS += -fno-builtin	#不需要gcc内置函数
CFLAGS += -nostdinc	#不需要标准头文件
CFLAGS += -fno-pic	#不需要位置无关代码
CFLAGS += -fno-pie	#不需要位置无关可执行程序
CFLAGS += -nostdlib	#不需要标准库
CFLAGS += -fno-stack-protector	#不需要栈保护
CFLAGS := $(strip ${CFLAGS})

DEBUG := -g

HD_IMG_NAME := hd.img

all: ${BUILD}/boot/boot.o ${BUILD}/boot/setup.o ${BUILD}/system.bin
	$(shell rm -rf $(BUILD)$(HD_IMG_NAME))
	bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $(BUILD)/$(HD_IMG_NAME)
	dd if=$(BUILD)/boot/boot.o of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=0 count=1 conv=notrunc
	dd if=$(BUILD)/boot/setup.o of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=1 count=2 conv=notrunc
	dd if=$(BUILD)/system.bin of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=3 count=60 conv=notrunc

${BUILD}/system.bin:${BUILD}/kernel.bin
	objcopy -O binary ${BUILD}/kernel.bin $@
	nm ${BUILD}/kernel.bin | sort > ${BUILD}/system.map

${BUILD}/kernel.bin:${BUILD}/boot/head.o ${BUILD}/init/main.o
	ld -m elf_i386 $^ -o $@ -Ttext 0x1200

${BUILD}/init/main.o:oskernel/init/main.c
	$(shell mkdir -p ${BUILD}/init)
	gcc ${CFLAGS} ${DEBUG} -c $^ -o $@

${BUILD}/boot/head.o:oskernel/boot/head.asm
	nasm -f elf32 -g $< -o $@

${BUILD}/boot/%.o: oskernel/boot/%.asm
	$(shell mkdir -p ${BUILD}/boot)
	nasm $< -o $@

clean:
	$(shell rm -rf ${BUILD})

bochs:
	bochs -q -f bochsrc

qemug: all
	qemu-system-x86_64 -m 32M -hda $(BUILD)/$(HD_IMG_NAME) -S -s

qemu: all
	qemu-system-i386 \
	-m 32M \
	-boot c \
	-hda $(BUILD)/$(HD_IMG_NAME)
