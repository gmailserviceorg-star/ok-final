#!/bin/sh
# Boots the ISO in QEMU with a blank disk so files persist between boots.
[ -f disk.img ] || qemu-img create -f raw disk.img 64M
qemu-system-i386 -cdrom ClaudeOS.iso -drive file=disk.img,format=raw,if=ide -m 256 -boot d -serial stdio
