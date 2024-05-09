#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

const uint8_t fs_signature[BLOCK_SIZE] = { // signature
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '4', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

static struct FAT32DriverState fat32_driverstate;

uint8_t count_empty_fat_slots(uint32_t* buf, uint8_t required) {
    uint8_t count = 0;
    uint32_t i = 0;

    while (count < required && i < CLUSTER_MAP_SIZE) {
        if (fat32_driverstate.fat_table.cluster_map[i] == FAT32_FAT_EMPTY_ENTRY) {
            buf[count++] = i;
        }
        i++;
    }
    return count;
}

uint8_t ceil(float n) {
    if ((int)n == n) return (int)n;
    return (int)n + 1;
}

uint8_t floor(float n) {
    if ((int)n == n) return (int)n;
    return (int)n - 1;
}

uint32_t cluster_to_lba(uint32_t cluster) {
    return CLUSTER_BLOCK_COUNT * cluster;
}

void write_clusters(const void *ptr, uint32_t cluster_number, uint8_t cluster_count) {
    write_blocks(ptr, cluster_to_lba(cluster_number), cluster_count * CLUSTER_BLOCK_COUNT);
}

void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count) {
    read_blocks(ptr, cluster_to_lba(cluster_number), cluster_count * CLUSTER_BLOCK_COUNT);
}

void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name, uint32_t parent_dir_cluster) {
    // current directory
    struct FAT32DirectoryEntry* current_dir = &dir_table->table[0];

    for (uint8_t l = 0; l < 8; l++) current_dir->name[l] = '\0';
    uint8_t i = 0;
    while (*name != '\0' && i < 8) {
        current_dir->name[i] = *name;
        i++;
        name++;
    }
    current_dir->attribute = ATTR_SUBDIRECTORY;
    current_dir->user_attribute = UATTR_NOT_EMPTY;

    // parent directory
    struct FAT32DirectoryEntry* parent_dir = &dir_table->table[1];

    // get parent directory cluster
    struct ClusterBuffer cb;
    memset(&cb, 0, CLUSTER_SIZE);
    read_clusters(&cb, parent_dir_cluster, 1);

    // copy parent directory name to current directory
    struct FAT32DirectoryTable t;
    memset(&t, 0, CLUSTER_SIZE);
    memcpy(&t, &cb, CLUSTER_SIZE);

    uint8_t j = 0;
    while (j < 8) {
        parent_dir->name[j] = t.table[0].name[j];
        j++;
    }

    parent_dir->attribute = ATTR_SUBDIRECTORY;
    parent_dir->user_attribute = UATTR_NOT_EMPTY;
    parent_dir->cluster_low = (uint16_t) (parent_dir_cluster & 0xFFFF);
    parent_dir->cluster_high = (uint16_t) ((parent_dir_cluster >> 16) & 0xFFFF);
}

bool is_empty_storage(void) {
    struct BlockBuffer boot_sector;
    read_blocks(&boot_sector, BOOT_SECTOR, 1);
    return memcmp(boot_sector.buf, fs_signature, BLOCK_SIZE);
}

void create_fat32(void) {
    // Create FAT32 Boot Sector
    struct BlockBuffer boot_sector;
    memset(&boot_sector, 0, BLOCK_SIZE);
    memcpy(&boot_sector, fs_signature, BLOCK_SIZE);
    write_blocks(&boot_sector, BOOT_SECTOR, 1);

    // Create FAT32 File Allocation Table
    struct FAT32FileAllocationTable* fat_table = &fat32_driverstate.fat_table;
    memset(fat_table, 0, CLUSTER_SIZE);
    
    fat_table->cluster_map[0] = CLUSTER_0_VALUE;
    fat_table->cluster_map[1] = CLUSTER_1_VALUE;
    fat_table->cluster_map[2] = FAT32_FAT_END_OF_FILE;

    write_clusters(fat_table, FAT_CLUSTER_NUMBER, 1);

    // Create FAT32 Root Directory Table
    struct FAT32DirectoryTable* dir_table = &fat32_driverstate.dir_table_buf;
    memset(dir_table, 0, CLUSTER_SIZE);

    struct BlockBuffer b;
    memset(&b, 0, BLOCK_SIZE);
    char name[] = "root"; 

    uint8_t i = 0;
    while (name[i] != '\0') { 
        b.buf[i] = name[i];   
        i++;
    }

    write_blocks(&b, cluster_to_lba(2), 1);

    init_directory_table(dir_table, name, ROOT_CLUSTER_NUMBER);

    // Write root directory table to disk
    write_clusters(dir_table, ROOT_CLUSTER_NUMBER, 1);
}

void initialize_filesystem_fat32(void) {
    if (is_empty_storage()) {
        create_fat32();
    } else {
        struct FAT32FileAllocationTable* fat_table = &fat32_driverstate.fat_table;
        memset(fat_table, 0, CLUSTER_SIZE);

        read_clusters(fat_table, FAT_CLUSTER_NUMBER, 1);
    }
}

