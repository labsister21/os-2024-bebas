#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "framebuffer.c"
#include "interrupt.c"
#include "idt.c"



void kernel_setup(void)
{
  // uint32_t a;
  // uint32_t volatile b = 0x0000BABE;
  // __asm__("mov $0xCAFE0000, %0" : "=r"(a));
  // while (true)
  //   b += 1;
  // load_gdt(&_gdt_gdtr);
  // Testing frame buffer
  // framebuffer_clear();
  // framebuffer_write(3, 8,  'H', 0, 0xF);
  // framebuffer_write(3, 9,  'a', 0, 0xF);
  // framebuffer_write(3, 10, 'i', 0, 0xF);
  // framebuffer_write(3, 11, '!', 0, 0xF);
  // framebuffer_set_cursor(3, 10);
  // Testing interrupt
  load_gdt(&_gdt_gdtr);
  pic_remap();
  initialize_idt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);
  // __asm__("int $0x4");
  while (true);
}
