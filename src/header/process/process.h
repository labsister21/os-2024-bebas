#ifndef _PROCESS_H
#define _PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../memory/paging.h"
#include "../filesystem/fat32.h"


#define PROCESS_NAME_LENGTH_MAX          32
#define PROCESS_PAGE_FRAME_COUNT_MAX     8
#define PROCESS_COUNT_MAX                16

#define KERNEL_RESERVED_PAGE_FRAME_COUNT 4
#define KERNEL_VIRTUAL_ADDRESS_BASE      0xC0000000

#define CPU_EFLAGS_BASE_FLAG               0x2
#define CPU_EFLAGS_FLAG_CARRY              0x1
#define CPU_EFLAGS_FLAG_PARITY             0x4
#define CPU_EFLAGS_FLAG_AUX_CARRY          0x10
#define CPU_EFLAGS_FLAG_ZERO               0x40
#define CPU_EFLAGS_FLAG_SIGN               0x80
#define CPU_EFLAGS_FLAG_TRAP               0x100
#define CPU_EFLAGS_FLAG_INTERRUPT_ENABLE   0x200
#define CPU_EFLAGS_FLAG_DIRECTION          0x400
#define CPU_EFLAGS_FLAG_OVERFLOW           0x800
#define CPU_EFLAGS_FLAG_IO_PRIVILEGE       0x3000
#define CPU_EFLAGS_FLAG_NESTED_TASK        0x4000
#define CPU_EFLAGS_FLAG_MODE               0x8000
#define CPU_EFLAGS_FLAG_RESUME             0x10000
#define CPU_EFLAGS_FLAG_VIRTUAL_8086       0x20000
#define CPU_EFLAGS_FLAG_ALIGNMENT_CHECK    0x40000
#define CPU_EFLAGS_FLAG_VINTERRUPT_FLAG    0x80000
#define CPU_EFLAGS_FLAG_VINTERRUPT_PENDING 0x100000
#define CPU_EFLAGS_FLAG_CPUID_INSTRUCTION  0x200000
#define CPU_EFLAGS_FLAG_AES_SCHEDULE_LOAD  0x40000000
#define CPU_EFLAGS_FLAG_ALTER_INSTRUCTION  0x80000000

#define CPU_STACK 0x400000 - 4

// Return code constant for process_create_user_process()
#define PROCESS_CREATE_SUCCESS                   0
#define PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED 1
#define PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT   2
#define PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY    3
#define PROCESS_CREATE_FAIL_FS_READ_FAILURE      4
#define PROCESS_CREATE_FAIL_NO_AVAILABLE_SLOT 5

/**
 * Contain information needed for task to be able to get interrupted and resumed later
 *
 * @param cpu                         All CPU register state
 * @param eip                         CPU instruction counter to resume execution
 * @param eflags                      Flag register to load before resuming the execution
 * @param page_directory_virtual_addr CPU register CR3, containing pointer to active page directory
 */

/**
 * CPURegister, store CPU registers values.
 * 
 * @param index   CPU index register (di, si)
 * @param stack   CPU stack register (bp, sp)
 * @param general CPU general purpose register (a, b, c, d)
 * @param segment CPU extra segment register (gs, fs, es, ds)
 */
struct CPURegister {
    struct {
        uint32_t edi;
        uint32_t esi;
    } __attribute__((packed)) index;
    struct {
        uint32_t ebp;
        uint32_t esp;
    } __attribute__((packed)) stack;
    struct {
        uint32_t ebx;
        uint32_t edx;
        uint32_t ecx;
        uint32_t eax;
    } __attribute__((packed)) general;
    struct {
        uint32_t gs;
        uint32_t fs;
        uint32_t es;
        uint32_t ds;
    } __attribute__((packed)) segment;
} __attribute__((packed));

struct Context {
  struct CPURegister cpu;
  uint32_t eip;
  uint32_t eflags;
  struct PageDirectory *page_directory_virtual_addr;

};

typedef enum PROCESS_STATE {
  NO_PROCESS,
  PROCESS_STATE_READY,     
  PROCESS_STATE_RUNNING,   
  PROCESS_STATE_BLOCKED
} PROCESS_STATE;

/**
 * Structure data containing information about a process
 *
 * @param metadata Process metadata, contain various information about process
 * @param context  Process context used for context saving & switching
 * @param memory   Memory used for the process
 */

struct ProcessControlBlock {
  struct {
    uint32_t pid; 
    char name[PROCESS_NAME_LENGTH_MAX];  
    PROCESS_STATE state; 
  } metadata;

  struct Context context;  

  struct {
    void     *virtual_addr_used[PROCESS_PAGE_FRAME_COUNT_MAX];  
    uint32_t page_frame_used_count;  
  } memory;
};

extern struct ProcessControlBlock _process_list[PROCESS_COUNT_MAX];
struct ProcessManagerState {
  bool process_slot_occupied[PROCESS_COUNT_MAX];
  int active_process_count;
  uint32_t current_running_pid;
};
extern struct ProcessManagerState process_manager_state;

/**
 * Get currently running process PCB pointer
 * 
 * @return Will return NULL if there's no running process
 */
struct ProcessControlBlock* process_get_current_running_pcb_pointer(void);

/**
 * Create new user process and setup the virtual address space.
 * All available return code is defined with macro "PROCESS_CREATE_*"
 * 
 * @note          This procedure assumes no reentrancy in ISR
 * @param request Appropriate read request for the executable
 * @return        Process creation return code
 */
int32_t process_create_user_process(struct FAT32DriverRequest request);

/**
 * Destroy process then release page directory and process control block
 * 
 * @param pid Process ID to delete
 * @return    True if process destruction success
 */
bool process_destroy(uint32_t pid);

uint32_t ceil_div(uint32_t numerator, uint32_t denominator);

int32_t process_list_get_inactive_index();


int32_t process_load_executable(struct ProcessControlBlock *pcb, struct FAT32DriverRequest request);

void process_initialize_context(struct ProcessControlBlock *pcb, void *entry_point);

#endif