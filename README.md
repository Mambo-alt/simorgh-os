# Simorgh OS (سیمرغ)

A custom x86 operating system written from scratch — bootloader, kernel, drivers,
filesystem, and shell — with no existing kernel, framework, or libc underneath it.

Named after the Simorgh, the mythical bird of Persian legend: a creature that rises
from nothing and is rebuilt from its own ashes. This OS is built the same way —
one register, one interrupt, one sector at a time.

Inspired by Terry Davis and TempleOS: the belief that an operating system should be
something one person can fully understand, top to bottom, with no layer left as a
black box.

## What it does

Simorgh boots real hardware (or QEMU) from a raw disk image, switches itself into
32-bit Protected Mode by hand, brings up its own interrupt system, and drops you
into an interactive shell with a real, directory-aware filesystem — all without
touching a single line of code that isn't in this repository.

simorgh:/> info
Simorgh OS
Version 0.2
Architecture: x86
Kernel: Simorgh Kernel
Filesystem: SimorghFS
Shell: Simorgh Shell

simorgh:/> mkdir docs
Directory created.

simorgh:/> cd docs
simorgh:/docs> write notes.txt hello simorgh
File written.

simorgh:/docs> ls
notes.txt

simorgh:/docs> cd ..
simorgh:/> ls
docs/

simorgh:/> help
Commands:
help - show this list
info - show OS info
clear - clear the screen
echo <text> - print text
ls - list files/dirs
cat <file> - show file content
write <file> <t>- create/overwrite file with text
rm <name> - delete a file or empty dir
mkdir <name> - create a directory
cd <name> - change directory (.. or / supported)
pwd - print current directory
fstest - run filesystem self-test
reboot - restart the system


## Features

- **Bootloader** — 512-byte x86 real-mode bootloader, hand-written in NASM.
  Loads the kernel from disk, sets up a GDT, and switches the CPU into 32-bit
  Protected Mode.
- **Kernel** — written in freestanding C, linked as a flat binary with a custom
  linker script.
- **VGA text driver** — direct writes to `0xB8000`, hardware cursor control,
  automatic scrolling.
- **Interrupts** — custom IDT, PIC remapping, ISR stubs in assembly.
- **Keyboard driver** — IRQ1-driven, full Shift/Caps support, backspace handling.
- **Disk driver** — ATA PIO mode, direct port I/O, no BIOS calls once in
  Protected Mode.
- **SimorghFS** — a flat-array filesystem living on raw disk sectors, with a
  real directory tree on top of it: create, read, write, delete, list, `mkdir`,
  `cd`, `pwd`. Up to 12 entries total (files + directories combined), each
  tagged with a type and a parent, so `ls`/`cat`/`write`/`rm` are always scoped
  to the current directory.
- **Shell** — a command interpreter with clear, consistent error handling
  (`Unknown command`, `Error: file not found`, `Error: disk error`,
  `Error: invalid filename`, `Error: not a directory`, `Error: directory not
  empty`, ...), and a prompt that shows the current path
  (`simorgh:/docs>`).

## Project structure

boot.asm Bootloader (real mode -> protected mode)
kernel_entry.asm Kernel entry stub, jumps into C
kernel.c Kernel main, boot sequence

screen.c/h VGA text-mode driver
idt.c/h IDT setup, PIC remapping
idt_load.asm lidt wrapper
isr.asm CPU exception stub
keyboard.c/h PS/2 keyboard driver
isr_kb.asm IRQ1 stub

disk.c/h ATA PIO disk driver
fs.c/h SimorghFS filesystem (files + directories)
shell.c/h Command shell

linker.ld Kernel memory layout


## Building

### Requirements

- `i686-elf-gcc` cross-compiler (Binutils + GCC targeting `i686-elf`)
- `nasm`
- `qemu-system-x86_64` (for testing)

### Build steps
Compile the C sources

i686-elf-gcc -ffreestanding -c screen.c -o screen.o
i686-elf-gcc -ffreestanding -c idt.c -o idt.o
i686-elf-gcc -ffreestanding -c keyboard.c -o keyboard.o
i686-elf-gcc -ffreestanding -c disk.c -o disk.o
i686-elf-gcc -ffreestanding -c fs.c -o fs.o
i686-elf-gcc -ffreestanding -c shell.c -o shell.o
i686-elf-gcc -ffreestanding -c kernel.c -o kernel.o

Assemble the kernel-side asm (ELF objects, link with C)

nasm -f elf32 kernel_entry.asm -o kernel_entry.o
nasm -f elf32 idt_load.asm -o idt_load.o
nasm -f elf32 isr.asm -o isr.o
nasm -f elf32 isr_kb.asm -o isr_kb.o

Link the kernel

i686-elf-ld -T linker.ld -o kernel.elf
kernel_entry.o kernel.o screen.o idt.o idt_load.o isr.o
keyboard.o isr_kb.o disk.o fs.o shell.o

Flatten to raw binary

objcopy -O binary kernel.elf kernel.bin

Assemble the bootloader (flat binary, not ELF)

nasm -f bin boot.asm -o boot.bin

Build the final disk image

cat boot.bin kernel.bin > os-image.bin
truncate -s 5M os-image.bin


### Running

qemu-system-x86_64 -drive file=os-image.bin,format=raw


## Roadmap

- [x] Directory support in SimorghFS
- [ ] More shell commands (`mv`, `cp`)
- [ ] A simple memory allocator
- [ ] User-mode / Ring 3 support
- [ ] Basic multitasking

## License

MIT — do whatever you want with it.
