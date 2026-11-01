#!/bin/sh
# ClaudeOS build script: kernel -> boot.exe -> ClaudeOS.iso   (run from a Linux shell, WSL, or any Bash that has the tools)
set -e
cd "$(dirname "$0")"
need() { command -v "$1" >/dev/null 2>&1; }
sed -i 's/\r$//' *.sh Makefile boot.S linker.ld grub/grub.cfg 2>/dev/null || true

echo "[1/4] Building the kernel"
need gcc || { echo "gcc is missing. Install: sudo apt install build-essential gcc-multilib"; exit 1; }
make

echo "[2/4] Building GRUB images for boot.exe"
if need grub-mkimage && need xxd; then
  printf '%s\n' 'search --no-floppy --file --set=root /claudeos/boot/claudeos.elf' 'set prefix=($root)/claudeos/grub' 'configfile ($root)/claudeos/grub/grub.cfg' > early.cfg
  grub-mkimage -O i386-pc -o core.img -c early.cfg -p /claudeos/grub biosdisk part_msdos part_gpt fat ntfs ext2 multiboot chain normal configfile search search_fs_file search_fs_uuid echo ls cat test sleep all_video boot reboot halt terminal minicmd extcmd gettext datetime crypto bufio
  cp /usr/lib/grub/i386-pc/boot.img boot.img
  { xxd -i boot.img; xxd -i core.img; xxd -i claudeos.elf; } > payload.h
else
  echo "  skipped (needs grub-pc-bin and xxd: sudo apt install grub-pc-bin xxd)"
fi

echo "[3/4] Building boot.exe"
if [ -f payload.h ] && need x86_64-w64-mingw32-g++; then
  x86_64-w64-mingw32-g++ -std=c++17 -O2 -static -static-libgcc -static-libstdc++ -o boot.exe boot.cpp -lshell32 -ladvapi32
else
  echo "  skipped (needs mingw-w64: sudo apt install mingw-w64)"
fi

echo "[4/4] Building ClaudeOS.iso"
if need grub-mkrescue && need xorriso; then
  rm -rf iso && mkdir -p iso/boot/grub
  cp claudeos.elf iso/boot/claudeos.elf
  cp grub/grub.cfg iso/boot/grub/grub.cfg
  [ -f boot.exe ] && cp boot.exe iso/boot.exe && printf '[autorun]\r\nopen=boot.exe\r\nlabel=ClaudeOS Setup\r\n' > iso/autorun.inf
  cp README.md iso/README.TXT
  grub-mkrescue -o ClaudeOS.iso iso
  echo "Done: ClaudeOS.iso$( [ -f boot.exe ] && echo ' (boot.exe inside)' )"
else
  echo "  skipped (needs grub-pc-bin, xorriso, mtools: sudo apt install grub-pc-bin xorriso mtools)"
fi
