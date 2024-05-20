#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"
#include "header/process/scheduler.h"

#define SHELL_MAX_LENGTH 256
#define SYSCALL_ENABLE_TYPING 7
#define SYSCALL_READ_CHAR 4
#define SYSCALL_WRITE_CHAR 5
#define SYSCALL_DISABLE_TYPING 8
#define COLOR_WHITE_ON_BLACK 0xF

char* itoa[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
    "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
    "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
    "40", "41", "42", "43", "44", "45", "46", "47", "48", "49"
};


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

// BIOS colors
#define BIOS_BLACK                     0x0
#define BIOS_BLUE                      0x1
#define BIOS_GREEN                     0x2
#define BIOS_CYAN                      0x3
#define BIOS_RED                       0x4
#define BIOS_MAGENTA                   0x5
#define BIOS_BROWN                     0x6
#define BIOS_LIGHT_GRAY                0x7
#define BIOS_DARK_GRAY                 0x8
#define BIOS_LIGHT_BLUE                0x9
#define BIOS_LIGHT_GREEN               0xA
#define BIOS_LIGHT_CYAN                0xB
#define BIOS_LIGHT_RED                 0xC
#define BIOS_LIGHT_MAGENTA             0xD
#define BIOS_YELLOW                    0xE
#define BIOS_WHITE                     0xF

// Extended scancode handling
#define EXT_BUFFER_NONE               0
#define EXT_BUFFER_UP                 1
#define EXT_BUFFER_DOWN               2
#define EXT_BUFFER_LEFT               3
#define EXT_BUFFER_RIGHT              4

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


char* listName[100];
uint32_t listCluster[100];
uint32_t id = 0;

uint32_t idx = 0;
uint32_t clusters[100];

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx)
{
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

uint32_t strlen(char *text)
{
    int retval = 0;
    while (text[retval] != '\0')
    {
        retval++;
    }
    return retval;
}

void invert_memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    uint8_t *dstbuf = (uint8_t *)dest;
    const uint8_t *srcbuf = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++)
        dstbuf[i] = srcbuf[n - i - 1];
}

uint32_t get_parent_directory(uint32_t current_cluster) {
    struct FAT32DirectoryTable buff;
    uint8_t retcode;

    // Get the directory table of the current directory
    struct FAT32DriverRequest req = {
        .buf = buff.table,
        .name = ".\0\0\0\0\0\0\0",
        .ext = "\0\0\0",
        .parent_cluster_number = current_cluster,
        .buffer_size = 0x0,
    };
    syscall(1, (uint32_t)&req, (uint32_t)&retcode, 0);

    // Return the cluster number of the parent directory
    return buff.table[1].cluster_low | buff.table[1].cluster_high << 16;
}

// void print_dir_tree(char* current_folder_name ,uint32_t cluster_number, char* text_pointer){
//     char temp[SHELL_MAX_LENGTH/2];
//     // char current_folder[9] = "\0\0\0\0\0\0\0\0";
//     uint32_t current_cluster = cluster_number;
//     uint16_t prog = 0;
//     struct FAT32DirectoryTable buff;
//     //asumsi selalu exist
//     // memcpy(current_folder,current_folder_name,strlen(current_folder_name));
//     //move current_folder_name first
//     invert_memcpy(temp,current_folder_name,strlen(current_folder_name));
//     prog += strlen(current_folder_name);

//     while(current_cluster != ROOT_CLUSTER_NUMBER){
//         memset(temp + sizeof(char) * (prog),'\\',1);
//         prog++;
//         uint8_t retcode;
//         //get parent of current folder
//         struct FAT32DriverRequest req = {
//             .buf                   = buff.table,
//             .name                  = "..\0\0\0\0\0\0",
//             .ext                   = "\0\0\0",
//             .parent_cluster_number = current_cluster,
//             .buffer_size           = 0x0,
//         };
//         syscall(1,(uint32_t) &req, (uint32_t) &retcode, 0);

