#ifndef _IDT_H
#define _IDT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define IDT_MAX_ENTRY_COUNT 256
#define ISR_STUB_TABLE_LIMIT 64
#define INTERRUPT_GATE_R_BIT_1 0b000
#define INTERRUPT_GATE_R_BIT_2 0b110
#define INTERRUPT_GATE_R_BIT_3 0b0

extern void *isr_stub_table[ISR_STUB_TABLE_LIMIT];

extern struct IDTR _idt_idtr;

struct IDTGate
{
  uint16_t offset_low;
  uint16_t segment;
  uint8_t _reserved;
  uint8_t _r_bit_1 : 3;
  uint8_t _r_bit_2 : 3;
  uint8_t gate_32 : 1;
  uint8_t _r_bit_3 : 1;
  uint16_t offset_high;
} __attribute__((packed));

struct IDT
{
  struct IDTGate gates[IDT_MAX_ENTRY_COUNT];
} __attribute__((packed));

extern struct IDT idt;

struct IDTR
{
  uint16_t limit;
  struct IDT *base;
} __attribute__((packed));

/**
 * Set IDTGate with proper interrupt handler values.
 * Will directly edit global IDT variable and set values properly
 *
 * @param int_vector       Interrupt vector to handle
 * @param handler_address  Interrupt handler address
 * @param gdt_seg_selector GDT segment selector, for kernel use GDT_KERNEL_CODE_SEGMENT_SELECTOR
 * @param privilege        Descriptor privilege level
 */
void set_interrupt_gate(uint8_t int_vector, void *handler_address, uint16_t gdt_seg_selector, uint8_t privilege);

void initialize_idt(void);

#endif
