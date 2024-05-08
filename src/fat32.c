#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '4', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

static struct FAT32DriverState fat32_driverstate;

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
    // Current Directory Entry
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

    // Parent Directory Entry
    struct FAT32DirectoryEntry* parent_dir = &dir_table->table[1];

    // ~ Get Parent Directory Cluster
    struct ClusterBuffer cb;
    memset(&cb, 0, CLUSTER_SIZE);
    read_clusters(&cb, parent_dir_cluster, 1);

    // ~ Copy Cluster to FAT32DirectoryTable
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
    // Write Boot Sector
    struct BlockBuffer boot_sector;
    memset(&boot_sector, 0, BLOCK_SIZE);
    memcpy(&boot_sector, fs_signature, BLOCK_SIZE);
    write_blocks(&boot_sector, BOOT_SECTOR, 1);

    // Create FAT32 FileAllocationTable
    struct FAT32FileAllocationTable* fat_table = &fat32_driverstate.fat_table;
    memset(fat_table, 0, CLUSTER_SIZE);
    
    fat_table->cluster_map[0] = CLUSTER_0_VALUE;
    fat_table->cluster_map[1] = CLUSTER_1_VALUE;
    fat_table->cluster_map[2] = FAT32_FAT_END_OF_FILE;

    // ~ Write FAT32 table to disk
    write_clusters(fat_table, FAT_CLUSTER_NUMBER, 1);

    // Create Root Directory
    struct FAT32DirectoryTable* dir_table = &fat32_driverstate.dir_table_buf;
    memset(dir_table, 0, CLUSTER_SIZE);

    // ~ Write directory name to disk
    struct BlockBuffer b;
    memset(&b, 0, BLOCK_SIZE);
    char name[] = "root"; 

    uint8_t i = 0;
    while (name[i] != '\0') { 
        b.buf[i] = name[i];   
        i++;
    }

    write_blocks(&b, cluster_to_lba(2), 1);

    // ~ Initialize directory
    init_directory_table(dir_table, name, ROOT_CLUSTER_NUMBER);

    // ~ Write directory to disk
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

int8_t read_directory(struct FAT32DriverRequest request) {
    // Get parent directory cluster
    struct FAT32DirectoryTable dir_table;
    memset(&dir_table, 0, CLUSTER_SIZE);
    read_clusters(&dir_table, request.parent_cluster_number, 1);

    // Iterate through all entries in the parent folder
    for (uint8_t i = 0; i < 64; i++) {
        struct FAT32DirectoryEntry dir_entry = dir_table.table[i];

        // Check if directory entry is empty
        if (dir_entry.user_attribute != UATTR_NOT_EMPTY) continue;
        
        // Check if dir_entry has the same name and ext
        if (!memcmp(&dir_entry.name, &request.name, 8) && !memcmp(&dir_entry.ext, &request.ext, 3)){
            // Check if entry is a directory
            if (dir_entry.attribute == ATTR_SUBDIRECTORY) {
                // Check if buffer is enough
                if (request.buffer_size < sizeof(struct FAT32DirectoryEntry)) {
                    return -1; // Buffer size is not enough
                }
                // Copy directory entry to buffer
                uint32_t cluster = ((uint32_t) dir_entry.cluster_high << 16) | dir_entry.cluster_low;
                read_clusters(request.buf, cluster, 1);
                // request.buf += sizeof(struct FAT32DirectoryEntry);
                return 0;
            }
            else{
                return 1; // Error Code -> Folder not found
            }
        }
    }
    return 2;
}

