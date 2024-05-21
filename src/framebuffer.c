#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/driver/framebuffer.h"
#include "header/stdlib/string.h"
#include "header/cpu/portio.h"

void framebuffer_set_cursor(uint8_t r, uint8_t c) {
    uint16_t pos = r * VGA_WIDTH + c;

    out(0x3D4, 0x0F);
    out(0x3D5, (uint8_t) (pos & 0xFF));
    out(0x3D4, 0x0E);
    out(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

void framebuffer_write(uint8_t row, uint8_t col, char c, uint8_t fg, uint8_t bg) {
    FRAMEBUFFER_MEMORY_OFFSET[(row * VGA_WIDTH + col)*2] = c;
    FRAMEBUFFER_MEMORY_OFFSET[(row * VGA_WIDTH + col)*2+1] = fg | bg;
}

void framebuffer_clear(void) {
    for (int i = 0; i < 80; i++){
        for (int j = 0; j < 25; j++){
            framebuffer_write(j,i,0x00,0x07,0x07);
        }
    }
    framebuffer_set_cursor(0,0);
}

char framebuffer_get(uint8_t row, uint8_t col){
    return FRAMEBUFFER_MEMORY_OFFSET[(row * VGA_WIDTH + col)*2];
}

uint16_t get_cursor_pos(void) {
    uint16_t pos = 0;
    out(0x3D4, 0x0F);
    pos |= in(0x3D5);
    out(0x3D4, 0x0E);
    pos |= ((uint16_t)in(0x3D5)) << 8;
    return pos;
}

void puts(char *str, uint32_t len, uint32_t color) {
        for (uint32_t i = 0; i < len; i++)
        {
            putchar(str+i,color);
        }
    }

void shift_up(void){
    for(int i = 0; i < VGA_HEIGHT-1; i++){
        for(int j = 0; j < VGA_WIDTH; j++){
            FRAMEBUFFER_MEMORY_OFFSET[(i * VGA_WIDTH + j)*2] = FRAMEBUFFER_MEMORY_OFFSET[((i+1) * VGA_WIDTH + j)*2];
            FRAMEBUFFER_MEMORY_OFFSET[(i * VGA_WIDTH + j)*2+1] = FRAMEBUFFER_MEMORY_OFFSET[((i+1) * VGA_WIDTH + j)*2+1];
        }
    }
    for (int j = 0; j < 80; j++){
        framebuffer_write(VGA_HEIGHT-1,j,0x00,0x07,0x07);
    }
}

void putchar(char* c, uint32_t color) {
    if (*c){
    uint16_t row_now = get_cursor_pos() / VGA_WIDTH;
    uint16_t col = get_cursor_pos() % VGA_WIDTH;
    if (*c == '\n'){
        framebuffer_write(row_now, col, *c, 0x00, 0x00);
        row_now ++;
        col = 0;
    }
    else if (*c == '\b'){
        if (col > 0){
            col = col-1;
            framebuffer_write(row_now, col,0x00,0x07,0x07);
        }
        else if (row_now > 0){
            col = VGA_WIDTH - 1;
            row_now -= 1;
            while(!framebuffer_get(row_now,col) && framebuffer_get(row_now,col)!='\n'){
                if(col == 0){
                    col = VGA_WIDTH;
                    row_now -= 1;
                }
                col--;
            };
            framebuffer_write(row_now, col,0x00,0x07,0x07);
        }
    }
    else{
        framebuffer_write(row_now, col, *c, color, 0);
        col++;
    }

    row_now += col / VGA_WIDTH;
    col = col % VGA_WIDTH;

    if (row_now == VGA_HEIGHT){
        shift_up();
        row_now = VGA_HEIGHT-1;
    }

    framebuffer_set_cursor(row_now,col);
    }
}