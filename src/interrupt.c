#include "./header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"

void activate_keyboard_interrupt(void) {
    // Start initialization sequence for PIC1 (master) and PIC2 (slave)
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // Set interrupt vector offsets for PIC1 and PIC2
    out(PIC1_DATA, 0x20); // PIC1 vector offset
    io_wait();
    out(PIC2_DATA, 0x28); // PIC2 vector offset
    io_wait();

    // Configure PIC chaining
    out(PIC1_DATA, 4); // IRQ2 as slave for PIC1
    io_wait();
    out(PIC2_DATA, 2); // Tell slave PIC its cascade identity
    io_wait();

    // Set PIC operating mode
    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    // Unmask keyboard IRQ (IRQ 1)
    uint8_t mask = in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD);
    out(PIC1_DATA, mask);

    // Enable interrupts
    asm("sti");
}

void io_wait(void) {
    out(0x80, 0);
}

void pic_ack(uint8_t irq) {
    if (irq >= 8) out(PIC2_COMMAND, PIC_ACK);
    out(PIC1_COMMAND, PIC_ACK);
}

void pic_remap(void) {
    // Starts the initialization sequence in cascade mode
    out(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); 
    io_wait();
    out(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    out(PIC1_DATA, PIC1_OFFSET); // ICW2: Master PIC vector offset
    io_wait();
    out(PIC2_DATA, PIC2_OFFSET); // ICW2: Slave PIC vector offset
    io_wait();
    out(PIC1_DATA, 0b0100); // ICW3: tell Master PIC, slave PIC at IRQ2 (0000 0100)
    io_wait();
    out(PIC2_DATA, 0b0010); // ICW3: tell Slave PIC its cascade identity (0000 0010)
    io_wait();

    out(PIC1_DATA, ICW4_8086);
    io_wait();
    out(PIC2_DATA, ICW4_8086);
    io_wait();

    // Disable all interrupts
    out(PIC1_DATA, PIC_DISABLE_ALL_MASK);
    out(PIC2_DATA, PIC_DISABLE_ALL_MASK);
}

void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case 1:
            keyboard_isr();
    }
}
