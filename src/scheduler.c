#include "header/process/scheduler.h"

void activate_timer_interrupt(void) {
  __asm__ volatile("cli");
  // Setup how often PIT fire
  uint32_t pit_timer_counter_to_fire = PIT_TIMER_COUNTER;
  out(PIT_COMMAND_REGISTER_PIO, PIT_COMMAND_VALUE);
  out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) (pit_timer_counter_to_fire & 0xFF));
  out(PIT_CHANNEL_0_DATA_PIO, (uint8_t) ((pit_timer_counter_to_fire >> 8) & 0xFF));

  // Activate the interrupt
  out(PIC1_DATA, in(PIC1_DATA) & ~(1 << IRQ_TIMER));
}

void scheduler_init(void) {
  activate_timer_interrupt();
}

void scheduler_save_context_to_current_running_pcb(struct Context ctx) {
  struct ProcessControlBlock *pcb = process_get_current_running_pcb_pointer();

  if (pcb != NULL) {
    pcb->metadata.state = PROCESS_STATE_READY;

    struct Context* pcb_ctx = &pcb->context;
    pcb_ctx->eip = ctx.eip;
    pcb_ctx->eflags = ctx.eflags;
    pcb_ctx->cpu = ctx.cpu;
  }

}

__attribute__((noreturn)) void scheduler_switch_to_next_process(void) {
  uint32_t current_process_index = process_manager_state.current_running_pid;

  if (current_process_index == NO_PROCESS_RUNNING) current_process_index = 0;

  struct ProcessControlBlock pcb;
  while (true) {
    current_process_index = (current_process_index + 1) % PROCESS_COUNT_MAX;
    if (process_manager_state.process_slot_occupied[current_process_index]) {
      pcb = _process_list[current_process_index];
      _process_list[current_process_index].metadata.state = PROCESS_STATE_RUNNING;
      process_manager_state.current_running_pid = current_process_index;
      break;
    }
  }

  paging_use_page_directory(pcb.context.page_directory_virtual_addr);
  process_context_switch(pcb.context);
}