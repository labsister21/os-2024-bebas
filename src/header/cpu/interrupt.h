#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../process/process.h"
#include "../process/scheduler.h"
#include "portio.h"

/* -- PIC constants -- */
// Syscall numbers
#define SYSCALL_READ                    0
#define SYSCALL_READ_DIRECTORY          1
#define SYSCALL_WRITE                   2
#define SYSCALL_DELETE                  3
#define SYSCALL_GETCHAR                 4
#define SYSCALL_PUTCHAR                 5
#define SYSCALL_PUTS                    6
#define SYSCALL_PUTS_AT                 7
#define SYSCALL_ACTIVATE_KEYBOARD       8
#define SYSCALL_DEACTIVATE_KEYBOARD     9
#define SYSCALL_SET_KEYBOARD_BORDERS    10
#define SYSCALL_KEYBOARD_PRESS_SHIFT    11
#define SYSCALL_KEYBOARD_PRESS_CTRL     12
#define SYSCALL_CLEAR_SCREEN            13
#define SYSCALL_SET_CURSOR              14
#define SYSCALL_GET_CURSOR_ROW          15
#define SYSCALL_GET_CURSOR_COL          16
#define SYSCALL_READ_CLUSTER            17
#define SYSCALL_TERMINATE_PROCESS       18
#define SYSCALL_CREATE_PROCESS          19
#define SYSCALL_GET_MAX_PROCESS_COUNT   20
#define SYSCALL_GET_PROCESS_INFO        21
#define SYSCALL_GET_CLOCK_TIME          22
#define SYSCALL_FIND_FILE               25
// PIC interrupt offset
#define PIC1_OFFSET          0x20
#define PIC2_OFFSET          0x28

// PIC ports
#define PIC1                 0x20
#define PIC2                 0xA0
#define PIC1_COMMAND         PIC1
#define PIC1_DATA            (PIC1 + 1)
#define PIC2_COMMAND         PIC2
#define PIC2_DATA            (PIC2 + 1)

// PIC ACK & mask constant
#define PIC_ACK              0x20
#define PIC_DISABLE_ALL_MASK 0xFF

// PIC remap constants
#define ICW1_ICW4            0x01   /* ICW4 (not) needed */
#define ICW1_SINGLE          0x02   /* Single (cascade) mode */
#define ICW1_INTERVAL4       0x04   /* Call address interval 4 (8) */
#define ICW1_LEVEL           0x08   /* Level triggered (edge) mode */
#define ICW1_INIT            0x10   /* Initialization - required! */

#define ICW4_8086            0x01   /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO            0x02   /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE       0x08   /* Buffered mode/slave */
#define ICW4_BUF_MASTER      0x0C   /* Buffered mode/master */
#define ICW4_SFNM            0x10   /* Special fully nested (not) */


/* -- PICs IRQ list -- */

// PIC Master
#define IRQ_TIMER        0
#define IRQ_KEYBOARD     1
#define IRQ_CASCADE      2
#define IRQ_COM2         3
#define IRQ_COM1         4
#define IRQ_LPT2         5
#define IRQ_FLOPPY_DISK  6
#define IRQ_LPT1_SPUR    7

// PIC Slave
#define IRQ_CMOS         8
#define IRQ_PERIPHERAL_1 9
#define IRQ_PERIPHERAL_2 10
#define IRQ_PERIPHERAL_3 11
#define IRQ_MOUSE        12
#define IRQ_FPU          13
#define IRQ_PRIMARY_ATA  14
#define IRQ_SECOND_ATA   15
#define IRQ_SYSCALL      16



/**
 * InterruptStack, data pushed by CPU when interrupt / exception is raised.
 * Refer to Intel x86 Vol 3a: Figure 6-4 Stack usage on transfer to Interrupt.
 * 
 * Note, when returning from interrupt handler with iret, esp must be pointing to eip pushed before 
 * or in other words, CPURegister, int_number and error_code should be pop-ed from stack.
 * 
 * @param error_code Error code that pushed with the exception
 * @param eip        Instruction pointer where interrupt is raised
 * @param cs         Code segment selector where interrupt is raised
 * @param eflags     CPU eflags register when interrupt is raised
 */
struct InterruptStack {
    uint32_t error_code;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} __attribute__((packed));

