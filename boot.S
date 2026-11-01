.set MAGIC,0x1BADB002
.set FLAGS,0x7
.section .multiboot
.align 4
.long MAGIC,FLAGS,-(MAGIC+FLAGS)
.long 0,0,0,0,0
.long 0,1024,768,32
.section .bss
.align 16
stack_bot: .skip 65536
stack_top:
.section .text
.global _start
_start:
 cli
 mov $stack_top,%esp
 push %ebx
 push %eax
 call kmain
1: hlt
 jmp 1b
