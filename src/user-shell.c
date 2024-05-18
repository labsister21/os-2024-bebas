#include <stdint.h>
#include "header/stdlib/string.h"
#include "header/filesystem/fat32.h"

#define SHELL_MAX_LENGTH 256
#define SYSCALL_ENABLE_TYPING 7
#define SYSCALL_READ_CHAR 4
#define SYSCALL_WRITE_CHAR 5
#define SYSCALL_DISABLE_TYPING 8
#define COLOR_WHITE_ON_BLACK 0xF

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

void print_dir_tree(uint32_t cluster_number, char *text_pointer)
{
    char temp[SHELL_MAX_LENGTH / 2];
    // char current_folder[9] = "\0\0\0\0\0\0\0\0";
    uint32_t current_cluster = cluster_number;
    uint16_t prog = 0;
    struct FAT32DirectoryTable buff;
    // asumsi selalu exist
    //  memcpy(current_folder,current_folder_name,strlen(current_folder_name));
    // move current_folder_name first

    do
    {
        // prog++;
        uint8_t retcode;
        // get parent of current folder
        struct FAT32DriverRequest req = {
            .buf = buff.table,
            .name = ".\0\0\0\0\0\0\0",
            .ext = "\0\0\0",
            .parent_cluster_number = current_cluster,
            .buffer_size = 0x0,
        };
        syscall(1, (uint32_t)&req, (uint32_t)&retcode, 0);

        // add name to temp
        invert_memcpy(temp + sizeof(char) * (prog), buff.table[0].name, strlen(buff.table[0].name));
        prog += strlen(buff.table[0].name);

        current_cluster = buff.table[1].cluster_low | buff.table[1].cluster_high << 16;

        // ALT ROOT CHECK
        if (!((buff.table[0].cluster_low == buff.table[1].cluster_low) && (buff.table[0].cluster_high == buff.table[1].cluster_high)))
        {
            memset(temp + sizeof(char) * (prog), '\\', 1);
            prog++;
            // break;
        }
        else
        {
            break;
        }
    } while (true);

    invert_memcpy(text_pointer, temp, prog);
}

