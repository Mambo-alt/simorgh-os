[bits 32]

[extern kernel_main]
[extern _bss_start]
[extern _bss_end]

global _start

section .text
_start:
    ; ---- Zero out .bss ----
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, edi          ; length = end - start
    jz .bss_done          ; skip if empty
    xor eax, eax
    cld                   ; forward direction for stosb
    rep stosb             ; write ecx zero-bytes at [edi]
.bss_done:

    ; ---- Jump into C kernel ----
    call kernel_main

    ; ---- If kernel_main returns (it shouldn't), hang ----
.hang:
    cli
    hlt
    jmp .hang
