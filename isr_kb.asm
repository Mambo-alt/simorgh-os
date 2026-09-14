[bits 32]
global isr33
extern keyboard_handler

isr33:
    pusha
    call keyboard_handler
    popa
    iret
