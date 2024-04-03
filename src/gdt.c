#include "header/cpu/gdt.h"

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
