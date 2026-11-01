CFLAGS=-m32 -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-tree-loop-distribute-patterns -mgeneral-regs-only -nostdlib -fno-builtin -Wall -Wno-unused-function
all: claudeos.elf
claudeos.elf: boot.S kernel.c font.h linker.ld
	gcc $(CFLAGS) -c boot.S -o boot.o
	gcc $(CFLAGS) -c kernel.c -o kernel.o
	ld -m elf_i386 -T linker.ld -o claudeos.elf boot.o kernel.o
iso: claudeos.elf
	./build-iso.sh
clean:
	rm -f *.o claudeos.elf ClaudeOS.iso