void puts(char *text, uint32_t color)
{
    syscall(6, (uint32_t)text, strlen(text), color);
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

bool find_in_directory(uint16_t parent_cluster, char *name, char *found_path)
{
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
        if (memcmp(dirbuffer.table[i].name, name, strlen(name)) == 0)
        {
            // Match found, construct the path
            print_dir_tree(dirbuffer.table[i].cluster_low | (dirbuffer.table[i].cluster_high << 16), found_path);
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
    if (memcmp(buffer, "cd", 2) == 0) ////CD
    {
        // uint8_t len = strlen(buffer + 3);
        char pos = *parent_cluster + '0';
        syscall(5, (uint32_t)&pos, 0xF, 0);
        puts("\n", 0xF);
        puts(buffer + 3, 0xF);
        puts("\n", 0xF);
        uint32_t retcode = 0;
        uint8_t progress = 3;
        uint16_t current_cluster = *parent_cluster;
        struct FAT32DirectoryTable dirbuffer;
        char splitter = '/';
        while (retcode == 0 && strlen(buffer + progress) > 0)
        { // while no errors and remaining text length is not 0
            uint8_t current_len = 0;
            // char fname[9] = "\0\0\0\0\0\0\0\0";
            while (buffer[progress + current_len] != splitter && current_len < 8 && strlen(buffer + progress + current_len) > 0)
            {
                current_len++;
            }
            if (current_len == 8 && buffer[progress + current_len] != splitter)
            {
                // Too long!
                puts("File name too long!", 0x4);
                break;
            }
            else
            {
                syscall(6, (uint32_t)buffer + progress, (uint32_t)current_len, 0xF);
                puts("\n", 0xF);
                retcode = read_directory_interface(&dirbuffer, buffer + progress, current_len, current_cluster);
                current_cluster = dirbuffer.table[0].cluster_low | dirbuffer.table[0].cluster_high << 16;
            }

            progress += current_len + 1; //+1 to offset for the splitter
        }

        if (retcode != 0)
        {
            puts("File not found!", 0x4);
        }
        else
        {
            *parent_cluster = current_cluster;
        }
        // if (memcmp("..", buffer + 3, 2) == 0)
        // {
        //     if (id == 0)
        //     {
        //         puts("Already in root", 0x4);
        //         return;
        //     }
        //     id--;
        //     puts("Change directory success", 0x2);
        //     return;
        // }
        // else
        // {
        //     struct FAT32DriverRequest request = {
        //         .buf = &clusbuff,
        //         .parent_cluster_number = listCluster[id],
        //         .buffer_size = 0
        //     };
        //     memcpy(request.name, listName[id], 8);
        //     int32_t retcode = listCluster[id];
        //     struct FAT32DirectoryTable table = {};
        //     request.buf = &table;
        //     syscall(1, (uint32_t) &request, (uint32_t) &retcode, 0);
        //     for (int i = 0 ; i < 64 ; i++)
        //     {
        //         if (memcmp(table.table[i].name, (char *) buffer + 3, 8) == 0)
        //         {
        //             id++;
        //             listCluster[id] = table.table[i].cluster_low | (table.table[i].cluster_high << 16);
        //             // memcpy(listName[id],table.table[i].name,8);
        //             listName[id] = table.table[i].name;
        //             // memcpy(listName[id],cek2, 8);
        //             puts("Change directory success", 0x2);
        //             return;
        //         }
        //     }
        //     puts("No such directory", 0x4);
        // }
    }
    else if (memcmp((char *)buffer, "ls", 2) == 0) // ls command
    {
        struct FAT32DirectoryTable dtable;
        struct FAT32DriverRequest request = {
            .buf = dtable.table,
            .buffer_size = 0,
            .parent_cluster_number = *parent_cluster,
            .name = ".\0\0\0\0\0\0"};

        uint8_t retval = 0;

        syscall(1, (uint32_t)&request, (uint32_t)&retval, 0);
        // syscall(5,(uint32_t)'0'+retval,0xF,0);

        for (int i = 0; i < 64; i++)
        {
            if (dtable.table[i].attribute != 0x00000000)
            {
                int len = strlen(dtable.table[i].name);
                char *ext = dtable.table[i].ext;

                // Check if it's a folder (directory)
                if ((dtable.table[i].attribute & 0x10) != 0)
                { // If the directory flag is set
                    syscall(6, (uint32_t)dtable.table[i].name, (uint32_t)len, 0xF);
                }
                else
                { // It's a file
                    // Check if the character to the right is '\0'
                    if (ext[0] == '\0')
                    {
                        // If the character to the right is '\0', add a dot
                        syscall(6, (uint32_t)dtable.table[i].name, (uint32_t)len, 0xF);
                        syscall(6, (uint32_t) ".", (uint32_t)1, 0xF);
                    }
                    else
                    {
                        // Otherwise, don't add a dot
                        syscall(6, (uint32_t)dtable.table[i].name, (uint32_t)len, 0xF);
                    }
                    // Print the extension
                    syscall(6, (uint32_t)ext, (uint32_t)3, 0xF);
                }
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
    else if (memcmp((char *)buffer, "cat", 3) == 0) ////cat
    {
        int prev_press = 0;
        bool double_guard = false;
        bool exit = false;

        syscall(7, 0, 0, 0);
        while (true)
        {

            // release double guard
            if (prev_press == 0 && double_guard)
            {
                double_guard = false;
            }

            // else if (kbd event here)
            else
            {
                prev_press = 0;
            }

            if (exit)
            {
                break;
            }

            // continue typing if nothing happened
            char input;
            syscall(4, (uint32_t)&input, 0, 0);
        }

        syscall(8, 0, 0, 0);
    }
    // else if (memcmp((char * ) buffer, "cp", 2) == 0) ////cp
    // {
    //     struct FAT32DriverRequest request = {
    //         .buf                   = &clusbuff,
    //         .parent_cluster_number = ROOT_CLUSTER_NUMBER,
    //         .buffer_size           = 0,
    //     };
    //     request.buffer_size = 5*CLUSTER_SIZE;
    //     int nameLen = 0;
    //     char* itr = (char * ) buffer + 3;
    //     for(int i = 0; i < strlen(itr) ; i++){
    //         if(itr[i] == '.'){
    //             request.ext[0] = itr[i+1];
    //             request.ext[1] = itr[i+2];
    //             request.ext[2] = itr[i+3];
    //             break;
    //         }else{
    //             nameLen++;
    //         }
    //     }

    //     memcpy(request.name, (void *) (buffer + 3), nameLen);
    //     int32_t retcode;

    //     struct FAT32DriverRequest request2 = {
    //         .buf                   = &clusbuff,
    //         .parent_cluster_number = ROOT_CLUSTER_NUMBER,
    //         .buffer_size           = 0,
    //     };

    //     memcpy(request2.name, listName[id], 8);
    //     syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);

    //     /* Read the Destination Folder to paste */
    //     if (retcode == 0)
    //         puts("Read success", 0x2);
    //     else if (retcode == 1){
    //         puts("Not a file", 0x4);
    //         return;
    //     }
    //     else if (retcode == 2){
    //         puts("Not enough buffer", 0x4);
    //         return;
    //     }
    //     else if (retcode == 3){
    //         puts("File Not found", 0x4);
    //         return;
    //     }
    //     else{
    //         puts("Unknown error", 0x4);
    //         return;
    //     }

    //     int32_t retcode2;
    //     struct FAT32DirectoryTable table = {};
    //     request2.buf = &table;
    //     syscall(1, (uint32_t) &request2, (uint32_t) &retcode, 0);

    //     for(int i = 0; i < 64; i++){
    //         if(memcmp(table.table[i].name, (void * )(buffer + 3 + nameLen + 5), strlen((char *) buffer + 3 + nameLen + 5)) == 0){
    //             request.parent_cluster_number = (table.table[i].cluster_high << 16) | table.table[i].cluster_low;
    //             syscall(2, (uint32_t) &request, (uint32_t) &retcode2, 0);
    //             break;
    //         }
    //     }

    //     if (retcode2 == 0)
    //         puts("Write success", 0x2);
    //     else if (retcode2 == 1)
    //         puts("File/Folder already exist", 0x4);
    //     else if (retcode2 == 2)
    //         puts("Invalid parent cluster", 0x4);
    //     else
    //         puts("Target folder not found", 0x4);
    // }
    else if (memcmp((char *)buffer, "rm", 2) == 0) ////rm
    {
        char *filename = buffer + 3; // Skip the "rm " part
        while (*filename == ' ')
        {
            filename++;
        }

        // struct FAT32DirectoryEntry entry;
        char found_path[SHELL_MAX_LENGTH] = "";
        if (find_in_directory(*parent_cluster, filename, found_path))
        {
            struct FAT32DriverRequest req = {
                .buf = NULL,
                .name = "\0\0\0\0\0\0\0",
                .parent_cluster_number = *parent_cluster,
                .buffer_size = 0,
            };
            memcpy(req.name, filename, strlen(filename));
            uint8_t retval;
            syscall(3, (uint32_t)&req, (uint32_t)&retval, 0);
            if (retval == 0)
            {
                puts("File or directory removed successfully.\n", 0x0F);
            }
            else
            {
                puts("Failed to remove file or directory.\n", 0x0F);
            }
        }
        else
        {
            puts("File or directory not found.\n", 0x0F);
        }
    }
    // else if (memcmp((char *) buffer, "mv", 2) == 0) ////mv
    // {

    //     struct FAT32DriverRequest request = {
    //         .buf                   = &clusbuff,
    //         .parent_cluster_number = ROOT_CLUSTER_NUMBER,
    //         .buffer_size           = 0,
    //     };
    //     request.buffer_size = 5*CLUSTER_SIZE;
    //     int nameLen = 0;
    //     char* itr = (char * ) buffer + 3;
    //     for(int i = 0; i < strlen(itr) ; i++){
    //         if(itr[i] == '.'){
    //             request.ext[0] = itr[i+1];
    //             request.ext[1] = itr[i+2];
    //             request.ext[2] = itr[i+3];
    //             break;
    //         }else{
    //             nameLen++;
    //         }
    //     }

    //     memcpy(request.name, (void *) (buffer + 3), nameLen);
    //     int32_t retcode;

    //     struct FAT32DriverRequest request2 = {
    //         .buf                   = &clusbuff,
    //         .parent_cluster_number = ROOT_CLUSTER_NUMBER,
    //         .buffer_size           = 0,
    //     };

    //     memcpy(request2.name, listName[id], 8);
    //     syscall(0, (uint32_t) &request, (uint32_t) &retcode, 0);

    //     /* Read the Destination Folder to paste */
    //     if (retcode == 0)
    //         puts("Read success", 0x2);
    //     else if (retcode == 1){
    //         puts("Not a file", 0x4);
    //         return;
    //     }
    //     else if (retcode == 2){
    //         puts("Not enough buffer", 0x4);
    //         return;
    //     }
    //     else if (retcode == 3){
    //         puts("File Not found", 0x4);
    //         return;
    //     }
    //     else{
    //         puts("Unknown error", 0x4);
    //         return;
    //     }

    //     int32_t retcode2;
    //     struct FAT32DirectoryTable table = {};

    //             request2.buf = &table;
    //     syscall(1, (uint32_t) &request2, (uint32_t) &retcode, 0);
    //     for(int i = 0; i < 64; i++){
    //         if(memcmp(table.table[i].name, (void * )(buffer + 3 + nameLen + 5), strlen((char *) buffer + 3 + nameLen + 5)) == 0){
    //             request.parent_cluster_number = (table.table[i].cluster_high << 16) | table.table[i].cluster_low;
    //             syscall(2, (uint32_t) &request, (uint32_t) &retcode2, 0);
    //             break;
    //         }
    //     }

    //     if (retcode2 == 0)
    //         puts("Write success", 0x2);
    //     else if (retcode2 == 1)
    //         puts("File/Folder already exist", 0x4);
    //     else if (retcode2 == 2)
    //         puts("Invalid parent cluster", 0x4);
    //     else
    //         puts("Target folder not found", 0x4);

    //     struct FAT32DriverRequest request3 = {
    //         .buf                   = &clusbuff,
    //         .parent_cluster_number = listCluster[id],
    //         .buffer_size           = 0,
    //     };
    //     request3.buffer_size = 5*CLUSTER_SIZE;
    //     int nameLen1 = 0;
    //     char* itr1 = (char * ) buffer + 3;
    //     for(int i = 0; i < strlen(itr) ; i++){
    //         if(itr[i] == '.'){
    //             request3.ext[0] = itr1[i+1];
    //             request3.ext[1] = itr1[i+2];
    //             request3.ext[2] = itr1[i+3];
    //             break;
    //         }else{
    //             nameLen1++;
    //         }
    //     }
    //     memcpy(request3.name, (void *) (buffer + 3), 8);

    //     int32_t retcode3;
    //     syscall(3, (uint32_t) &request3, (uint32_t) &retcode3, 0);
    //     if (retcode3 == 0)
    //         puts("Move Success", 0x2);
    //     else if (retcode3 == 1)
    //         puts("File/Folder Not Found", 0x4);
    //     else if (retcode3 == 2)
    //         puts("Folder is empty", 0x4);
    //     else
    //         puts("Unknown Error", 0x4);
    // }
    else if (memcmp((char *)buffer, "find", 4) == 0) ////find
    {
        char name[9];
        memcpy(name, buffer + 5, 8);
        name[8] = '\0'; // ensure null-terminated

        char found_path[SHELL_MAX_LENGTH] = "";
        if (find_in_directory(*parent_cluster, name, found_path))
        {
            puts("Found: ", 0x2);
            puts(found_path, 0x2);
        }
        else
        {
            puts("Not found", 0x4);
        }
    }
    // else if (memcmp((char *) buffer, "cls", 3) == 0)
    // {
    //     puts("cls", 0xF);
    // }
    // else if (memcmp((char *) buffer, "touch", 5) == 0)
    // {
    //     struct FAT32DriverRequest request = {
    //         .buffer                   = &clusbuff,
    //         .parent_cluster_number = listCluster[id],
    //         .buffer_size           = 0,
    //     };
    //     request.buffer_size = 5*CLUSTER_SIZE;

    //     /* get the name and ext of the file */
    //     int nameLen1 = 0;
    //     char* itr = (char * ) buffer + 6;
    //     for(int i = 0; i < strlen(itr) ; i++){
    //         if(itr[i] == '.'){
    //             request.ext[0] = itr[i+1];
    //             request.ext[1] = itr[i+2];
    //             request.ext[2] = itr[i+3];
    //             break;
    //         }else{
    //             nameLen1++;
    //         }
    //     }

    //     memcpy(request.name, (void * ) buffer + 6, nameLen1);
    //     uint32_t retcode;
    //     struct ClusterBuffer cbuf[5] = {};

    //     /* Dapetin isi filenya */
    //     char* isi = "";
    //     isi = (char *)buffer + 6 + nameLen1 + 2 + 3;

    //     for (uint32_t i = 0; i < 5; i++)
    //         for (uint32_t j = 0; j < CLUSTER_SIZE; j++)
    //             cbuf[i].buffer[j] = isi[j];

    //     /* Write to the file */
    //     request.buffer = cbuf;
    //     syscall(2, (uint32_t) &request, (uint32_t) &retcode, 0);
    //     if(retcode == 0){
    //         puts("Write success", 0x2);
    //     }else{
    //         puts("Write unsuccessful", 0x4);
    //     }

    // }
    else
    {
        puts("Command not found", 0x4);
    }
}

void clearBuffer(char* buffer, int length) {
    for (int i = 0; i < length; i++) {
        buffer[i] = '\0';
    }
}

void handleInput(char* inputBuffer, int* inputLength) {
    char charBuffer;
    while (true) {
        syscall(SYSCALL_READ_CHAR, (uint32_t)&charBuffer, 0, 0);
        if (charBuffer) {
            switch (charBuffer) {
            case '\n':
                syscall(SYSCALL_WRITE_CHAR, (uint32_t)&charBuffer, COLOR_WHITE_ON_BLACK, 0);
                return;
            case '\b':
                if (*inputLength > 0) {
                    inputBuffer[*inputLength] = '\0';
                    (*inputLength)--;
                    syscall(SYSCALL_WRITE_CHAR, (uint32_t)&charBuffer, COLOR_WHITE_ON_BLACK, 0);
                }
                break;
            default:
                if (*inputLength < SHELL_MAX_LENGTH - 1) {
                    syscall(SYSCALL_WRITE_CHAR, (uint32_t)&charBuffer, COLOR_WHITE_ON_BLACK, 0);
                    inputBuffer[*inputLength] = charBuffer;
                    (*inputLength)++;
                }
                break;
            }
        }
    }
}

int shell(void) {
    char inputBuffer[SHELL_MAX_LENGTH];
    int inputLength = 0;
    char current_dir[SHELL_MAX_LENGTH / 2];
    uint16_t current_cluster_pos = 2;

    while (true) {
        clearBuffer(current_dir, SHELL_MAX_LENGTH / 2);
        print_dir_tree(current_cluster_pos, current_dir);
        puts(current_dir, COLOR_WHITE_ON_BLACK);
        puts("BEBAS@OS-IF2230>", 0x05);

        clearBuffer(inputBuffer, SHELL_MAX_LENGTH);
        inputLength = 0;

        syscall(SYSCALL_ENABLE_TYPING, 0, 0, 0);
        handleInput(inputBuffer, &inputLength);
        syscall(SYSCALL_DISABLE_TYPING, 0, 0, 0);

        parseCommand(inputBuffer, &current_cluster_pos);
        puts("\n", 0x00);
    }

    return 0;
}

int main(void)
{
    shell();

    return 0;
}