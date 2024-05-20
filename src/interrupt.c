#include "header/cpu/interrupt.h"
#include "header/cpu/portio.h"
#include "header/driver/keyboard.h"
#include "header/cpu/gdt.h"
#include "header/filesystem/fat32.h"
#include "header/stdlib/string.h"
#include "header/driver/framebuffer.h"

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


// Main interrupt handler
void main_interrupt_handler(struct InterruptFrame frame) {
    switch (frame.int_number) {
        case PIC1_OFFSET + IRQ_TIMER:
            struct Context ctx = {
                .eip = frame.int_stack.eip, 
                .eflags = frame.int_stack.eflags,
                .cpu = frame.cpu,
            };

            pic_ack(IRQ_TIMER);
            
            scheduler_save_context_to_current_running_pcb(ctx);            

            scheduler_switch_to_next_process();
            break;
        case PIC1_OFFSET + IRQ_KEYBOARD:
            keyboard_isr();
            break;
        case PIC1_OFFSET + IRQ_SYSCALL:
            syscall(frame);
            break;
    }
}


void activate_keyboard_interrupt(void) {
    out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_KEYBOARD));
}

struct TSSEntry _interrupt_tss_entry = {
    .ss0  = GDT_KERNEL_DATA_SEGMENT_SELECTOR,
};

void set_tss_kernel_current_stack(void) {
    uint32_t stack_ptr;
    // Reading base stack frame instead esp
    __asm__ volatile ("mov %%ebp, %0": "=r"(stack_ptr) : /* <Empty> */);
    // Add 8 because 4 for ret address and other 4 is for stack_ptr variable
    _interrupt_tss_entry.esp0 = stack_ptr + 8; 
}

void syscall(struct InterruptFrame frame) {
    switch (frame.cpu.general.eax) {
        case 0:
            *((int8_t*) frame.cpu.general.ecx) = read(
                *(struct FAT32DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 1:
            *((int8_t*) frame.cpu.general.ecx) = read_directory(
                *(struct FAT32DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 2:
            *((int8_t*) frame.cpu.general.ecx) = write(
                *(struct FAT32DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 3:
            *((int8_t*) frame.cpu.general.ecx) = delete(
                *(struct FAT32DriverRequest*) frame.cpu.general.ebx
            );
            break;
        case 4:
            get_keyboard_buffer((char*) frame.cpu.general.ebx);
            break;
        case 5:
            putchar(
                (char*) frame.cpu.general.ebx,
                frame.cpu.general.ecx
            );
            break;
        case 6:
            puts(
                (char*) frame.cpu.general.ebx, 
                frame.cpu.general.ecx, 
                frame.cpu.general.edx
            ); // Assuming puts() exist in kernel
            break;
        case 7: 
            keyboard_state_activate();
            break;
        case 8:
            keyboard_state_deactivate();
            break;
        case 9: //EBX : pointer to keyboardState
            break;
        case 10: //cls
            framebuffer_clear();
            break;
        case 11: // Create new user process
            break;
        case 12: // Terminate process by PID
            break;
        case 13: // Get process info
            break;
        case SYSCALL_GET_MAX_PROCESS_COUNT:
            *((uint32_t*) frame.cpu.general.ebx) = PROCESS_COUNT_MAX;
            break;
        case SYSCALL_GET_PROCESS_INFO:
            get_process_info((struct SyscallProcessInfoArgs*) frame.cpu.general.ebx);
            break;
        case SYSCALL_TERMINATE_PROCESS:
            process_destroy(process_manager_state.current_running_pid);
            break;
    }
}
void get_process_info(struct SyscallProcessInfoArgs* args) {
    // Get process index
    uint32_t pid = args->pid;

    // Set process_exists flag
    args->process_exists = process_manager_state.process_slot_occupied[pid];

    // If process exists, get the process info
    if (args->process_exists) {
        // Get process metadata
        struct ProcessControlBlock* pcb = &(_process_list[pid]);
        
        // Process name
        for (uint8_t i = 0; i < PROCESS_NAME_LENGTH_MAX; i++) {
            args->name[i] = pcb->metadata.name[i];
        }
        
        // Process state
        switch (pcb->metadata.state) {
            case PROCESS_STATE_RUNNING:
                for (uint8_t i = 0; i < 7; i++) {
                    args->state[i] = "RUNNING"[i];
                }
                break;
            case PROCESS_STATE_READY:
                for (uint8_t i = 0; i < 5; i++) {
                    args->state[i] = "READY"[i];
                }
                break;
            case PROCESS_STATE_BLOCKED:
                for (uint8_t i = 0; i < 7; i++) {
                    args->state[i] = "BLOCKED"[i];
                }
                break;
            default:
                break;
        } 

        // Get process memory info
        args->page_frame_used_count = pcb->memory.page_frame_used_count;
    }
}