//         //add name to temp
//         invert_memcpy(temp + sizeof(char) * (prog),buff.table[0].name,strlen(buff.table[0].name));
//         prog += strlen(buff.table[0].name);

//         current_cluster = buff.table[0].cluster_low | buff.table[0].cluster_high  << 16;

//         //ALT ROOT CHECK
//         // if ((buff.table[0].cluster_low == buff.table[1].cluster_low)&&(buff.table[0].cluster_high == buff.table[1].cluster_high)){
//         //     break;
//         // }
//     }

//     invert_memcpy(text_pointer,temp,prog);
// }

void puts(char *text, uint32_t color)
{
    syscall(6, (uint32_t)text, strlen(text), color);
}

void print_dir_tree(uint32_t cluster_number, char *text_pointer) {
    char temp[SHELL_MAX_LENGTH / 2] = {0};
    uint32_t current_cluster = cluster_number;
    int prog = 0;
    struct FAT32DirectoryTable buff;

    do {
        uint8_t retcode;
        // Get current directory information
        struct FAT32DriverRequest req = {
            .buf = buff.table,
            .name = ".\0\0\0\0\0\0\0",
            .ext = "\0\0\0",
            .parent_cluster_number = current_cluster,
            .buffer_size = sizeof(buff.table),
        };
        syscall(1, (uint32_t)&req, (uint32_t)&retcode, 0);

        if (retcode != 0) {
            puts("Error: Unable to read directory.", 0xE);
            return; // Error handling: return if syscall fails
        }

        // Ensure name is null-terminated
        char name[9];
        strncpy(name, buff.table[0].name, 8);
        name[8] = '\0';

        // Add name to temp
        int name_len = strlen(name);
        memcpy(temp + prog, name, name_len);
        prog += name_len;

        // Add backslash separator if not root
        if (!((buff.table[0].cluster_low == buff.table[1].cluster_low) && (buff.table[0].cluster_high == buff.table[1].cluster_high))) {
            temp[prog] = '\\';
            prog++;
        }

        // Get parent cluster number
        current_cluster = buff.table[1].cluster_low | (buff.table[1].cluster_high << 16);

    } while (current_cluster != 0x2); // Stop when we reach the root

    // Reverse the directory path stored in temp
    char *start = temp;
    char *end = temp + prog - 1;

    // Reverse the entire string first
    while (start < end) {
        char temp_char = *start;
        *start = *end;
        *end = temp_char;
        start++;
        end--;
    }

    // Reverse each directory name back to correct order
    start = temp;
    while (start < temp + prog) {
        end = strchr(start, '\\');
        if (!end) end = temp + prog;

        // Reverse the directory name
        char *dir_start = start;
        char *dir_end = end - 1;
        while (dir_start < dir_end) {
            char temp_char = *dir_start;
            *dir_start = *dir_end;
            *dir_end = temp_char;
            dir_start++;
            dir_end--;
        }
        start = end + 1;
    }

    // Copy the reversed string to the output and add a null character at the end
    memcpy(text_pointer, temp, prog);
    text_pointer[prog] = '\0';
}

int8_t read_directory_interface(struct FAT32DirectoryTable *buf, char *folder_name, uint8_t len, uint16_t cluster)
{
    struct FAT32DriverRequest req = {
        .buf = buf->table,
        .name = "\0\0\0\0\0\0\0",
        .parent_cluster_number = cluster,
        .buffer_size = 0,
    };
    memcpy(req.name, folder_name, len);
    int8_t retcode;
    syscall(1, (uint32_t)&req, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        // Check if the directory exists
        if (buf->table[0].name[0] == (char)0xE5 || buf->table[0].name[0] == 0x00)
        {
            puts("Directory not found!", 0x4);
            retcode = -1;  // Set retcode to a non-zero value to indicate failure
        }
    }
    return retcode;
};

char *strcat(char *dest, const char *src)
{
    size_t dest_len = strlen(dest);
    size_t i;

    for (i = 0; src[i] != '\0'; i++)
    {
        dest[dest_len + i] = src[i];
    }

    dest[dest_len + i] = '\0';

    return dest;
}

