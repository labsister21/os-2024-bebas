#include "header/filesystem/fat32.h"
#include "header/stdlib/string.h"

const uint8_t fs_signature[BLOCK_SIZE] = {
    'C', 'o', 'u', 'r', 's', 'e', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',  ' ',
    'D', 'e', 's', 'i', 'g', 'n', 'e', 'd', ' ', 'b', 'y', ' ', ' ', ' ', ' ',  ' ',
    'L', 'a', 'b', ' ', 'S', 'i', 's', 't', 'e', 'r', ' ', 'I', 'T', 'B', ' ',  ' ',
    'M', 'a', 'd', 'e', ' ', 'w', 'i', 't', 'h', ' ', '<', '3', ' ', ' ', ' ',  ' ',
    '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '-', '2', '0', '2', '4', '\n',
    [BLOCK_SIZE-2] = 'O',
    [BLOCK_SIZE-1] = 'k',
};

static struct FAT32DriverState memory;

double ceil(double x)
{
    int intPart = (int)x;
    return (x > intPart) ? (double)(intPart + 1) : (double)intPart;
}

// Kalau misalnya directory table belum initialize
void create_fat32(void) {
    // Write fs_signature into boot sector
    //memcpy(BOOT_SECTOR, fs_signature, BLOCK_SIZE);
    write_blocks(fs_signature,BOOT_SECTOR,1);

    // Write proper FileAllocationTable into cluster number 1
    // Declare the missing variables

    memory.fat_table.cluster_map[0] = CLUSTER_0_VALUE;
    memory.fat_table.cluster_map[1] = CLUSTER_1_VALUE;
    //// notice : see init_directory table as to why i think we dont need this
    //memory.fat_table.cluster_map[2] = FAT32_FAT_END_OF_FILE;
    //// tldr : init also puts it in FAT for us

    //write FTA and DirTable to disk on CLUSTER 1 and CLUSTER 2
    //FAT into ClusterBuffer for cluster 1
    struct ClusterBuffer buff;
    memcpy(buff.buf,memory.fat_table.cluster_map,CLUSTER_SIZE);
    write_clusters(buff.buf,1,1);

    //DirTable into memory.dirtable
    //memcpy(memory.dir_table_buf.table,CLUSTER_SIZE);
    //write_clusters(buff.buf,2,1);


    // Initialize root directory table
    //karena ini root, parent nya adalah dirinya sendiri
    init_directory_table(&memory.dir_table_buf, "root", ROOT_CLUSTER_NUMBER);

}

bool is_empty_storage(void) {
    //return memcmp(BOOT_SECTOR, fs_signature, BLOCK_SIZE) != 0;
    uint8_t fs_signature_check[BLOCK_SIZE];
    read_blocks(fs_signature_check,BOOT_SECTOR,1);
    return memcmp(fs_signature_check,fs_signature,BLOCK_SIZE);
}


uint32_t cluster_to_lba(uint32_t cluster) {
    return cluster * CLUSTER_BLOCK_COUNT;
}


//Asumsi : Selalu ada empty space
void init_directory_table(struct FAT32DirectoryTable *dir_table, char *name, uint32_t parent_dir_cluster) {
    //find empty cluster in FAT
    uint32_t index = -1;
    for (int i = 1; i < CLUSTER_MAP_SIZE; i++){
        if (memory.fat_table.cluster_map[i] == FAT32_FAT_EMPTY_ENTRY){
            index = i;
            break;
        }
    }

    //set in fat
    memory.fat_table.cluster_map[index] = FAT32_FAT_END_OF_FILE;
    //write to drive
    memcpy(memory.cluster_buf.buf,memory.fat_table.cluster_map,CLUSTER_SIZE);
    write_clusters(memory.cluster_buf.buf,FAT_CLUSTER_NUMBER,1);
    
    // Initialize Entry-0: DirectoryEntry about itself
    struct FAT32DirectoryEntry self = {
        .attribute = ATTR_SUBDIRECTORY,
        .user_attribute = UATTR_NOT_EMPTY,
        .cluster_high = index >> 16 & 0xFFFF,
        .cluster_low = index & 0xFFFF,
        .filesize = 0
    };

    memcpy(self.name,name,8);

    struct FAT32DirectoryEntry parententry;
    //get parent dir cluster and add self as child to parent
    if (index != ROOT_CLUSTER_NUMBER) { //not root        
        //write entry in parent unless it is root (of index 0x2)
        //go to parent dir
        struct FAT32DirectoryTable parentdirtable;
        read_clusters(parentdirtable.table,parent_dir_cluster,1);

        //find empty slot
        int tempindex = -1;
        for (uint32_t i = 0; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry); i++){
            if (parentdirtable.table[i].user_attribute != UATTR_NOT_EMPTY){
                tempindex = i;
                break;
            }
        }

        //write to parent
        parentdirtable.table[tempindex] = self;

        //clear buffer
        memset(memory.cluster_buf.buf,0,CLUSTER_SIZE);
        memcpy(memory.cluster_buf.buf,parentdirtable.table,CLUSTER_SIZE);
        write_clusters(memory.cluster_buf.buf,parent_dir_cluster,1);

        parententry = parentdirtable.table[0];
    }
    else { //root
        parententry = self;
    }

    memcpy(parententry.name,"..\0\0\0\0\0\0",8);
    dir_table->table[0] = self;
    dir_table->table[1] = parententry;

    //write to cluster
    memset(memory.cluster_buf.buf,0,CLUSTER_SIZE);
    memcpy(memory.cluster_buf.buf,dir_table->table,sizeof(struct FAT32DirectoryEntry) * 2);
    write_clusters(memory.cluster_buf.buf,index,1);

}

