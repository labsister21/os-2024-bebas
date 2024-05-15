#include "header/cpu/gdt.h"
#include "header/cpu/interrupt.h"

/**
 * global_descriptor_table, predefined GDT.
 * Initial SegmentDescriptor already set properly according to Intel Manual & OSDev.
 * Table entry : [{Null Descriptor}, {Kernel Code}, {Kernel Data (variable, etc)}, ...].
 */
struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {.segment_low = 0,
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0,
         .non_system = 0,
         .privilege_level = 0,
         .present = 0,
         .limit_high = 0,
         .avl = 0,
         .l = 0,
         .db = 0,
         .granularity = 0,
         .base_high = 0},
        {.segment_low = 0xFFFF, // Limit = 0xFFFFF
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0xA, // Code, executable, read-accessible
         .non_system = 1,
         .privilege_level = 0, // Kernel level
         .present = 1,
         .limit_high = 0xF,
         .avl = 0,
         .l = 0,           // 32-bit code segment
         .db = 1,          // 32-bit opcode
         .granularity = 1, // 4KiB granularity
         .base_high = 0},  // Kernel code segment
        {
            .segment_low = 0xFFFF, // Limit = 0xFFFFF
            .base_low = 0,
            .base_mid = 0,
            .type_bit = 0x2, // Data, read/write, accessed
            .non_system = 1,
            .privilege_level = 0, // Kernel level
            .present = 1,
            .limit_high = 0xF,
            .avl = 0,
            .l = 0,           // 32-bit data segment
            .db = 1,          // 32-bit opcode
            .granularity = 1, // 4KiB granularity
            .base_high = 0},
    }};

/**
 * _gdt_gdtr, predefined system GDTR.
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    .limit = sizeof(global_descriptor_table) - 1,
    .base = &global_descriptor_table};

static struct GlobalDescriptorTable global_descriptor_table = {
    .table = {
        {/* TODO: Null Descriptor */

        },
        {/* TODO: Kernel Code Descriptor */},
        {/* TODO: Kernel Data Descriptor */},
        {/* TODO: User   Code Descriptor */},
        {/* TODO: User   Data Descriptor */},
        {
            .segment_high      = (sizeof(struct TSSEntry) & (0xF << 16)) >> 16,
            .segment_low       = sizeof(struct TSSEntry),
            .base_high         = 0,
            .base_mid          = 0,
            .base_low          = 0,
            .non_system        = 0,    // S bit
            .type_bit          = 0x9,
            .privilege         = 0,    // DPL
            .valid_bit         = 1,    // P bit
            .opr_32_bit        = 1,    // D/B bit
            .long_mode         = 0,    // L bit
            .granularity       = 0,    // G bit
        },
        {0}
    }
};

void gdt_install_tss(void) {
    uint32_t base = (uint32_t) &_interrupt_tss_entry;
    global_descriptor_table.table[5].base_high = (base & (0xFF << 24)) >> 24;
    global_descriptor_table.table[5].base_mid  = (base & (0xFF << 16)) >> 16;
    global_descriptor_table.table[5].base_low  = base & 0xFFFF;
}

