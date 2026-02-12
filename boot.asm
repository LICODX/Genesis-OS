MBALIGN  equ  1 << 0
MEMINFO  equ  1 << 1
VIDINFO  equ  1 << 2
FLAGS    equ  MBALIGN | MEMINFO | VIDINFO
MAGIC    equ  0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    dd 0, 0, 0, 0, 0
    dd 0    ; 0 = linear graphics mode
    dd 800  ; Lebar layar
    dd 600  ; Tinggi layar
    dd 32   ; Kedalaman warna

section .bss
align 16
stack_bottom:
resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_top
    push eax ; Push Multiboot Magic Number
    push ebx ; Push Multiboot Info Struct
    call kernel_main
    cli
.hang:
    hlt
    jmp .hang
