#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/memory/paging.h"

__attribute__((aligned(0x1000))) static struct PageDirectory page_directory_list[PAGING_DIRECTORY_TABLE_MAX_COUNT] = {0};

static struct {
    bool page_directory_used[PAGING_DIRECTORY_TABLE_MAX_COUNT];
} page_directory_manager = {
    .page_directory_used = {false},
};

__attribute__((aligned(0x1000))) struct PageDirectory _paging_kernel_page_directory = {
    .table = {
        [0] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
        [0x300] = {
            .flag.present_bit       = 1,
            .flag.write_bit         = 1,
            .flag.use_pagesize_4_mb = 1,
            .lower_address          = 0,
        },
    }
};

static struct PageManagerState page_manager_state = {
    .page_frame_map = {
        [0]                            = true,
        [1 ... PAGE_FRAME_MAX_COUNT-1] = false
    },
    .free_page_frame_count = PAGE_FRAME_MAX_COUNT - 1
};

void update_page_directory_entry(
    struct PageDirectory *page_dir,
    void *physical_addr, 
    void *virtual_addr, 
    struct PageDirectoryEntryFlag flag
) {
    uint32_t page_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;
    page_dir->table[page_index].flag          = flag;
    page_dir->table[page_index].lower_address = ((uint32_t) physical_addr >> 22) & 0x3FF;
    flush_single_tlb(virtual_addr);
}

void flush_single_tlb(void *virtual_addr) {
    asm volatile("invlpg (%0)" : /* <Empty> */ : "b"(virtual_addr): "memory");
}



/* --- Memory Management --- */
bool paging_allocate_check(uint32_t amount) {
    if(page_manager_state.free_page_frame_count>=amount){
        return true;
    }
    return false;
}


bool paging_allocate_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    /**
     * TODO: Find free physical frame and map virtual frame into it
     * - Find free physical frame in page_manager_state.page_frame_map[] using any strategies
     * - Mark page_manager_state.page_frame_map[]
     * - Update page directory with user flags:
     *     > present bit    true
     *     > write bit      true
     *     > user bit       true
     *     > pagesize 4 mb  true
     */ 
    // Iterasi melalui array page_frame_map untuk mencari frame yang kosong
    for (size_t i = 0; i < PAGE_FRAME_MAX_COUNT; ++i) {
        if (!page_manager_state.page_frame_map[i]) {
            // Jika ditemukan frame yang kosong, maka lakukan pemetaan virtual-physical
            page_manager_state.page_frame_map[i] = true; // tandai frame sebagai digunakan
            struct PageDirectoryEntryFlag flag;
            flag.present_bit = 1;
            flag.write_bit = 1;
            flag.user = 1;
            flag.use_pagesize_4_mb = 1;
            update_page_directory_entry(page_dir, (void*)(i * PAGE_FRAME_SIZE), virtual_addr, flag);
            page_manager_state.free_page_frame_count--; // Kurangi jumlah frame yang tersedia
            return true; // Kembalikan true untuk menandakan alokasi berhasil
        }
    }
    return false; // Jika tidak ditemukan frame kosong, kembalikan false
}

bool paging_free_user_page_frame(struct PageDirectory *page_dir, void *virtual_addr) {
    /* 
     * TODO: Deallocate a physical frame from respective virtual address
     * - Use the page_dir.table values to check mapped physical frame
     * - Remove the entry by setting it into 0
     */
     // Hitung indeks entri dalam tabel direktori halaman berdasarkan alamat virtual
    uint32_t page_index = ((uint32_t) virtual_addr >> 22) & 0x3FF;

    // Periksa apakah entri di tabel direktori halaman tersebut ada
    if (page_dir->table[page_index].flag.present_bit) {
        // Bebaskan page frame yang terkait dengan alamat virtual
        uint32_t physical_addr = (page_dir->table[page_index].lower_address) << 22;
        page_manager_state.page_frame_map[physical_addr / PAGE_FRAME_SIZE] = false;

        // Hapus entri di tabel direktori halaman
        struct PageDirectoryEntryFlag flag;
        flag.present_bit = 0;
        flag.write_bit = 0;
        flag.user = 0;
        flag.use_pagesize_4_mb = 0;

        update_page_directory_entry(page_dir, 0, virtual_addr, flag);

        // Tambahkan jumlah frame yang tersedia kembali
        page_manager_state.free_page_frame_count++;

        return true; // Alokasi berhasil
    }

    return false; // Jika entri tidak ada, kembalikan false
}

struct PageDirectory* paging_create_new_page_directory(void) {
    for (size_t i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; ++i) {
        if (!page_directory_manager.page_directory_used[i]) {
            // Mark the page directory as used
            page_directory_manager.page_directory_used[i] = true;

            // Initialize the page directory
            struct PageDirectory *page_dir = &page_directory_list[i];
            memset(page_dir, 0, sizeof(struct PageDirectory));

            // Create a new page directory entry for the kernel higher half
            struct PageDirectoryEntry entry;
            entry.flag.present_bit = 1;
            entry.flag.write_bit = 1;
            entry.flag.use_pagesize_4_mb = 1;

            // Set the kernel page directory entry
            page_dir->table[0x300] = entry;

            return page_dir;
        }
    }
    return NULL; // No free page directory found
}

bool paging_free_page_directory(struct PageDirectory *page_dir) {
    for (size_t i = 0; i < PAGING_DIRECTORY_TABLE_MAX_COUNT; ++i) {
        if (&page_directory_list[i] == page_dir) {
            // Mark the page directory as unused
            page_directory_manager.page_directory_used[i] = false;

            // Clear all page directory entries
            memset(page_dir, 0, sizeof(struct PageDirectory));

            return true;
        }
    }
    return false; // Page directory not found
}

struct PageDirectory* paging_get_current_page_directory_addr(void) {
    uint32_t current_page_directory_phys_addr;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_page_directory_phys_addr): /* <Empty> */);
    uint32_t virtual_addr_page_dir = current_page_directory_phys_addr + KERNEL_VIRTUAL_ADDRESS_BASE;
    return (struct PageDirectory*) virtual_addr_page_dir;
}

void paging_use_page_directory(struct PageDirectory *page_dir_virtual_addr) {
    uint32_t physical_addr_page_dir = (uint32_t) page_dir_virtual_addr;
    // Additional layer of check & mistake safety net
    if ((uint32_t) page_dir_virtual_addr > KERNEL_VIRTUAL_ADDRESS_BASE)
        physical_addr_page_dir -= KERNEL_VIRTUAL_ADDRESS_BASE;
    __asm__  volatile("mov %0, %%cr3" : /* <Empty> */ : "r"(physical_addr_page_dir): "memory");
}