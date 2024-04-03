#include "header/cpu/idt.h"
#include "header/cpu/gdt.h"

struct IDT idt;
struct IDTR _idt_idtr;

void initialize_idt(void) {
  for (int i = 0; i < ISR_STUB_TABLE_LIMIT; i++) {
    set_interrupt_gate(i, isr_stub_table[i]);
  }
  _idt_idtr.limit = sizeof(struct IDT) * IDT_MAX_ENTRY_COUNT - 1;
  _idt_idtr.base = &idt;
  __asm__ volatile("lidt %0" : : "m"(_idt_idtr));
  __asm__ volatile("sti");
}

void set_interrupt_gate(uint8_t int_vector, void *handler_address) {
  struct IDTGate *idt_int_gate = &idt.gates[int_vector];
  idt_int_gate->offset_low = (uint16_t)((uint32_t)handler_address & 0xFFFF);
  idt_int_gate->segment = GDT_KERNEL_CODE_SEGMENT_SELECTOR;
  idt_int_gate->_reserved = 0;
  idt_int_gate->_r_bit_1 = INTERRUPT_GATE_R_BIT_1;
  idt_int_gate->_r_bit_2 = INTERRUPT_GATE_R_BIT_2;
  idt_int_gate->gate_32 = 1;
  idt_int_gate->_r_bit_3 = INTERRUPT_GATE_R_BIT_3;
  idt_int_gate->offset_high = (uint16_t)(((uint32_t)handler_address >> 16) & 0xFFFF);
  idt_int_gate->valid_bit = 1;
}