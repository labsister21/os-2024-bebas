#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/cpu/idt.h"
#include "header/driver/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/cpu/interrupt.h"
#include "header/filesystem/disk.h"
#include "header/filesystem/fat32.h"


void kernel_setup(void) {
    uint32_t parent_cluster_number = 0; 

    uint8_t buf[1024];

    struct FAT32DriverRequest req = {
        .buf = buf,
        .name = "example",
        .ext = "txt",
        .parent_cluster_number = parent_cluster_number,
        .buffer_size = 1024,
    };

    delete(req);
}
