[bits 32]
section .data
msg db "editing: "
msg_len equ $ - msg
msg2 db "saved!"
msg2_len equ $ - msg2
nl db 10
nl_len equ 1

section .bss
buffer resb 64
filename resd 1

section .text
global _start

_start:
    mov eax, [esp + 4]
    cmp eax, 2
    jl .exit

    mov eax, [esp + 8]
    mov eax, [eax + 4]
    mov [filename], eax

    mov eax, 2
    mov ebx, msg
    mov ecx, 0x0F
    mov edx, msg_len
    int 0x80

    mov edi, buffer
    xor esi, esi

.read_loop:
    cmp esi, 63
    jge .done

    mov eax, 8
    mov ebx, edi
    int 0x80

    mov al, [edi]
    cmp al, 10
    je .done

    mov eax, 2
    mov ebx, edi
    mov ecx, 0x0F
    mov edx, 1
    int 0x80

    inc edi
    inc esi
    jmp .read_loop

.done:
    mov byte [edi], 0

    mov eax, 2
    mov ebx, nl
    mov ecx, 0x0F
    mov edx, nl_len
    int 0x80

    mov eax, 5
    mov ebx, [filename]
    mov ecx, 1
    int 0x80

    mov ebx, eax
    mov eax, 7
    mov ecx, buffer
    mov edx, esi
    int 0x80

.exit:
    mov eax, 1
    int 0x80