void initialize_filesystem_fat32() {
    if (is_empty_storage()) {
        create_fat32();
    } else {
        // 2 THINGS 2 READ : FAT AND DIRTABLE
        //FAT
        struct ClusterBuffer buffer;
        read_clusters(buffer.buf,1,1);
        memcpy(memory.fat_table.cluster_map,buffer.buf,CLUSTER_SIZE);

        read_clusters(buffer.buf,2,1);
        memcpy(memory.dir_table_buf.table,buffer.buf,CLUSTER_SIZE);
    }
}

void read_clusters(void *ptr, uint32_t cluster_number, uint8_t cluster_count){
    uint8_t* buffer = (uint8_t*) ptr;
    read_blocks(buffer,cluster_to_lba(cluster_number),cluster_count*CLUSTER_BLOCK_COUNT);
};

void write_clusters(const void *ptr, uint32_t cluster_number, uint8_t cluster_count){
    uint8_t* buffer = (uint8_t*) ptr;
    write_blocks(buffer,cluster_to_lba(cluster_number),cluster_count*CLUSTER_BLOCK_COUNT);
};

/**
 * FAT32DriverRequest - Request for Driver CRUD operation
 * 
 * @param buf                   Pointer pointing to buffer
 * @param name                  Name for directory entry
 * @param ext                   Extension for file
 * @param parent_cluster_number Parent directory cluster number, for updating metadata
 * @param buffer_size           Buffer size, CRUD operation will have different behaviour with this attribute
 */

int8_t read_directory(struct FAT32DriverRequest request){
    //disinfect
    memset(&memory.dir_table_buf,'\0',64*sizeof(struct FAT32DirectoryEntry));
    // Membaca Directory Table
    read_clusters(&memory.dir_table_buf, request.parent_cluster_number, 1);

    // Mencari name yang sama
    char current[8] = ".\0\0\0\0\0\0";
    int idx = -1;
    if (memcmp(request.name,current,8) == 0){
        idx = 0;
    }
    else{
        for (uint32_t i = 1; i < 64; i++){
            if (memcmp(memory.dir_table_buf.table[i].name, request.name, 8) == 0){
                idx = i;
                break;
            }
        }
    }


    if (idx == -1){ // Folder TIDAK ditemukan
        return 2;

    }else{  // Kondisi Valid
        uint32_t entry = (memory.dir_table_buf.table[idx].cluster_high << 16) | (memory.dir_table_buf.table[idx].cluster_low);
        read_clusters(request.buf,entry, 1);
        return 0;
    }


    // TO DO: Handler Entry dengan nama sudah ada (yg nge return 1)


    return -1;
};


