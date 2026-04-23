[BITS 32]

%define SYS_EXIT              1
%define SYS_WRITE             2
%define SYS_READ              8
%define SYS_TERMINAL_CLEAR    11
%define SYS_SPAWN_TASK        13
%define SYS_SCHEDULE          14

section .bss
input resb 128

section .data
prompt db "> "
prompt_len equ $ - prompt

section .text
global _start

_start:
.loop:
    mov eax, SYS_WRITE
    mov ebx, prompt
    mov ecx, 0x0F
    mov edx, prompt_len
    int 0x80

    mov edi, input

.read_loop:
    mov eax, SYS_READ
    mov ebx, edi
    mov ecx, 1
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, edi
    mov ecx, 0x0F
    mov edx, 1
    int 0x80

    cmp byte [edi], 13
    je .done

    cmp byte [edi], 10
    je .done

    inc edi
    jmp .read_loop

.done:
    mov byte [edi], 0

    mov eax, SYS_SPAWN_TASK
    mov ebx, input
    mov ecx, 0
    int 0x80

    jmp .loop