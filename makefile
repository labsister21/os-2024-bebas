# Compiler & linker
ASM           = nasm
LIN           = ld
CC            = gcc

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS2024

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386

run: all
	@qemu-system-i386 -s -S -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso
all: build
build: iso
clean:
	rm -rf $(OUTPUT_FOLDER)/*.o $(OUTPUT_FOLDER)/*.iso $(OUTPUT_FOLDER)/kernel

$(OUTPUT_FOLDER)/gdt.o: $(SOURCE_FOLDER)/gdt.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/kernel.o: $(SOURCE_FOLDER)/kernel.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/framebuffer.o: $(SOURCE_FOLDER)/framebuffer.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/interrupt.o: $(SOURCE_FOLDER)/interrupt.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/idt.o: $(SOURCE_FOLDER)/idt.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/intsetup.o: $(SOURCE_FOLDER)/intsetup.s
	$(ASM) $(AFLAGS) $< -o $@

$(OUTPUT_FOLDER)/keyboard.o: $(SOURCE_FOLDER)/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/string.o: $(SOURCE_FOLDER)/string.c
	$(CC) $(CFLAGS) $< -o $@

keyboard: $(OUTPUT_FOLDER)/keyboard.o
gdt: $(OUTPUT_FOLDER)/gdt.o
idt: $(OUTPUT_FOLDER)/idt.o
framebuffer: $(OUTPUT_FOLDER)/framebuffer.o
interrupt: $(OUTPUT_FOLDER)/interrupt.o
intsetup: $(OUTPUT_FOLDER)/intsetup.o
string: $(OUTPUT_FOLDER)/string.o

kernel: gdt idt string framebuffer interrupt intsetup keyboard
	$(ASM) $(AFLAGS) src/kernel-entrypoint.s -o bin/kernel-entrypoint.o
	$(CC) $(CFLAGS) $(SOURCE_FOLDER)/kernel.c -o $(OUTPUT_FOLDER)/kernel.o
	$(LIN) $(LFLAGS) bin/*.o -o $(OUTPUT_FOLDER)/kernel
	@echo Linking object files and generate elf32...
	@rm -f *.o

iso: kernel
	mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	cp $(OUTPUT_FOLDER)/kernel     $(OUTPUT_FOLDER)/iso/boot/
	cp other/grub1                 $(OUTPUT_FOLDER)/iso/boot/grub/
	cp $(SOURCE_FOLDER)/menu.lst   $(OUTPUT_FOLDER)/iso/boot/grub/
	genisoimage -R                   \
	-b boot/grub/grub1         \
	-no-emul-boot              \
	-boot-load-size 4          \
	-A os                      \
	-input-charset utf8        \
	-quiet                     \
	-boot-info-table           \
	-o bin/OS2024.iso              \
	bin/iso
	rm -r $(OUTPUT_FOLDER)/iso