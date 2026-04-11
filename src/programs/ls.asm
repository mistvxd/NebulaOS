[BITS 32]

%define SYS_EXIT 1
%define SYS_WRITE 2
%define SYS_FS_READDIR 3

section .data
nl db 10
prefix db 16, 32, "["
prefix_len equ $ - prefix
suffix db "]"
suffix_len equ $ - suffix
nofiles db "[empty]", 10
nofiles_len equ $ - nofiles

section .bss
ent resb 36
i   resd 1
printed resd 1

section .text
global _start

_start:
    mov dword [i], 0
    mov dword [printed], 0

    mov eax, [esp + 4]
    cmp eax, 1
    jle .no_arg

    mov ebx, [esp + 8]
    mov edi, [ebx + 4]
    jmp .arg_done

.no_arg:
    mov edi, 0

.arg_done:

.loop:
    mov eax, SYS_FS_READDIR
    mov ebx, edi
    mov ecx, [i]
    mov edx, ent
    int 0x80

    mov al, [ent]
    cmp al, 0
    je .done

    mov eax, SYS_WRITE
    mov ebx, prefix
    mov ecx, 0x07
    mov edx, prefix_len
    int 0x80

    mov esi, ent
    call strlen

    mov edx, eax

    mov eax, SYS_WRITE
    mov ebx, ent
    mov ecx, 0x0F
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, suffix
    mov ecx, 0x07
    mov edx, suffix_len
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, nl
    mov ecx, 0x0F
    mov edx, 1
    int 0x80

    mov dword [printed], 1

    inc dword [i]
    jmp .loop

.done:
    cmp dword [printed], 0
    jne .exit

    mov eax, SYS_WRITE
    mov ebx, nofiles
    mov ecx, 0x08
    mov edx, nofiles_len
    int 0x80

.exit:
    mov eax, SYS_EXIT
    int 0x80

strlen:
    xor eax, eax
.str_loop:
    cmp byte [esi + eax], 0
    je .end
    inc eax
    jmp .str_loop
.end:
    ret