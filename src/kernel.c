#include <stdint.h>
#include <stdbool.h>
#include "header/cpu/gdt.h"
#include "header/kernel-entrypoint.h"
#include "header/cpu/idt.h"
#include "header/driver/framebuffer.h"
#include "header/driver/keyboard.h"
#include "header/cpu/interrupt.h"

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
  // framebuffer_write(3, 8, 'H', 0, 0xF);
  // framebuffer_write(3, 9, 'a', 0, 0xF);
  // framebuffer_write(3, 10, 'i', 0, 0xF);
  // framebuffer_write(3, 11, '!', 0, 0xF);
  // framebuffer_set_cursor(3, 10);
  // while (true)
  //   ;
  // uint32_t a;
  // uint32_t volatile b = 0x0000BABE;
  // __asm__("mov $0xCAFE0000, %0" : "=r"(a));
  // while (true)
  //   b += 1;
  load_gdt(&_gdt_gdtr);
  pic_remap();
  initialize_idt();
  activate_keyboard_interrupt();
  framebuffer_clear();
  framebuffer_set_cursor(0, 0);
  
  int col = 0;
  int row = 0;
  keyboard_state_activate();
  // framebuffer_write(3, 10, 'i', 0xF, 0);
  // framebuffer_write(3, 11, '!', 0, 0xF);
  while(true){
      char c;
      get_keyboard_buffer(&c);
      framebuffer_write(row,col,c,0xF,0);
      col++;
      // framebuffer_set_cursor(row,col);
  }
}