bool is_in_directory(uint16_t parent_cluster, char *name, char *found_path)
{
    // Special case for root directory
    if (strcmp(name, "/") == 0 || strcmp(name, "") == 0) {
        strcpy(found_path, "/");
        return true;
    }

    struct FAT32DirectoryTable dirbuffer;
    struct FAT32DriverRequest request = {
        .buf = dirbuffer.table,
        .buffer_size = 0,
        .parent_cluster_number = parent_cluster,
        .name = ".\0\0\0\0\0\0"};

    uint8_t retval;
    syscall(1, (uint32_t)&request, (uint32_t)&retval, 0);

    if (retval != 0)
    {
        return false;
    }
    return true;
}

bool find_in_directory(uint16_t parent_cluster, char *name, char *found_path)
{
    // Special case for root directory
    if (strcmp(name, "/") == 0 || strcmp(name, "") == 0) {
        strcpy(found_path, "/");
        return true;
    }

    struct FAT32DirectoryTable dirbuffer;
    struct FAT32DriverRequest request = {
        .buf = dirbuffer.table,
        .buffer_size = 0,
        .parent_cluster_number = parent_cluster,
        .name = ".\0\0\0\0\0\0"};

    uint8_t retval;
    syscall(1, (uint32_t)&request, (uint32_t)&retval, 0);

    if (retval != 0)
    {
        return false;
    }

    size_t found_path_len = strlen(found_path);

    for (int i = 2; i < 64; i++)
    {
        // Trim trailing spaces from name in directory
        char trimmed_name[12];
        memcpy(trimmed_name, dirbuffer.table[i].name, 11);
        trimmed_name[11] = '\0'; // ensure null-terminated
        char *end = trimmed_name + strlen(trimmed_name) - 1;
        while (end > trimmed_name && *end == ' ') {
            *end-- = '\0';
        }

        if (strcmp(trimmed_name, name) == 0)
        {
            // Match found, construct the path
            print_dir_tree(dirbuffer.table[i].cluster_low | (dirbuffer.table[i].cluster_high << 16), found_path);
            // If it's a directory, print its contents
            if (dirbuffer.table[i].attribute & 0x10)
            {
                char output[SHELL_MAX_LENGTH];
                puts(output, 0xF); // Replace COLOR_WHITE with the desired color

                for (int j = 2; j < 64; j++)
                {
                    if (dirbuffer.table[j].name[0] != '\0')
                    {
                        puts(output, 0xF); // Replace COLOR_WHITE with the desired color
                    }
                }
            }

            return true;
        }

        if (dirbuffer.table[i].attribute & 0x10)
        {
            // Directory, search recursively
            char new_path[SHELL_MAX_LENGTH];
            memcpy(new_path, found_path, strlen(found_path));
            if (found_path_len > 0 && found_path[found_path_len - 1] != '/')
            {
                strcat(new_path, "/"); // Tambahkan pemisah jika belum ada
                found_path_len++;
            }
            size_t name_len = strlen(dirbuffer.table[i].name);
            if (found_path_len + name_len < SHELL_MAX_LENGTH)
            {
                strcat(new_path, dirbuffer.table[i].name);

                if (find_in_directory(dirbuffer.table[i].cluster_low | (dirbuffer.table[i].cluster_high << 16), name, new_path))
                {
                    memcpy(found_path, new_path, strlen(new_path) + 1); // +1 to include null terminator
                    return true;
                }
            }
        }
    }

    return false;
}

