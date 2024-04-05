#include "header/driver/keyboard.h"
#include "header/driver/framebuffer.h"
#include "header/cpu/portio.h"
#include "header/stdlib/string.h"

static bool key_pressed = false;
static bool backspace_pressed = false;
static struct KeyboardDriverState keyboard_state = {false, false, 0, {0}};

const char keyboard_scancode_1_to_ascii_map[256] = {
    0,
    0x1B,
    '1',
    '2',
    '3',
    '4',
    '5',
    '6',
    '7',
    '8',
    '9',
    '0',
    '-',
    '=',
    '\b',
    '\t',
    'q',
    'w',
    'e',
    'r',
    't',
    'y',
    'u',
    'i',
    'o',
    'p',
    '[',
    ']',
    '\n',
    0,
    'a',
    's',
    'd',
    'f',
    'g',
    'h',
    'j',
    'k',
    'l',
    ';',
    '\'',
    '`',
    0,
    '\\',
    'z',
    'x',
    'c',
    'v',
    'b',
    'n',
    'm',
    ',',
    '.',
    '/',
    0,
    '*',
    0,
    ' ',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    '-',
    0,
    0,
    0,
    '+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,

    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

// struct KeyboardDriverState {
//     bool keyboard_input_on;
//     char keyboard_buffer;
// } KeyboardDriverState;

int8_t terminal_length = 0;

int8_t col = 0;

int8_t row = 0;

void keyboard_isr(void)
{
  if (!keyboard_state.keyboard_input_on) {
        keyboard_state.buffer_index = 0;
    }else{
        uint8_t scancode = in(KEYBOARD_DATA_PORT);
        char mapped_char = keyboard_scancode_1_to_ascii_map[scancode];
        if(mapped_char == '\b'){
            if(col >= terminal_length + 1){
                backspace_pressed = true;
                framebuffer_write(row, col-1, ' ', 0x0F, 0x00);
                framebuffer_set_cursor(row,col-1);
                keyboard_state.keyboard_buffer[keyboard_state.buffer_index-1] = ' ';
            }
        }
        else if (scancode == 0x1C && !key_pressed)
        {
            keyboard_state_deactivate();
            row++;
            framebuffer_set_cursor(row,col);
            key_pressed = true;
        }
        else if (scancode >= 0x02 && scancode <=0x4A && !key_pressed) {
            framebuffer_write(row, col, mapped_char, 0x0F, 0x00);
            framebuffer_set_cursor(row,col+1);
            keyboard_state.keyboard_buffer[keyboard_state.buffer_index] = mapped_char;
            key_pressed = true;
        }
        else if (scancode >= 0x80 && backspace_pressed){
            backspace_pressed = false;
            if(keyboard_state.buffer_index != 0){
                keyboard_state.buffer_index--;
                col--;
            }
        }
        else if (scancode >= 0x80 && scancode != 0x9C  && key_pressed) {
            key_pressed = false;
            keyboard_state.buffer_index++;
            col++;
        }
        else if(scancode == 0x9C)
        {
            key_pressed = false;
        }
    }

  pic_ack(IRQ_KEYBOARD);
}

// Activate keyboard ISR / start listen keyboard & save to buffer
void keyboard_state_activate(void)
{
  keyboard_state.keyboard_input_on = true;
    keyboard_state.buffer_index = 0;
    memset(keyboard_state.keyboard_buffer, 0, KEYBOARD_BUFFER_SIZE);
}

// Deactivate keyboard ISR / stop listening keyboard interrupt
void keyboard_state_deactivate(void)
{
  keyboard_state.keyboard_input_on = false;
}

// Get keyboard buffer value and flush the buffer - @param buf Pointer to char buffer
void get_keyboard_buffer(char *buf)
{
  memcpy(buf, keyboard_state.keyboard_buffer, KEYBOARD_BUFFER_SIZE);
}