int8_t read(struct FAT32DriverRequest request){
    //disinfect
    memset(&memory.dir_table_buf,'\0',64*sizeof(struct FAT32DirectoryEntry));
    // Membaca DirectoryTable
    read_clusters(&memory.dir_table_buf, request.parent_cluster_number, 1);
    
    // Mencari name dan ext yang sama
    int idx = -1;
    for (uint32_t i = 0; i < 64; i++){
        if (memcmp(memory.dir_table_buf.table[i].name, request.name, 8) == 0 && memcmp(memory.dir_table_buf.table[i].ext, request.ext, 3) == 0){
            idx = i;
            break;
        }
    }
    
    
    if (idx == -1){ // File TIDAK ditemukan
        return 2;

    }else if(memory.dir_table_buf.table[idx].attribute == 1 || request.buffer_size == 0){   // Not a file
        return 1;

    }else{  // Kondisi Valid
        // Check if the entry is a file or not

        uint32_t cluster_count = (memory.dir_table_buf.table[idx].filesize / CLUSTER_SIZE) + 1;
        
        // checks if the file size is not a multiple of the cluster size
        if (cluster_count * CLUSTER_SIZE < memory.dir_table_buf.table[idx].filesize){
            cluster_count++;

        }else{ // reading of the file's data into a buffer

            uint32_t cluster_number = (memory.dir_table_buf.table[idx].cluster_high << 16) | memory.dir_table_buf.table[idx].cluster_low;
            for (uint32_t j = 0 ; j < cluster_count; j++){
                if (j == 0){    // handle first cluster
                    read_clusters(request.buf, cluster_number, 1);
                    cluster_number = memory.fat_table.cluster_map[cluster_number];

                }else{   // handle subsequent cluster
                    read_clusters(request.buf + (j * CLUSTER_SIZE), cluster_number, 1);
                    cluster_number = memory.fat_table.cluster_map[cluster_number];
                }
            }
        }

        return 0;
    }
    return -1;

};

int8_t write(struct FAT32DriverRequest request){

    // Read directory table of the parent cluster
    read_clusters(memory.dir_table_buf.table, request.parent_cluster_number, 1);
    // Read fat table to memory
    read_clusters(memory.fat_table.cluster_map,1,1);


    // Find the first empty entry in the File Allocation Table (FAT)
    int index = ROOT_CLUSTER_NUMBER + 1; //mulai dari 3
    while (memory.fat_table.cluster_map[index] != FAT32_FAT_EMPTY_ENTRY && index < CLUSTER_MAP_SIZE)
    {
        index++;
    }

    if (index == CLUSTER_MAP_SIZE){
        return -1; //FAT IS FULL
    }


    // Check if the file or directory already exists
    if(memory.dir_table_buf.table[0].user_attribute == UATTR_NOT_EMPTY){

        //test if file already exists
        for (uint32_t i = 0; i < 64; i++){
            if (memcmp(memory.dir_table_buf.table[i].name, request.name, 8) == 0 && memcmp(memory.dir_table_buf.table[i].ext, request.ext, 3) == 0){
                return 1;
            }
        }

        //handle directory
        if(request.buffer_size == 0){
            
            //check if folder dir is full
            int j = 0;
            while (memory.dir_table_buf.table[j].user_attribute == UATTR_NOT_EMPTY && j < 64){
                j++;
            }
            if (j == 64){
                return -1;
            }

            
            memset(&memory.dir_table_buf,'\0',64*sizeof(struct FAT32DirectoryEntry));
            init_directory_table(&memory.dir_table_buf,request.name, request.parent_cluster_number);
            return 0;
        }
        //handle file
        else{
            // Find the first empty directory entry
            int directory_idx = 0;
            while (memory.dir_table_buf.table[directory_idx].user_attribute == UATTR_NOT_EMPTY){
                directory_idx++;
            }

            int tempindex = index;
            int firstindex;
            int previndex = -1;

            int required_clusters = ceil((double) request.buffer_size / CLUSTER_SIZE);

            for (int i = 0; i < required_clusters; i++) //seek all empty usable slots
            {
                // Find the next empty cluster in the FAT
                while (memory.fat_table.cluster_map[tempindex] != FAT32_FAT_EMPTY_ENTRY && tempindex < CLUSTER_MAP_SIZE)
                {
                    tempindex++;
                }
                if (tempindex == CLUSTER_MAP_SIZE){//no space
                    return -1;
                }
                
                // Create a new directory entry for the file
                if (i == 0)
                {
                    //buat entry baru
                    struct FAT32DirectoryEntry new_entry = {
                        .user_attribute = UATTR_NOT_EMPTY,
                        .filesize = request.buffer_size,
                        .cluster_high = (tempindex >> 16) & 0xFFFF,
                        .cluster_low = tempindex & 0xFFFF,
                    };

                    memcpy(new_entry.name, request.name, 8);
                    memcpy(new_entry.ext, request.ext, 3);

                    firstindex = tempindex;
                    memory.dir_table_buf.table[directory_idx] = new_entry;
                    //jangan langsung, harusnya cek dulu kalau ada space cukup atau tidak
                    //write_clusters(&memory.dir_table_buf, request.parent_cluster_number, 1);
                }
                
                //write prev if any
                if (previndex != -1)
                {
                    memory.fat_table.cluster_map[previndex] = tempindex;
                }
                
                //kalau hanya satu file atau sudah sampai ahkir
                if (i == required_clusters - 1)
                {
                    memory.fat_table.cluster_map[tempindex] = FAT32_FAT_END_OF_FILE;
                }

                previndex = tempindex;
                tempindex++;
                
            } // Kalau sampai sini, successfully found cluster for all.

            //start by writing data first.
            tempindex = firstindex;
            int totalsize = request.buffer_size;
            for (int i = 0; i < required_clusters; i++){
                int writesize;
                if (totalsize >= CLUSTER_SIZE){
                    writesize = CLUSTER_SIZE;
                    totalsize -= CLUSTER_SIZE;
                }
                else{
                    writesize = totalsize;
                }

                memset(memory.cluster_buf.buf,0,CLUSTER_SIZE);

                memcpy(memory.cluster_buf.buf,request.buf + i * (int) CLUSTER_SIZE, writesize);
                write_clusters(memory.cluster_buf.buf, tempindex, 1);
                tempindex = memory.fat_table.cluster_map[tempindex];
            }

            //finally, save changes to fat table
            write_clusters(&memory.fat_table, FAT_CLUSTER_NUMBER, 1);
            write_clusters(&memory.dir_table_buf, request.parent_cluster_number, 1);
            return 0;
        }
    
    }
    return -1;
};

