#include "./header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"

void activate_keyboard_interrupt(void) {
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK ^ (1 << IRQ_KEYBOARD));
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void io_wait(void) {
    out(0x80, 0);
}

void pic_ack(uint8_t irq) {
    if (irq >= 8) out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void) {
    // Start initialization sequence for PIC1 (master) and PIC2 (slave)
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // Set interrupt vector offsets for PIC1 and PIC2
    out(PIC1_DATA, PIC1_OFFSET); // PIC1 vector offset
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // PIC2 vector offset
    io_wait();

    // Configure PIC chaining
    out(PIC1_DATA, 0b0100); // IRQ2 as slave for PIC1
    io_wait();
    out(PIC2_DATA, 0b0010); // Tell slave PIC its cascade identity
    io_wait();

    // Set PIC operating mode
    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case IRQ_KEYBOARD + PIC1_OFFSET:
            keyboard_isr();
            break;
        default:
            break;
    }
}
