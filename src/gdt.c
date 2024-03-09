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
         .flags = 0,
         .size = 0,
         .granularity = 0},
        {.segment_low = 0xFFFFF,
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0xA,
         .non_system = 1,
         .privilege_level = 0,
         .present = 1,
         .limit_high = 0xF,
         .flags = 0xA,
         .size = 1,
         .granularity = 1},
        {.segment_low = 0xFFFFF,
         .base_low = 0,
         .base_mid = 0,
         .type_bit = 0x2,
         .non_system = 1,
         .privilege_level = 0,
         .present = 1,
         .limit_high = 0xF,
         .flags = 0x2,
         .size = 1,
         .granularity = 1}}};

/**
 * _gdt_gdtr, predefined system GDTR.
 * GDT pointed by this variable is already set to point global_descriptor_table above.
 * From: https://wiki.osdev.org/Global_Descriptor_Table, GDTR.size is GDT size minus 1.
 */
struct GDTR _gdt_gdtr = {
    .limit = sizeof(global_descriptor_table.table) - 1,
    .base = (uint64_t)&global_descriptor_table.table};
