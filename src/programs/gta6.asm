[BITS 32]

%define SYS_EXIT     1
%define SYS_FS_OPEN  5
%define SYS_FS_WRITE 7

section .data
prefix   db "file", 0
content  db "boom", 0

section .bss
filename    resb 32
counter_str resb 16

section .text
global _start

_start:
    mov esi, 100

.loop:
    cmp esi, 0
    je .exit

    mov edi, filename
    mov ebx, prefix

.copy_prefix:
    mov al, [ebx]
    inc ebx
    stosb
    cmp al, 0
    jne .copy_prefix

    dec edi

    mov eax, esi
    mov ebx, 10
    mov ebp, counter_str
    add ebp, 15
    mov byte [ebp], 0

.convert:
    dec ebp
    xor edx, edx
    div ebx
    add dl, '0'
    mov [ebp], dl
    test eax, eax
    jnz .convert

.copy_num:
    mov al, [ebp]
    stosb
    inc ebp
    cmp al, 0
    jne .copy_num

    mov eax, SYS_FS_OPEN
    mov ebx, filename
    mov ecx, 1
    int 0x80

    mov ebx, eax

    mov eax, SYS_FS_WRITE
    mov ecx, content
    mov edx, 4
    int 0x80

    dec esi
    jmp .loop

.exit:
    mov eax, SYS_EXIT
    int 0x80