void parseCommand(char buffer[SHELL_MAX_LENGTH], uint16_t *parent_cluster)
{
    char *command = strtok(buffer, " ");
    char *argument = strtok(NULL, " ");
    char* com = (char * ) buffer + 3;
    if (strcmp(command, "cd") == 0)
    {   
        if (memcmp(com, "..", 2)== 0){
            if(idx == 0){
                puts("Already in root", 0x4);
                return;
            }
            idx--;
            puts("Successfully changed directory.", 0xF);
            *parent_cluster = clusters[idx];
            return;
        }
        if (argument == NULL || strlen(argument) == strspn(argument, " \t\n\r"))
        {
            return;
        }

        uint32_t retcode = 0;
        uint8_t progress = 3;
        uint16_t current_cluster = *parent_cluster;
        struct FAT32DirectoryTable dirbuffer;
        char splitter = '\\';
        bool directory_found = false;

        while (retcode == 0 && strlen(buffer + progress) > 0)
        {
            uint8_t current_len = 0;
            while (buffer[progress+current_len] != splitter && current_len < 8 && strlen(buffer+progress+current_len) > 0)
            {
                current_len++;
            }

            if (current_len == 8 && buffer[progress+current_len] != splitter)
            {
                puts("File name too long!", 0x4);
                break;
            }
            else
            {
                retcode = read_directory_interface(&dirbuffer,buffer+progress,current_len,current_cluster);
                if (retcode != 0)
                {
                    break;
                }

                // Check if the directory exists
                if (dirbuffer.table[0].name[0] != '\0')
                {
                    current_cluster = dirbuffer.table[0].cluster_low | (dirbuffer.table[0].cluster_high << 16);
                    directory_found = true;
                }
            }

            progress += current_len + 1;
        }

        if (!directory_found)
        {
            puts("Directory not found!", 0x4);
        }
        else if (retcode == 0)
        {   
            idx++;
            clusters[idx] = current_cluster;
            *parent_cluster = current_cluster;
            puts("Successfully changed directory.", 0xF);
        }
    }
    else if (strcmp(command, "ls") == 0) // ls command
    {
        struct FAT32DirectoryTable dtable;
        struct FAT32DriverRequest request = {
            .buf = &dtable,
            .buffer_size = 0,
            .parent_cluster_number = *parent_cluster,
            .name = ".\0\0\0\0\0\0"};

        uint8_t retval = 0;

        syscall(1, (uint32_t)&request, (uint32_t)&retval, 0);

        if (argument != NULL && strlen(argument) > 0)
        {
            for (int i = 0; i < 64; i++)
            {
                if (dtable.table[i].user_attribute != 0x00000000 && strcmp(dtable.table[i].name, argument) == 0)
                {
                    syscall(6, (uint32_t)dtable.table[i].name, (uint32_t)strlen(dtable.table[i].name), 0xF);
                    return;
                }
            }
            puts("Not found", 0x4);
            return;
        }

        for (int i = 0; i < 64; i++)
        {
            if (dtable.table[i].user_attribute != 0x00000000)
            {
                // Skip the "." and ".." entries
                if (memcmp(dtable.table[i].name, ".", 1) == 0 || memcmp(dtable.table[i].name, "..", 2) == 0)
                {
                    continue;
                }
                int len = strlen(dtable.table[i].name);
                char *ext = dtable.table[i].ext;

                // Check if it's a folder (directory)
                if ((dtable.table[i].attribute & 0x10) != 0)
                {
                    syscall(6, (uint32_t)dtable.table[i].name, (uint32_t)len, 0xF);
                }
                else
                {
                    if (ext[0] == '\0')
                    {
                        syscall(6, (uint32_t)dtable.table[i].name, (uint32_t)len, 0xF);
                    }
                    else
                    {
                        syscall(6, (uint32_t)dtable.table[i].name, (uint32_t)len, 0xF);
                        syscall(6, (uint32_t) ".", (uint32_t)1, 0xF);
                    }
                    syscall(6, (uint32_t)ext, (uint32_t)3, 0xF);
                }
                // Add a space after each name
                syscall(6, (uint32_t) " ", (uint32_t)1, 0xF);
            }
        }
    }
    else if (memcmp((char *)buffer, "mkdir", 5) == 0) // mkdir
    {
        // Ambil nama folder dari buffer command setelah "mkdir "
        char folder_name[9]; // nama folder harus 8 karakter + null-terminator
        memcpy(folder_name, buffer + 6, 8);
        folder_name[8] = '\0'; // pastikan null-terminator

        // Buat struct FAT32DriverRequest untuk membuat folder baru
        struct FAT32DriverRequest request = {
            .parent_cluster_number = *parent_cluster,
            .buffer_size = 0,
        };
        memcpy(request.name, folder_name, strlen(folder_name));

        int8_t retcode;
        // Panggil syscall untuk membuat folder
        syscall(2, (uint32_t)&request, (uint32_t)&retcode, 0);

        // Tampilkan pesan sesuai hasil operasi
        if (retcode == 0)
        {
            puts("Folder created successfully", 0x2);
        }
        else if (retcode == 1)
        {
            puts("Folder already exists", 0x4);
        }
        else if (retcode == 2)
        {
            puts("Invalid parent cluster", 0x4);
        }
        else
        {
            puts("Unknown error", 0x4);
        }
    }
    else if (strcmp(command, "cat") == 0) ////cat
    {
        struct ClusterBuffer cl_b[4] = {0}; // Let's say a file at most would eat up to 4 clusters
        struct FAT32DriverRequest request = {
            .buf                    = &cl_b,
            .ext                    = "\0\0\0",
            .parent_cluster_number  = *parent_cluster,
            .buffer_size = CLUSTER_SIZE * 4,
        };
        int32_t retcode = 0;
        int nameLen = 0;
        for(size_t i = 0; i < strlen(argument) ; i++){
            if(argument[i] == '.'){
                request.ext[0] = argument[i+1];
                request.ext[1] = argument[i+2];
                request.ext[2] = argument[i+3];
                break;
            }else{
                nameLen++;
            }
        }

        memcpy(request.name, argument, nameLen);

        syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);
            
        if (retcode == 3) {
            puts("The file size is too big to read!", 0x4);
            return;
        } else if (retcode == 2) {
            puts("Can't find the file!", 0x4);
            return;
        } else if (retcode == 1) {
            puts("Not a file!", 0x4);
            return;
        } else if (retcode == -1) {
            puts("An error occured!", 0x4);
            return;
        }

        for (int j = 0; j < 2; j++) {
            syscall(6, (uint32_t) &cl_b[j], CLUSTER_SIZE, 0xf);
        }
        syscall(5, (uint32_t) "\n", 0xf, 0);
    }
    else if (memcmp((char * ) buffer, "cp", 2) == 0){
        struct ClusterBuffer clusbuff = {0};
        struct FAT32DriverRequest request = {
            .buf = &clusbuff,
            .parent_cluster_number = *parent_cluster,
            .buffer_size = 0,
        };
        request.buffer_size = 5*CLUSTER_SIZE;
        int nameLen = 0;
        char* itr = (char * ) buffer + 3;
        for(size_t i = 0; i < strlen(itr) ; i++){
            if(itr[i] == '.'){
                request.ext[0] = itr[i+1];
                request.ext[1] = itr[i+2];
                request.ext[2] = itr[i+3];
                break;
            }else{
                nameLen++;
            }
        }

        memcpy(request.name, (void *) (buffer + 3), nameLen);
        int32_t retcode;

        struct FAT32DriverRequest request2 = {
            .buf                   = &clusbuff,
            .parent_cluster_number = *parent_cluster,
            .buffer_size           = 0,
        };

        char* itr2 = (char * ) buffer + 8 +nameLen;

        if(request.ext[0] == 0x0){
            itr2 = (char * ) buffer + 4 +nameLen;
        }

        syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);

         /* Read the Destination Folder to paste */
        if (retcode == 0)
            puts("Read success", 0x2);
        else if (retcode == 1){
            puts("Not a file", 0x4);
            return;
        }
        else if (retcode == 2){
            puts("File Not found", 0x4);
            return;
        }
        else{
            puts("Unknown error", 0x4);
            return;
        }
        
        memcpy(request2.name, itr2, 8);

        int32_t retcode2;
        struct FAT32DirectoryTable table = {};
        request2.buf = &table;
        syscall(1, (uint32_t) &request2, (uint32_t) &retcode, 0);

        for(int i = 0; i < 64; i++){
            if(memcmp(table.table[i].name, request2.name, strlen((char *) request2.name)) == 0){
                request.parent_cluster_number = (table.table[i].cluster_high << 16) | table.table[i].cluster_low;
                syscall(2, (uint32_t) &request, (uint32_t) &retcode2, 0);
                break;
            }
        }

        if (retcode2 == 0)
            puts("Write success", 0x2);
        else if (retcode2 == 1)
            puts("File/Folder already exist", 0x4);
        else if (retcode2 == 2)
            puts("Invalid parent cluster", 0x4);
        else
            puts("Target folder not found", 0x4);

    }
    
    else if (strcmp(command, "rm") == 0) ////rm
    {
        if (argument == NULL || strlen(argument) == strspn(argument, " \t\n\r"))
        {
            return;
        }
        // struct FAT32DirectoryEntry entry;
        char found_path[SHELL_MAX_LENGTH] = "";
        if (is_in_directory(*parent_cluster, argument, found_path))
        {
            struct FAT32DriverRequest req = {
                .parent_cluster_number = *parent_cluster,
                .ext = "\0\0\0",
            };
            memcpy(req.name, argument, strlen(argument));
            uint8_t retval;
            syscall(3, (uint32_t)&req, (uint32_t)&retval, 0);
            if (retval == 0)
            {
                puts("File or directory removed successfully.", 0x0F);
            }
            else
            {
                puts("Failed to remove file or directory.", 0x4);
            }
        }
        else
        {
            puts("File or directory not found.\n", 0x4);
        }
    }
    else if (strcmp(command, "mv") == 0) ////mv
    {
        // mv : Memindah dan merename lokasi file/folder
        if (argument == NULL || strlen(argument) == strspn(argument, " \t\n\r"))
        {
            puts("mv: missing file operand", 0x4);
            return;
        }

        struct ClusterBuffer clusbuff = {0};
        struct FAT32DriverRequest request = {
            .buf = &clusbuff,
            .parent_cluster_number = *parent_cluster,
            .buffer_size = 0,
        };
        request.buffer_size = 5*CLUSTER_SIZE;
        int nameLen = 0;
        char* itr = (char * ) buffer + 3;
        for(size_t i = 0; i < strlen(itr) ; i++){
            if(itr[i] == '.'){
                request.ext[0] = itr[i+1];
                request.ext[1] = itr[i+2];
                request.ext[2] = itr[i+3];
                break;
            }else{
                nameLen++;
            }
        }

        memcpy(request.name, (void *) (buffer + 3), nameLen);
        int32_t retcode;

        struct FAT32DriverRequest request2 = {
            .buf                   = &clusbuff,
            .parent_cluster_number = *parent_cluster,
            .buffer_size           = 0,
        };

        char* itr2 = (char * ) buffer + 8 +nameLen;

        if(request.ext[0] == 0x0){
            itr2 = (char * ) buffer + 4 +nameLen;
        }

        syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);

         /* Read the Destination Folder to paste */

        if (retcode == 1){
            puts("Not a file", 0x4);
            return;
        }
        else if (retcode == 2){
            puts("File Not found", 0x4);
            return;
        }
        else{
            puts("Unknown error", 0x4);
            return;
        }
        
        memcpy(request2.name, itr2, 8);

        int32_t retcode2;
        struct FAT32DirectoryTable table = {};
        request2.buf = &table;
        syscall(1, (uint32_t) &request2, (uint32_t) &retcode, 0);

        for(int i = 0; i < 64; i++){
            if(memcmp(table.table[i].name, request2.name, strlen((char *) request2.name)) == 0){
                request.parent_cluster_number = (table.table[i].cluster_high << 16) | table.table[i].cluster_low;
                syscall(2, (uint32_t) &request, (uint32_t) &retcode2, 0);
                break;
            }
        }

        if (retcode2 == 1){
            puts("File/Folder already exist", 0x4);
            return;
        }
            
        else if (retcode2 == 2){
            puts("Invalid parent cluster", 0x4);
            return;
        }
            
        else{
            puts("Target folder not found", 0x4);
            return;
        }
            
        
        struct FAT32DriverRequest request3 = {
            .buf = &clusbuff,
            .parent_cluster_number = *parent_cluster,
            .buffer_size = 0,
        };
        request3.buffer_size = 5*CLUSTER_SIZE;
        nameLen = 0;
        itr = (char * ) buffer + 3;
        for(size_t i = 0; i < strlen(itr) ; i++){
            if(itr[i] == '.'){
                request3.ext[0] = itr[i+1];
                request3.ext[1] = itr[i+2];
                request3.ext[2] = itr[i+3];
                break;
            }else{
                nameLen++;
            }
        }

        memcpy(request3.name, (void *) (buffer + 3), nameLen);
        int32_t retcode3;
        syscall(3, (uint32_t) &request3, (uint32_t) &retcode3, 0);
        if (retcode3 == 0)
            puts("Move Success", 0x2);
        else if (retcode3 == 1)
            puts("File/Folder Not Found", 0x4);
        else if (retcode3 == 2)
            puts("Folder is empty", 0x4);
        else
            puts("Unknown Error", 0x4);

    }
    else if (memcmp((char *)buffer, "find", 4) == 0) ////find
    {
        char name[SHELL_MAX_LENGTH];
        memcpy(name, buffer + 5, SHELL_MAX_LENGTH - 1);
        name[SHELL_MAX_LENGTH - 1] = '\0'; // ensure null-terminated

        char found_path[SHELL_MAX_LENGTH] = "";
        if (find_in_directory(*parent_cluster, name, found_path))
        {
            puts(found_path, 0x2);
        }
        else
        {
            puts("Not found", 0x4);
        }
    }
    else if (strcmp(command, "ps") == 0) ////findmake
    {
    // Get max process count
    uint32_t max_process_count;
    syscall(SYSCALL_GET_MAX_PROCESS_COUNT, (uint32_t) &max_process_count, 0, 0);

    // Newline struct
    struct SyscallPutsArgs newline = {
        .buf = "\n",
        .count = strlen(newline.buf),
        .fg_color = 0x0,
        .bg_color = 0x0
    };

    // Whitespace struct
    struct SyscallPutsArgs whitespace = {
        .buf = " ",
        .count = strlen(whitespace.buf),
        .fg_color = 0x0,
        .bg_color = 0x0
    };
    uint32_t COL_NAME = 11;
    uint32_t COL_STATE = 23;
    uint32_t COL_FRAME_COUNT = 36;

    // Print header
    struct SyscallPutsArgs header = {
        .buf = "PID        Name        State        Frame Count",
        .count = strlen(header.buf),
        .fg_color = BIOS_YELLOW,
        .bg_color = 0x0
    };
    syscall(SYSCALL_PUTS, (uint32_t) &header, 0, 0);

    // Print process info
    uint32_t col;
    for (uint32_t i = 0; i < max_process_count; i++) {
        // Create process_info struct
        struct SyscallProcessInfoArgs process_info = {
            .pid = i,
        };

        // Read i-th process info
        syscall(SYSCALL_GET_PROCESS_INFO, (uint32_t) &process_info, 0, 0);

        // Print process info if exists at i-th slot
        if (process_info.process_exists) {
            // Print newline
            syscall(SYSCALL_PUTS, (uint32_t) &newline, 0, 0);

            // PID
            struct SyscallPutsArgs pid = {
                .buf = itoa[process_info.pid],
                .count = strlen(pid.buf),
                .fg_color = BIOS_WHITE,
                .bg_color = 0x0
            };
            syscall(SYSCALL_PUTS, (uint32_t) &pid, 0, 0);

            do {
                syscall(SYSCALL_PUTS, (uint32_t) &whitespace, 0, 0);
                syscall(SYSCALL_GET_CURSOR_COL, (uint32_t) &col, 0, 0);
            } while (col < COL_NAME);

            // Process name
            struct SyscallPutsArgs name = {
                .buf = process_info.name,
                .count = strlen(name.buf),
                .fg_color = BIOS_LIGHT_GREEN,
                .bg_color = 0x0
            };
            syscall(SYSCALL_PUTS, (uint32_t) &name, 0, 0);

            do {
                syscall(SYSCALL_PUTS, (uint32_t) &whitespace, 0, 0);
                syscall(SYSCALL_GET_CURSOR_COL, (uint32_t) &col, 0, 0);
            } while (col < COL_STATE);

            // Process state
            struct SyscallPutsArgs state = {
                .buf = process_info.state,
                .count = strlen(state.buf),
                .fg_color = BIOS_LIGHT_CYAN,
                .bg_color = 0x0
            };
            syscall(SYSCALL_PUTS, (uint32_t) &state, 0, 0);

            do {
                syscall(SYSCALL_PUTS, (uint32_t) &whitespace, 0, 0);
                syscall(SYSCALL_GET_CURSOR_COL, (uint32_t) &col, 0, 0);
            } while (col < COL_FRAME_COUNT);

            // Process frame count
            struct SyscallPutsArgs frame_count = {
                .buf = itoa[process_info.page_frame_used_count],
                .count = strlen(frame_count.buf),
                .fg_color = BIOS_LIGHT_BLUE,
                .bg_color = 0x0
            };
            syscall(SYSCALL_PUTS, (uint32_t) &frame_count, 0, 0);
        }
    }
}
    else if (strcmp(command, "exec")) {
        
    }
    else
    {
        puts("Command not found", 0x4);
    }
}

