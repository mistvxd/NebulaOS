[BITS 32]

global syscall_stub
extern syscall_handler
extern kernel_esp
extern kernel_eip
extern current_process_active
extern kernel_stack_top

section .text

syscall_stub:
    pushad
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call syscall_handler
    add esp, 4

    jmp .return

    pop gs
    pop fs
    pop es
    pop ds
    popad
    iretd

.return:
    pop gs
    pop fs
    pop es
    pop ds
    cli
    popad
    iretd