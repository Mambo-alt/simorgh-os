[org 0x7c00]
[bits 16]

start:
    mov [BOOT_DRIVE], dl
    xor ax, ax
    mov es, ax

    mov bp, 0x9000
    mov sp, bp

    mov bx, 0x1000
    mov dh, 48
    mov dl, [BOOT_DRIVE]
    call disk_load

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp CODE_SEG:init_pm

disk_load:
    push dx
    mov ah, 0x02
    mov al, dh
    mov ch, 0x00
    mov dh, 0x00
    mov cl, 0x02

    int 0x13
    jc disk_error

    pop dx
    cmp al, dh
    jne disk_error
    ret

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

DISK_ERROR_MSG: db 'Disk read error!', 0
BOOT_DRIVE: db 0

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
    mov ss, ax

    jmp 0x1000

times 510-($-$$) db 0
dw 0xaa55