void clearBuffer(char *buffer, int length)
{
    for (int i = 0; i < length; i++)
    {
        buffer[i] = '\0';
    }
}

void handleInput(char *inputBuffer, int *inputLength)
{
    char charBuffer;
    while (true)
    {
        syscall(SYSCALL_READ_CHAR, (uint32_t)&charBuffer, 0, 0);
        if (charBuffer)
        {
            switch (charBuffer)
            {
            case '\n':
                syscall(SYSCALL_WRITE_CHAR, (uint32_t)&charBuffer, COLOR_WHITE_ON_BLACK, 0);
                return;
            case '\b':
                if (*inputLength > 0)
                {
                    inputBuffer[*inputLength] = '\0';
                    (*inputLength)--;
                    syscall(SYSCALL_WRITE_CHAR, (uint32_t)&charBuffer, COLOR_WHITE_ON_BLACK, 0);
                }
                break;
            default:
                if (*inputLength < SHELL_MAX_LENGTH - 1)
                {
                    syscall(SYSCALL_WRITE_CHAR, (uint32_t)&charBuffer, COLOR_WHITE_ON_BLACK, 0);
                    inputBuffer[*inputLength] = charBuffer;
                    (*inputLength)++;
                }
                break;
            }
        }
    }
}

int shell(void)
{
    char inputBuffer[SHELL_MAX_LENGTH];
    int inputLength = 0;
    char current_dir[SHELL_MAX_LENGTH / 2];
    uint16_t current_cluster_pos = 2;
    clusters[0] = (uint32_t) ROOT_CLUSTER_NUMBER;

    while (true)
    {
        clearBuffer(current_dir, SHELL_MAX_LENGTH / 2);
        print_dir_tree(current_cluster_pos, current_dir);
        puts("BEBAS@OS-IF2230>", 0x05);
        puts(current_dir, COLOR_WHITE_ON_BLACK);
        puts("$", 0x2);

        clearBuffer(inputBuffer, SHELL_MAX_LENGTH);
        inputLength = 0;

        syscall(SYSCALL_ENABLE_TYPING, 0, 0, 0);
        handleInput(inputBuffer, &inputLength);
        syscall(SYSCALL_DISABLE_TYPING, 0, 0, 0);

        parseCommand(inputBuffer, &current_cluster_pos);
        puts("\n", 0x0);
    }

    return 0;
}



int main(void)
{
    shell();

    return 0;
}