int8_t delete(struct FAT32DriverRequest request){
    // Read directory table of the parent cluster
    read_clusters(memory.dir_table_buf.table, request.parent_cluster_number, 1);
    // Read fat table to memory
    read_clusters(memory.fat_table.cluster_map,1,1);

    //location of where definition of file or folder is in parent dirtable
    int index = -1;
    int size;
    //check if file exists in parent dir
    for (uint32_t i = 0; i < 64; i++){
        if (memcmp(memory.dir_table_buf.table[i].name, request.name, 8) == 0 && memcmp(memory.dir_table_buf.table[i].ext, request.ext, 3) == 0){
            index = i;
            size = memory.dir_table_buf.table[i].filesize;
            break;
        }
    }

    if (index == -1){
        return 1;
    }

    //physical location
    uint32_t cluster = memory.dir_table_buf.table[index].cluster_low | memory.dir_table_buf.table[index].cluster_high << 16;
    if (cluster == ROOT_CLUSTER_NUMBER) {//deny operation
        return -1;
    }


    //if folder, then size = 0, otherwise, file.
    if (size == 0){
        //check current folder
        struct FAT32DirectoryTable temp;
        read_clusters(temp.table,cluster,1);
        for (uint32_t i = 2; i < CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry); i++){
            if (temp.table[i].user_attribute == UATTR_NOT_EMPTY){
                return 2;
            }
        }
        //safe to delete
        //nuke that shit
        memset(memory.cluster_buf.buf,0,CLUSTER_SIZE);
        write_clusters(memory.cluster_buf.buf,cluster,1);

        //delete from fatte
        memory.fat_table.cluster_map[cluster] = FAT32_FAT_EMPTY_ENTRY;
        write_clusters(memory.fat_table.cluster_map,FAT_CLUSTER_NUMBER,1);

    }
    else{
        //delete file
        bool done = false;
        int nextcluster = -1;
        int currentcluster = cluster;

        //purify clusterbuf
        memset(memory.cluster_buf.buf,0,CLUSTER_SIZE);
        while (!done){
            nextcluster = memory.fat_table.cluster_map[currentcluster];

            write_clusters(memory.cluster_buf.buf,currentcluster,1);

            if (nextcluster == FAT32_FAT_END_OF_FILE){
                done = true;
            }
            else{
                currentcluster = nextcluster;
            }
        }
        
    }

    //delete from parent entry
    int direntrysize = (int) sizeof(struct FAT32DirectoryEntry);
    memset(&memory.dir_table_buf.table[index],0,direntrysize);
    write_clusters(memory.dir_table_buf.table,request.parent_cluster_number,1);
    
    return 0;
};