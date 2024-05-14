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
#include "header/memory/paging.h"


void kernel_setup(void) {
    // tes paging
    // paging_allocate_user_page_frame(&_paging_kernel_page_directory, (uint8_t*) 0x600000);
    // *((uint8_t*) 0x900000) = 1;
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
