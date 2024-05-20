#include "header/process/process.h"
#include "header/memory/paging.h"
#include "header/stdlib/string.h"
#include "header/cpu/gdt.h"

struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];
struct ProcessManagerState process_manager_state;

int32_t process_create_user_process(struct FAT32DriverRequest request) {
    int32_t retcode = PROCESS_CREATE_SUCCESS; 
    if (process_manager_state.active_process_count >= PROCESS_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED;
        goto exit_cleanup;
    }

    // Ensure entrypoint is not located at kernel's section at higher half
    if ((uint32_t) request.buf >= KERNEL_VIRTUAL_ADDRESS_BASE) {
        retcode = PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT;
        goto exit_cleanup;
    }

    // Check whether memory is enough for the executable and additional frame for user stack
    uint32_t page_frame_count_needed = ceil_div(request.buffer_size + PAGE_FRAME_SIZE, PAGE_FRAME_SIZE);
    if (!paging_allocate_check(page_frame_count_needed) || page_frame_count_needed > PROCESS_PAGE_FRAME_COUNT_MAX) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    // Process PCB 
    int32_t p_index = process_list_get_inactive_index();
    if (p_index == -1) {
      retcode = PROCESS_CREATE_FAIL_NO_AVAILABLE_SLOT;
      goto exit_cleanup;
    }

    struct ProcessControlBlock *new_pcb = &(_process_list[p_index]);

    // Set up the process metadata
    new_pcb->metadata.pid = process_manager_state.current_running_pid;
    strncpy(new_pcb->metadata.name, request.name, PROCESS_NAME_LENGTH_MAX - 1);
    new_pcb->metadata.name[PROCESS_NAME_LENGTH_MAX - 1] = '\0';  // Ensure null termination
    new_pcb->metadata.state = PROCESS_STATE_READY;
    
    // 1. create new page directory
    struct PageDirectory *page_dir = paging_create_new_page_directory();
    if (page_dir == NULL) {
        retcode = PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY;
        goto exit_cleanup;
    }

    // 2. 1. Simpen page directory lama
    struct PageDirectory *old_page_dir = paging_get_current_page_directory_addr();
    // 2. 2. load page directory baru
    paging_use_page_directory(page_dir);
    // 2.3 read request buat masukin kode process ke page directory baru
    paging_allocate_user_page_frame(page_dir, request.buf);
    retcode = read(request);
    if (retcode != PROCESS_CREATE_SUCCESS) {
        goto exit_cleanup;
    }
    // 2.4 load page directory lama
    paging_use_page_directory(old_page_dir);

    // 3.1 set up state process
    process_manager_state.current_running_pid++;

    // 3.2 set context process
    process_initialize_context(new_pcb, request.buf);

    // 4. Set the page directory virtual address in the context
    new_pcb->context.page_directory_virtual_addr = page_dir;

    // Mark the process slot as occupied
    process_manager_state.process_slot_occupied[p_index] = true;
    process_manager_state.active_process_count++;

    exit_cleanup:
    return retcode;
}

uint32_t ceil_div(uint32_t numerator, uint32_t denominator) {
  return (numerator + denominator - 1) / denominator;
}

int32_t process_list_get_inactive_index() {
  for (int32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
    if (_process_list[i].metadata.pid == 0) {
      return i;
    }
  }
  return -1;  // Return -1 if no inactive process is found
}

/**
 * Get currently running process PCB pointer
 * 
 * @return Will return NULL if there's no running process
 */
struct ProcessControlBlock* process_get_current_running_pcb_pointer(void) {
    for (int32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
        if (process_manager_state.process_slot_occupied[i] &&   _process_list[i].metadata.pid == process_manager_state.current_running_pid) {
            return &_process_list[i];
        }
    }
    return NULL;  // Return NULL if no running process is found
}

/**
 * Destroy process then release page directory and process control block
 * 
 * @param pid Process ID to delete
 * @return    True if process destruction success
 */
bool process_destroy(uint32_t pid) {
  for (int32_t i = 0; i < PROCESS_COUNT_MAX; i++) {
    if (_process_list[i].metadata.pid == pid) {
      // Reset the process control block
      memset(&_process_list[i], 0, sizeof(struct ProcessControlBlock));

      // Mark the process slot as unoccupied
      process_manager_state.process_slot_occupied[i] = false;

      // Decrement the count of active processes
      process_manager_state.active_process_count--;

      return true;
    }
  }
  return false;  // Return false if no process with the given PID is found
}

bool process_allocate_virtual_memory(struct ProcessControlBlock *pcb, void *virtual_addr) {
    struct PageDirectory *page_dir = paging_get_current_page_directory_addr();

    bool allocation_success = paging_allocate_user_page_frame(page_dir, virtual_addr);

    if (allocation_success) {
      pcb->context.page_directory_virtual_addr = page_dir;
    }

    return allocation_success;
}

int32_t process_load_executable(struct ProcessControlBlock *pcb, struct FAT32DriverRequest request) {
    // Save the current page directory
    struct PageDirectory *old_page_dir = paging_get_current_page_directory_addr();

    // Switch to the new process's page directory
    paging_use_page_directory(pcb->context.page_directory_virtual_addr);

    // Read the executable into memory
    int32_t read_result = read(request);

    // Switch back to the old page directory
    paging_use_page_directory(old_page_dir);

    // If the read operation failed, cancel the process creation
    if (read_result < 0) {
        process_destroy(pcb->metadata.pid);
        return PROCESS_CREATE_FAIL_FS_READ_FAILURE;
    }

    return PROCESS_CREATE_SUCCESS;
}

void process_initialize_context(struct ProcessControlBlock *pcb, void *entry_point) {
    // Initialize all registers to zero
    memset(&pcb->context, 0, sizeof(struct Context));
    // Set the entry point
    pcb->context.eip = (uint32_t)entry_point;

    // Set the segment registers to the user data segment selector with privilege level 3
    pcb->context.cpu.segment.gs = USER_DATA_SEGMENT_SELECTOR;
    pcb->context.cpu.segment.fs = USER_DATA_SEGMENT_SELECTOR;
    pcb->context.cpu.segment.es = USER_DATA_SEGMENT_SELECTOR;
    pcb->context.cpu.segment.ds = USER_DATA_SEGMENT_SELECTOR;

    pcb->context.cpu.stack.ebp = CPU_STACK;
    pcb->context.cpu.stack.esp = CPU_STACK;



    // Set the necessary eflags
    pcb->context.eflags = CPU_EFLAGS_BASE_FLAG | CPU_EFLAGS_FLAG_INTERRUPT_ENABLE;
}