# BEBAS
## Anggota
| NIM | Nama |
| :---: | :---: |
| 13522151 | Samy Muhammad Haikal |
| 13522152 | Muhammad Roihan  |
| 13522161 | Mohammad Akmal Ramadan |
| 13522162 | Pradipta Rafa Mahesa |
## Deskripsi Singkat
Projek ini dibuat untuk memenuhi tugas IF2230. Dalam tugas ini, akan dibuat sebuah program Sistem Operasi. Sistem Operasi yang akan dibuat akan berjalan pada arsitektur x86 32-bit dan akan dijalankan menggunakan emulator QEMU.
## Milestone 1
Pada Milestone 1 kami mengimplementasikan fitur berikut :
- Driver Text Framebuffer
- Interrupt
- Keyboard Driver
- File System
## Milestone 2
Pada Milestone 2 kami mengimplementasikan fitur berikut :
- Paging
- User Mode
- Shell
## Milestone 3
Pada Milestone 3 kami mengimplementasikan fitur berikut :
- Process
- Scheduler
- Multitasking
## How To Use
1. Clone this repository
```
git clone https://github.com/labsister21/os-2024-bebas.git
```
2. Jalankan
```
make
```
## Struktur File
``` bash
.
├── README.md
├── bin
│   ├── OS2024.iso
│   ├── context_switch.o
│   ├── disk.o
│   ├── fat32.o
│   ├── framebuffer.o
│   ├── gdt.o
│   ├── idt.o
│   ├── inserter
│   ├── interrupt.o
│   ├── intsetup.o
│   ├── iso
│   │   └── boot
│   │       ├── grub
│   │       │   ├── grub1
│   │       │   └── menu.lst
│   │       └── kernel
│   ├── kernel
│   ├── kernel-entrypoint.o
│   ├── kernel.o
│   ├── keyboard.o
│   ├── paging.o
│   ├── portio.o
│   ├── process.o
│   ├── scheduler.o
│   ├── shell
│   ├── shell_elf
│   ├── storage.bin
│   └── string.o
├── makefile
├── other
│   └── grub1
└── src
    ├── context_switch.s
    ├── crt0.s
    ├── disk.c
    ├── external-inserter.c
    ├── fat32.c
    ├── framebuffer.c
    ├── gdt.c
    ├── header
    │   ├── cpu
    │   │   ├── gdt.h
    │   │   ├── idt.h
    │   │   ├── interrupt.h
    │   │   └── portio.h
    │   ├── driver
    │   │   ├── framebuffer.h
    │   │   └── keyboard.h
    │   ├── filesystem
    │   │   ├── disk.h
    │   │   └── fat32.h
    │   ├── kernel-entrypoint.h
    │   ├── memory
    │   │   └── paging.h
    │   ├── process
    │   │   ├── process.h
    │   │   └── scheduler.h
    │   └── stdlib
    │       └── string.h
    ├── idt.c
    ├── interrupt.c
    ├── intsetup.s
    ├── kernel-entrypoint.s
    ├── kernel.c
    ├── keyboard.c
    ├── linker.ld
    ├── menu.lst
    ├── paging.c
    ├── portio.c
    ├── process.c
    ├── scheduler.c
    ├── string.c
    ├── user-linker.ld
    └── user-shell.c
``` 
