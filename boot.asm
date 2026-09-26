[org 0x7c00]
[bits 16]

; ---- Constants ----
KERNEL_ADDR_OFF  equ 0x0000
KERNEL_ADDR_SEG  equ 0x1000      ; physical address = 0x10000
KERNEL_LBA       equ 1           ; sector 2 = LBA 1 (bootloader = LBA 0)
KERNEL_SECTORS   equ 64         ; 64 * 512 = 32KB
KERNEL_ENTRY     equ 0x10000     ; where to jump after entering PM

start:
    cli
    mov [BOOT_DRIVE], dl

    ; ---- segments + temp stack (below bootloader) ----
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; ---- require BIOS LBA extensions ----
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error
    cmp bx, 0xAA55
    jne disk_error

    ; ---- LBA read: load kernel to 0x10000 ----
    mov si, dap
    mov ah, 0x42
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error

    ; ---- enter protected mode ----
    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:init_pm

disk_error:
    mov si, DISK_ERROR_MSG
    call print_string_16
    jmp $

print_string_16:
    mov ah, 0x0e
.loop:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .loop
.done:
    ret

; ---- Disk Address Packet (16 bytes) ----
dap:
    db 0x10                      ; size of DAP
    db 0                         ; reserved
    dw KERNEL_SECTORS            ; number of sectors to read
    dw KERNEL_ADDR_OFF           ; destination offset
    dw KERNEL_ADDR_SEG           ; destination segment
    dq KERNEL_LBA                ; starting LBA

DISK_ERROR_MSG: db 'Disk read error!', 0
BOOT_DRIVE: db 0

; ---- Global Descriptor Table ----
gdt_start:
gdt_null:
    dd 0x0
    dd 0x0
gdt_code:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10011010b
    db 11001111b
    db 0x0
gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000             ; 576KB stack, way above kernel

    jmp KERNEL_ENTRY             ; jump to kernel entry

times 510-($-$$) db 0
dw 0xaa55