int8_t read_directory(struct FAT32DriverRequest request) {
    // Get parent directory cluster
    struct FAT32DirectoryTable dir_table;
    memset(&dir_table, 0, CLUSTER_SIZE);
    read_clusters(&dir_table, request.parent_cluster_number, 1);

    // Iterate through all entries in the parent folder, check if an entry with the same name exists
    for (uint8_t i = 0; i < 64; i++) {
        struct FAT32DirectoryEntry dir_entry = dir_table.table[i];

        // Skip if entry is empty
        if (dir_entry.user_attribute != UATTR_NOT_EMPTY) continue;
        
        // Check if dir_entry has the same name and ext
        if (!memcmp(&dir_entry.name, &request.name, 8) && !memcmp(&dir_entry.ext, &request.ext, 3)){
            if (dir_entry.attribute == ATTR_SUBDIRECTORY) {
                // Check if buffer is enough
                if (request.buffer_size < sizeof(struct FAT32DirectoryEntry)) {
                    return -1; // Buffer size is not enough
                }
                // Read data from cluster
                uint32_t cluster = ((uint32_t) dir_entry.cluster_high << 16) | dir_entry.cluster_low;
                read_clusters(request.buf, cluster, 1);
                // Return 0 if success
                return 0;
            }
            else{
                return 1; // Return 1 if it's not a file
            }
        }
    }
    return 2; // Return 2 if file is not found
}

int8_t read(struct FAT32DriverRequest request) {
    // return 2 if parent cluster is invalid
    if (fat32_driverstate.fat_table.cluster_map[request.parent_cluster_number] != FAT32_FAT_END_OF_FILE){
        return 2;
    }

    // get parent directory cluster
    struct FAT32DirectoryTable dir_table;
    memset(&dir_table, 0, CLUSTER_SIZE);
    read_clusters(&dir_table, request.parent_cluster_number, 1);

    // iterate through all entries in the parent folder, check if an entry with the same name exists
    for (uint8_t i = 0; i < 64; i++) {
        struct FAT32DirectoryEntry dir_entry = dir_table.table[i];

        // check if dir_entry has the same name and ext
        if (!memcmp(&dir_entry.name, &request.name, 8) && !memcmp(&dir_entry.ext, &request.ext, 3)) {
            
            // check if dir_entry is a file
            if (dir_entry.attribute == 0x10){
                return 1; // return 1 if it's a directory
            }
            
            // check if buffer is enough
            if ((request.buffer_size / CLUSTER_SIZE) < ceil(dir_entry.filesize / (float) CLUSTER_SIZE)) { // return -1 if buffer size is not enough
                return -1; // return -1 if buffer size is not enough
            }

            // read data from cluster
            uint32_t current_cluster = (dir_entry.cluster_high << 16) | dir_entry.cluster_low;
            uint32_t clusters_read = 0;
            while (current_cluster != FAT32_FAT_END_OF_FILE) {
                read_clusters(request.buf + clusters_read * CLUSTER_SIZE, current_cluster, 1);
                current_cluster = fat32_driverstate.fat_table.cluster_map[current_cluster];
                clusters_read++;

            }
            return 0;
        }
    }
    return 2; // return 2 if file is not found
}