/**
 * InterruptFrame, entirety of general CPU states exactly before interrupt.
 * When used for interrupt handler, cpu.stack is kernel state before C function called,
 * not user stack when it get called. Check InterruptStack and interprivilege interrupt for more detail.
 * 
 * @param cpu        CPU state
 * @param int_number Interrupt vector value
 * @param int_stack  Hardware-defined (x86) stack state, note: will not access interprivilege ss and esp
 */
struct InterruptFrame {
    struct CPURegister    cpu;
    uint32_t              int_number;
    struct InterruptStack int_stack;
} __attribute__((packed));



// Activate PIC mask for keyboard only
void activate_keyboard_interrupt(void);

// I/O port wait, around 1-4 microsecond, for I/O synchronization purpose
void io_wait(void);

// Send ACK to PIC - @param irq Interrupt request number destination, note: ACKED_IRQ = irq+PIC1_OFFSET
void pic_ack(uint8_t irq);

// Shift PIC interrupt number to PIC1_OFFSET and PIC2_OFFSET (master and slave)
void pic_remap(void);

/**
 * Main interrupt handler when any interrupt / exception is raised.
 * DO NOT CALL THIS FUNCTION.
 * 
 * This function will be called first if any INT 0x00 - 0x40 is raised, 
 * and will call proper ISR for respective interrupt / exception.
 * 
 * If inter-privilege interrupt raised, SS and ESP is automatically out of main_interrupt_handler()
 * parameter. Can be checked with ((int*) info) + 4 for user $esp, 5 for user $ss
 * 
 * Again, this function is not for normal function call, all parameter will be automatically set when interrupt is called.
 * @param frame Information about CPU during interrupt is raised
 */
void main_interrupt_handler(struct InterruptFrame frame);

extern struct TSSEntry _interrupt_tss_entry;

/**
 * TSSEntry, Task State Segment. Used when jumping back to ring 0 / kernel
 */
struct TSSEntry {
    uint32_t prev_tss; // Previous TSS 
    uint32_t esp0;     // Stack pointer to load when changing to kernel mode
    uint32_t ss0;      // Stack segment to load when changing to kernel mode
    // Unused variables
    uint32_t unused_register[23];
} __attribute__((packed));
// Helper structs
struct SyscallPutsArgs {
    char* buf;
    uint32_t count;
    uint32_t fg_color;
    uint32_t bg_color;
};

struct SyscallPutsAtArgs {
    char* buf;
    uint32_t count;
    uint32_t fg_color;
    uint32_t bg_color;
    uint8_t row;
    uint8_t col;
};

struct SyscallKeyboardBordersArgs {
    uint8_t up;
    uint8_t down;
    uint8_t left;
    uint8_t right;
};

struct SyscallProcessInfoArgs {
    // Metadata
    uint32_t pid;
    char name[32];
    char state[8];

    // Flag to check if pid-th slot has a process
    bool process_exists;

    // Memory
    uint32_t page_frame_used_count;
};

struct SyscallClockTimeArgs {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};


// Set kernel stack in TSS
void set_tss_kernel_current_stack(void);

void syscall(struct InterruptFrame frame);

#define PIT_MAX_FREQUENCY   1193182
#define PIT_TIMER_FREQUENCY 1000
#define PIT_TIMER_COUNTER   (PIT_MAX_FREQUENCY / PIT_TIMER_FREQUENCY)

#define PIT_COMMAND_REGISTER_PIO          0x43
#define PIT_COMMAND_VALUE_BINARY_MODE     0b0
#define PIT_COMMAND_VALUE_OPR_SQUARE_WAVE (0b011 << 1)
#define PIT_COMMAND_VALUE_ACC_LOHIBYTE    (0b11  << 4)
#define PIT_COMMAND_VALUE_CHANNEL         (0b00  << 6) 
#define PIT_COMMAND_VALUE (PIT_COMMAND_VALUE_BINARY_MODE | PIT_COMMAND_VALUE_OPR_SQUARE_WAVE | PIT_COMMAND_VALUE_ACC_LOHIBYTE | PIT_COMMAND_VALUE_CHANNEL)

#define PIT_CHANNEL_0_DATA_PIO 0x40

void get_process_info(struct SyscallProcessInfoArgs* args);

#endif