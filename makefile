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
	@qemu-system-i386 -s -S -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso
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

$(OUTPUT_FOLDER)/portio.o: $(SOURCE_FOLDER)/portio.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/intsetup.o: $(SOURCE_FOLDER)/intsetup.s
	$(ASM) $(AFLAGS) $< -o $@

$(OUTPUT_FOLDER)/context_switch.o: $(SOURCE_FOLDER)/context_switch.s
	$(ASM) $(AFLAGS) $< -o $@

$(OUTPUT_FOLDER)/keyboard.o: $(SOURCE_FOLDER)/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/string.o: $(SOURCE_FOLDER)/string.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/disk.o: $(SOURCE_FOLDER)/disk.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/fat32.o: $(SOURCE_FOLDER)/fat32.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/paging.o: $(SOURCE_FOLDER)/paging.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/process.o: $(SOURCE_FOLDER)/process.c
	$(CC) $(CFLAGS) $< -o $@

$(OUTPUT_FOLDER)/scheduler.o: $(SOURCE_FOLDER)/scheduler.c
	$(CC) $(CFLAGS) $< -o $@

keyboard: $(OUTPUT_FOLDER)/keyboard.o
idt: $(OUTPUT_FOLDER)/idt.o
framebuffer: $(OUTPUT_FOLDER)/framebuffer.o
interrupt: $(OUTPUT_FOLDER)/interrupt.o
intsetup: $(OUTPUT_FOLDER)/intsetup.o
string: $(OUTPUT_FOLDER)/string.o
disk: $(OUTPUT_FOLDER)/disk.o
fat32: $(OUTPUT_FOLDER)/fat32.o
paging: $(OUTPUT_FOLDER)/paging.o
portio: $(OUTPUT_FOLDER)/portio.o
process: $(OUTPUT_FOLDER)/process.o
gdt: $(OUTPUT_FOLDER)/gdt.o
scheduler: $(OUTPUT_FOLDER)/scheduler.o
context_switch: $(OUTPUT_FOLDER)/context_switch.o

DISK_NAME = storage
DISK_SIZE = 4M

disk:
	@qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M

$(OUTPUT_FOLDER)/$(DISK_NAME).bin:
	@qemu-img create -f raw $@ $(DISK_SIZE)

kernel: gdt idt string portio framebuffer interrupt intsetup keyboard fat32 paging context_switch process scheduler
	$(ASM) $(AFLAGS) src/kernel-entrypoint.s -o bin/kernel-entrypoint.o
	$(CC) $(CFLAGS) $(SOURCE_FOLDER)/kernel.c -o $(OUTPUT_FOLDER)/kernel.o
	$(LIN) $(LFLAGS) bin/*.o -o $(OUTPUT_FOLDER)/kernel
	@echo Linking object files and generate elf32...
	@rm -f *.o

iso: kernel $(OUTPUT_FOLDER)/$(DISK_NAME).bin
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

inserter:
	@$(CC) -Wno-builtin-declaration-mismatch -g -I$(SOURCE_FOLDER) \
		$(SOURCE_FOLDER)/string.c \
		$(SOURCE_FOLDER)/fat32.c \
		$(SOURCE_FOLDER)/external-inserter.c \
		-o $(OUTPUT_FOLDER)/inserter

user-shell:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-shell.c -o user-shell.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell
	@echo Linking object shell object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		crt0.o user-shell.o string.o -o $(OUTPUT_FOLDER)/shell_elf
	@echo Linking object shell object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/shell
	@rm -f *.o

insert-shell: inserter user-shell
	@echo Inserting shell into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter shell 2 $(DISK_NAME).bin


clock: inserter
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/clock.c -o clock.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/string.c -o string.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o clock.o string.o -o $(OUTPUT_FOLDER)/clock
	@echo Linking object clock object files and generate flat binary...
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=elf32-i386 \
		crt0.o clock.o string.o -o $(OUTPUT_FOLDER)/clock_elf
	@echo Linking object clock object files and generate ELF32 for debugging...
	@size --target=binary $(OUTPUT_FOLDER)/clock
	@rm -f *.o

insert-clock: inserter clock
	@echo Inserting clock into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter clock 2 $(DISK_NAME).bin