#!/bin/sh
# Builds ClaudeOS.iso (needs: gcc, binutils, grub-mkrescue, xorriso, mtools; on Debian/Ubuntu: sudo apt install build-essential grub-pc-bin xorriso mtools)
set -e
make
mkdir -p iso/boot/grub
cp claudeos.elf iso/boot/claudeos.elf
cp grub/grub.cfg iso/boot/grub/grub.cfg
grub-mkrescue -o ClaudeOS.iso iso
echo "Built ClaudeOS.iso"
