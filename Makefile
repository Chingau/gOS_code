CFLAGS := -m32	#32位程序
CFLAGS += -masm=intel
CFLAGS += -fno-builtin	#不需要gcc内置函数
CFLAGS += -nostdinc	#不需要标准头文件
CFLAGS += -fno-pic	#不需要位置无关代码
CFLAGS += -fno-pie	#不需要位置无关可执行程序
CFLAGS += -nostdlib	#不需要标准库
CFLAGS += -fno-stack-protector	#不需要栈保护
CFLAGS := $(strip ${CFLAGS})

DEBUG := -g -O0
HD_IMG_NAME := hd.img

PATHS += oskernel/init
PATHS += oskernel/kernel
PATHS += oskernel/kernel/chr_drv
PATHS += oskernel/kernel/thread
PATHS += oskernel/lib
PATHS += oskernel/mm
FILES := $(foreach path, $(PATHS), $(wildcard $(path)/*.c))
OBJS := $(patsubst %.c, %.o, $(FILES))

ASM_PATHS := oskernel/kernel/asm
ASM_FILES := $(foreach path, $(ASM_PATHS), $(wildcard $(path)/*.asm))
ASM_OBJS := $(patsubst %.asm, %.o, $(ASM_FILES))

INCS += oskernel/include
INCS += oskernel/include/asm
INCS += oskernel/include/linux
INC_PATHS := $(foreach path, $(INCS), $(patsubst %, -I%, $(path)))

all: oskernel/boot/boot.o oskernel/boot/setup.o oskernel/system.bin
	bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat oskernel/$(HD_IMG_NAME)
	dd if=oskernel/boot/boot.o of=oskernel/$(HD_IMG_NAME) bs=512 seek=0 count=1 conv=notrunc
	dd if=oskernel/boot/setup.o of=oskernel/$(HD_IMG_NAME) bs=512 seek=1 count=2 conv=notrunc
	dd if=oskernel/system.bin of=oskernel/$(HD_IMG_NAME) bs=512 seek=3 count=60 conv=notrunc

oskernel/system.bin:oskernel/kernel.bin
	objcopy -O binary oskernel/kernel.bin $@
	nm oskernel/kernel.bin | sort > oskernel/system.map

oskernel/kernel.bin:oskernel/boot/head.o $(OBJS) $(ASM_OBJS)
	ld -m elf_i386 $^ -o $@ -Ttext 0x1200

oskernel/boot/boot.o: oskernel/boot/boot.asm
	nasm $< -o $@

oskernel/boot/setup.o: oskernel/boot/setup.asm
	nasm $< -o $@

oskernel/boot/head.o:oskernel/boot/head.asm
	nasm -f elf32 ${DEBUG} $< -o $@

$(OBJS):
	gcc ${CFLAGS} ${DEBUG} -c $*.c -o $@ $(INC_PATHS)

$(ASM_OBJS):$(ASM_FILES)
	nasm -f elf32 ${DEBUG} $*.asm -o $@

CLEANS := $(shell find -name "*.bin")
CLEANS += $(shell find -name "*.map")
CLEANS += $(shell find -name "*.o")
CLEANS += $(shell find -name "*.img")

clean:
	$(RM) $(CLEANS)

bochs:
	bochs -q -f bochsrc

qemug: all
	qemu-system-x86_64 -m 32M -hda oskernel/$(HD_IMG_NAME) -S -s

qemu: all
	qemu-system-i386 \
	-m 32M \
	-boot c \
	-hda oskernel/$(HD_IMG_NAME)
