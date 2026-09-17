# Simorgh OS

Simorgh (سیمرغ) is a small x86 operating system I'm building from scratch to learn how computers actually work underneath the usual layers.

There is no Linux kernel, libc, or existing OS underneath it. The bootloader, kernel, interrupt handling, keyboard input, disk driver, filesystem, and shell are all part of this project.

The goal isn't to build a replacement for Linux or Windows. I mainly want to understand what happens between turning on a computer and getting an actual operating system running.

> «Built from scratch, one part at a time.»

## What is working?

Simorgh currently boots from a raw disk image in QEMU and enters 32-bit Protected Mode.

It has its own:

- x86 bootloader
- C kernel
- VGA text-mode driver
- IDT and interrupt handling
- PIC setup
- PS/2 keyboard driver
- ATA PIO disk driver
- filesystem
- command-line shell

The shell can currently work with files and directories:

`text
simorgh:/> mkdir docs
Directory created.

simorgh:/> cd docs
simorgh:/docs> write notes.txt hello simorgh
File written.

simorgh:/docs> ls
notes.txt

simorgh:/docs> cat notes.txt
hello simorgh
Some of the available commands are:
help
info
clear
echo
ls
cat
write
rm
mkdir
cd
pwd
fstest
reboot
The filesystem also keeps its data on the disk image, so files can survive a reboot.
How it works
The boot process currently looks roughly like this:
BIOS
 │
 ▼
Bootloader
 │
 ├── Load kernel
 ├── Set up GDT
 └── Enter 32-bit Protected Mode
        │
        ▼
     C Kernel
        │
        ├── VGA
        ├── IDT / PIC
        ├── Keyboard
        ├── ATA PIO
        ├── SimorghFS
        └── Shell
The project is deliberately kept small so I can follow the path from the CPU starting execution all the way to the shell.
Project structure
boot.asm          Bootloader
kernel_entry.asm  Kernel entry point

kernel.c          Main kernel code

screen.c/h        VGA text-mode driver

idt.c/h           Interrupt Descriptor Table
idt_load.asm      IDT loading
isr.asm           CPU exception handlers

keyboard.c/h      PS/2 keyboard driver
isr_kb.asm        Keyboard IRQ handler

disk.c/h          ATA PIO disk driver

fs.c/h            SimorghFS filesystem

shell.c/h         Command shell

linker.ld         Kernel memory layout
Building
Requirements
You'll need:
i686-elf-gcc
i686-elf-binutils
nasm
qemu-system-x86_64
1. Compile the C sources
i686-elf-gcc -ffreestanding -c screen.c -o screen.o
i686-elf-gcc -ffreestanding -c idt.c -o idt.o
i686-elf-gcc -ffreestanding -c keyboard.c -o keyboard.o
i686-elf-gcc -ffreestanding -c disk.c -o disk.o
i686-elf-gcc -ffreestanding -c fs.c -o fs.o
i686-elf-gcc -ffreestanding -c shell.c -o shell.o
i686-elf-gcc -ffreestanding -c kernel.c -o kernel.o
2. Assemble the kernel-side assembly
nasm -f elf32 kernel_entry.asm -o kernel_entry.o
nasm -f elf32 idt_load.asm -o idt_load.o
nasm -f elf32 isr.asm -o isr.o
nasm -f elf32 isr_kb.asm -o isr_kb.o
3. Link the kernel
i686-elf-ld -T linker.ld -o kernel.elf \
  kernel_entry.o kernel.o screen.o idt.o idt_load.o \
  isr.o keyboard.o isr_kb.o disk.o fs.o shell.o
4. Convert it to a binary
objcopy -O binary kernel.elf kernel.bin
5. Build the bootloader and disk image
nasm -f bin boot.asm -o boot.bin

cat boot.bin kernel.bin > os-image.bin
truncate -s 5M os-image.bin
6. Run it in QEMU
qemu-system-x86_64 -drive file=os-image.bin,format=raw
Current state
Simorgh is still a learning project and is nowhere near being a general-purpose operating system.
That's intentional.
Right now I'm focusing on getting the low-level pieces working correctly before trying to add more advanced features.
Some things I want to work on next:
Improve the filesystem
Add more shell commands
Add a simple memory allocator
Add user mode / Ring 3
Add basic multitasking
Improve hardware support
The roadmap will probably change as the project grows.
Why Simorgh?
The name comes from the Simorgh (سیمرغ), a legendary bird in Persian mythology.
