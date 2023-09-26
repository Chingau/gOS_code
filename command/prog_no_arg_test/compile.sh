#!/bin/bash

if [[ ! -d "../../oskernel/lib" || ! -d "../../oskernel/build" ]];then
    echo "dependent dir don't exist!"
    cwd=$(pwd)
    cwd=${cwd##*/}
    cwd=${cwd%/}
    if [ $cwd != "command" ];then
        echo -e "you'd better in command dir.\n"
    fi
    exit
fi

BIN="prog_no_arg"
CFLAGS="-Wall -c -fno-builtin -W -Wstrict-prototypes \
        -Wmissing-prototypes -Wsystem-headers -m32"
LIB="../../oskernel/include/linux ../../oskernel/lib/"
OBJS="../../oskernel/build/string.o ../../oskernel/build/syscall.o \
      ../../oskernel/build/stdio.o ../../oskernel/build/vsprintf.o"
DO_IN=$BIN
DO_OUT="../../oskernel/hd.img"

gcc $CFLAGS -I $LIB -o $BIN".o" $BIN".c"
ld -m elf_i386 -e main $BIN".o" $OBJS -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]];then
    dd if=./$DO_IN of=$DO_OUT bs=512 count=$SEC_CNT seek=300 conv=notrunc
fi