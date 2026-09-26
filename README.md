# Simorgh OS

Simorgh OS is a small, from-scratch x86 operating system by Mambo. It boots from a raw disk image, enters 32-bit protected mode, initializes a minimal kernel, and provides an interactive shell with a simple persistent filesystem.

Simorgh is an experimental project, not a replacement for Linux or Windows. Its goal is to make the path from boot sector to usable system understandable, testable, and gradually more capable.

## Why Simorgh?

I am building Simorgh to understand an operating system by building its layers myself: boot, interrupts, hardware I/O, a shell, and storage. Terry Davis's independent approach to creating a personal operating system is an inspiration. Simorgh is its own project, with its own design and code.

The aim is not to add features as quickly as possible. Each layer should remain understandable, and new features should be built on a stable foundation.

## How it works

1. `boot.asm` runs as a BIOS boot sector, reads the kernel from the disk image, sets up a GDT, and switches the CPU to 32-bit protected mode.
2. `kernel_entry.asm` clears the kernel's `.bss` section and calls `kernel_main()`.
3. The kernel initializes VGA text output, the keyboard, the interrupt table, and the shell.
4. Keyboard interrupts add key events to a queue. The main loop consumes those events and passes them to the shell.
5. The shell handles commands and line editing. File commands use SimorghFS, which stores a small file table and file data at fixed disk locations.

This is a deliberately small design. It is not yet a general-purpose operating system.

## Current capabilities

- BIOS boot into 32-bit protected mode.
- VGA text output and scrollback.
- Keyboard input, line editing, command history, and filename completion.
- An interactive shell with commands such as `help`, `info`, `ls`, `cat`, `write`, `mkdir`, `cd`, `pwd`, `mv`, `cp`, and `rm`.
- A small persistent filesystem with files and directories.
- Build-time checks for boot-sector size, boot signature, and kernel size.
- A guard against moving a directory into itself or its descendants.

## Build and run

You need an `i686-elf` cross-toolchain, NASM, GNU Make, `objcopy`, and QEMU available in your environment.

```sh
make clean
make
make run
```

`make` produces `os-image.bin`. `make run` starts the image in QEMU.

The bootloader currently reads a fixed maximum of 64 kernel sectors (32 KiB). The build check must reject a kernel that exceeds that limit.

## Limitations

- This is an early experimental system. Test it in QEMU; do not assume it is ready for everyday use or real hardware.
- The kernel-loading limit is currently fixed rather than determined dynamically.
- SimorghFS has a small, fixed number of entries and a fixed per-file size limit.
- The filesystem is not a mature, crash-safe filesystem.
- There is no multitasking, user-mode process isolation, networking, or graphical desktop yet.

## Project status

Development currently focuses on reliability: keeping builds reproducible, preventing filesystem corruption, and making failures easier to diagnose. New features will be added when the layers they depend on are stable enough.

## Author

Created by Mambo.
