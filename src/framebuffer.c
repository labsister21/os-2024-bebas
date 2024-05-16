#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/driver/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    // TODO : Implement
    uint16_t pos = r * 80 + c;
    out(CURSOR_PORT_CMD, 0x0F);
    out(CURSOR_PORT_DATA, (uint8_t)(pos & 0xFF));
    out(CURSOR_PORT_CMD, 0x0E);
    out(CURSOR_PORT_DATA, (uint8_t)((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    // TODO : Implement
    uint16_t color = (bg << 4) | (fg & 0x0F);
    volatile uint16_t* where = (volatile uint16_t*)FRAMEBUFFER_MEMORY_OFFSET + (row * 80 + col);
    *where = (color << 8) | c;
    framebuffer_set_cursor(row, col);
}

void framebuffer_clear(void) {
    // TODO : Implement
    uint8_t *fb = (uint8_t *) FRAMEBUFFER_MEMORY_OFFSET; // Mengambil alamat memory framebuffer
    // Looping untuk menghapus semua karakter yang ada di framebuffer
    memset(fb, 0, 80*25*2);
}