#include "tools/floppy.h"
#include "tools/fileinfo.h"

int main() {
    char* boot_filepath = "/home/ziya/gos/build/boot/boot.o";

    Floppy* floppy = create_floppy();

    Fileinfo* boot_fileinfo = read_file(boot_filepath);
    write_bootloader(floppy, boot_fileinfo);

    create_image("/home/ziya/gos/a.img", floppy);

    return 0;
}
