[bits 32]
global isr0
extern isr_handler_c

isr0:
    cli
    call isr_handler_c
    iret