int8_t write(struct FAT32DriverRequest request) {
    // Return 2 if parent cluster is invalid
    if (fat32_driverstate.fat_table.cluster_map[request.parent_cluster_number] != FAT32_FAT_END_OF_FILE) return 2;
    
    // Get parent directory cluster
    struct FAT32DirectoryTable dir_table;
    memset(&dir_table, 0, CLUSTER_SIZE);
    read_clusters(&dir_table, request.parent_cluster_number, 1);

    uint8_t dirtable_empty_slot = 0;

    for (uint8_t i = 0; i < 64; i++) {
        struct FAT32DirectoryEntry dir_entry = dir_table.table[i];

        // Check if empty slot is found
        if (dir_entry.user_attribute != UATTR_NOT_EMPTY && dirtable_empty_slot == 0) {
            dirtable_empty_slot = i;
        }

        // Check if dir_entry has the same name and ext
        if (!memcmp(&dir_entry.name, &request.name, 8) && !memcmp(&dir_entry.ext, &request.ext, 3)) return 1;
    }

    // Return -1 if empty slot is not found
    if (dirtable_empty_slot == 0) return -1;

    // Calculate required cluster
    uint8_t required = ceil(request.buffer_size / (float) CLUSTER_SIZE);

    bool isFolder = false;
    if (request.buffer_size == 0) {
        isFolder = true;
        required += 1;
    }

    uint32_t slot_buf[required];
    uint8_t empty = count_empty_fat_slots(slot_buf, required);

    // Return -1 if empty slots is less than required
    if (empty < required) return -1;

    if (isFolder) {
        // Initialize child directory table
        struct FAT32DirectoryTable child_dir;
        memset(&child_dir, 0, CLUSTER_SIZE);
        init_directory_table(&child_dir, request.name, request.parent_cluster_number);

        // Write to cluster
        struct FAT32DirectoryEntry* child = &child_dir.table[0];
        child->cluster_low = slot_buf[0] & 0xFFFF;
        child->cluster_high = (slot_buf[0] >> 16) & 0xFFFF;

        write_clusters(&child_dir, slot_buf[0], 1);
        // Write data to FAT Table
        fat32_driverstate.fat_table.cluster_map[slot_buf[0]] = FAT32_FAT_END_OF_FILE;
        write_clusters(&(fat32_driverstate.fat_table), FAT_CLUSTER_NUMBER, 1);
    } else {
        // Fill empty slots in slot_buf
        for (uint8_t j = 0; j < required; j++) {
            uint32_t cluster_num = slot_buf[j];
            uint32_t next_cluster = (j < required - 1) ? slot_buf[j + 1] : FAT32_FAT_END_OF_FILE;

            // Write data to FAT Table
            fat32_driverstate.fat_table.cluster_map[cluster_num] = next_cluster;
            write_clusters(&(fat32_driverstate.fat_table), FAT_CLUSTER_NUMBER, 1);

            // Write data to cluster
            write_clusters(request.buf, cluster_num, 1);
            request.buf += CLUSTER_SIZE;
        }
    }

    // Create new entry
    struct FAT32DirectoryEntry entry = {
        .attribute = isFolder ? ATTR_SUBDIRECTORY : 0,
        .user_attribute = UATTR_NOT_EMPTY,

        .cluster_low = slot_buf[0] & 0xFFFF,
        .cluster_high = (slot_buf[0] >> 16) & 0xFFFF,
        .filesize = request.buffer_size
    };

    // Copy name and ext to entry
    for (uint8_t a = 0; a < 8; a++) entry.name[a] = request.name[a];
    for (uint8_t b = 0; b < 3; b++) entry.ext[b] = request.ext[b];

    // Write entry to directory table
    dir_table.table[dirtable_empty_slot] = entry;
    write_clusters(&dir_table, request.parent_cluster_number, 1);

    return 0;
}

int8_t delete(struct FAT32DriverRequest request) {
    if (fat32_driverstate.fat_table.cluster_map[request.parent_cluster_number] != FAT32_FAT_END_OF_FILE) return 2;
    
    // Get parent directory cluster
    struct FAT32DirectoryTable dir_table;
    memset(&dir_table, 0, CLUSTER_SIZE);
    read_clusters(&dir_table, request.parent_cluster_number, 1);

    for (uint8_t i = 0; i < 64; i++) {
        struct FAT32DirectoryEntry dir_entry = dir_table.table[i];

        if (!memcmp(&dir_entry.name, &request.name, 8) && !memcmp(&dir_entry.ext, &request.ext, 3)) {
            uint32_t current_cluster = (dir_entry.cluster_high << 16) | dir_entry.cluster_low;

            // Check if entry is a folder
            if (dir_entry.attribute == ATTR_SUBDIRECTORY) {
                // Get directory table from disk
                struct FAT32DirectoryTable entry_table;
                memset(&entry_table, 0, CLUSTER_SIZE);
                read_clusters(&entry_table, current_cluster, 1);

                // Check if folder is empty
                bool isEmpty = true;
                uint8_t j = 2;
                while (isEmpty && j < 64) {
                    struct FAT32DirectoryEntry child_entry = entry_table.table[j];

                    if (child_entry.user_attribute == UATTR_NOT_EMPTY) {
                        isEmpty = false;
                    }
                    j++;
                }

                // Return 2 if folder is not empty
                if (!isEmpty) return 2;
            }

            // Delete entry from directory table
            struct FAT32DirectoryEntry new_entry;
            memset(&new_entry, 0, sizeof(struct FAT32DirectoryEntry));

            dir_table.table[i] = new_entry;
            write_clusters(&dir_table, request.parent_cluster_number, 1);

            // Delete entry from FAT Table
            do {
                // Get next cluster
                uint32_t next_cluster = fat32_driverstate.fat_table.cluster_map[current_cluster];
                fat32_driverstate.fat_table.cluster_map[current_cluster] = FAT32_FAT_EMPTY_ENTRY;

                // Write FAT Table to disk
                struct ClusterBuffer empty_cluster;
                memset(&empty_cluster, 0, CLUSTER_SIZE);
                write_clusters(&empty_cluster, current_cluster, 1);

                // Move to next cluster
                current_cluster = next_cluster;
            } while (current_cluster != FAT32_FAT_END_OF_FILE);

            write_clusters(&(fat32_driverstate.fat_table), FAT_CLUSTER_NUMBER, 1);

            return 0; // return 0 if success
        }
    }

    return 1; // return 1 if file is not found
}
