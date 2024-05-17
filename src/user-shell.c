#include <stdint.h>
#include "header/filesystem/fat32.h"

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}


uint32_t strlen (char* text){
    int retval = 0;
    while (text[retval] != '\0'){
        retval++;
    }
    return retval;
}

void puts (char* text, uint32_t color){
    syscall(6, (uint32_t) text, strlen(text), color);

}

int main(void) {
    puts("Halo", 0x0C);
    puts("This", 0x0C);
    return 0;
}

