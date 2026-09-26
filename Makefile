CROSS ?= i686-elf

CC      := $(CROSS)-gcc
LD      := $(CROSS)-ld
OBJCOPY := $(CROSS)-objcopy
NASM    := nasm

CFLAGS  := -m32 -ffreestanding -fno-pie -fno-stack-protector -Wall -Wextra
LDFLAGS := -T linker.ld

KERNEL_SECTORS ?= 64
SECTOR_SIZE    := 512
KERNEL_MAX     = $(shell echo $$(( $(KERNEL_SECTORS) * $(SECTOR_SIZE) )))
IMAGE_SIZE     := 5M

C_SOURCES := \
	screen.c \
	idt.c \
	keyboard.c \
	disk.c \
	fs.c \
	shell.c \
	kernel.c

C_OBJECTS := $(C_SOURCES:.c=.o)

ASM_SOURCES := \
	kernel_entry.asm \
	idt_load.asm \
	isr.asm \
	isr_kb.asm

ASM_OBJECTS := $(ASM_SOURCES:.asm=.o)

KERNEL_OBJECTS := \
	kernel_entry.o \
	kernel.o \
	screen.o \
	idt.o \
	idt_load.o \
	isr.o \
	keyboard.o \
	isr_kb.o \
	disk.o \
	fs.o \
	shell.o

.PHONY: all run clean check

all: os-image.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(NASM) -f elf32 $< -o $@

kernel.elf: $(KERNEL_OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

boot.bin: boot.asm
	$(NASM) -f bin $< -o $@

check: boot.bin kernel.bin
	@set -eu; \
	boot_size=$$(wc -c < boot.bin); \
	kernel_size=$$(wc -c < kernel.bin); \
	kernel_sectors=$$((($$kernel_size + 511) / 512)); \
	kernel_max=$$(( $(KERNEL_SECTORS) * 512 )); \
	signature=$$(od -An -t x1 -j 510 -N 2 boot.bin | tr -d '[:space:]'); \
	echo "check: boot.bin = $$boot_size bytes"; \
	echo "check: kernel.bin = $$kernel_size bytes"; \
	echo "check: kernel sectors = $$kernel_sectors"; \
	echo "check: loader limit = $(KERNEL_SECTORS) sectors ($$kernel_max bytes)"; \
	if [ "$$boot_size" -ne 512 ]; then \
	echo "error: boot.bin must be exactly 512 bytes"; \
	exit 1; \
	fi; \
	if [ "$$signature" != "55aa" ]; then \
	echo "error: boot.bin is missing signature 0xAA55"; \
	exit 1; \
	fi; \
	if [ "$$kernel_size" -gt "$$kernel_max" ]; then \
	echo "error: kernel.bin is too large for the bootloader"; \
	exit 1; \
	fi; \
	echo "ok: image layout is valid"

os-image.bin: boot.bin kernel.bin check
	cat boot.bin kernel.bin > $@
	truncate -s $(IMAGE_SIZE) $@

run: os-image.bin
	qemu-system-x86_64 -drive file=os-image.bin,format=raw

clean:
	rm -f *.o *.bin *.elf os-image.bin
