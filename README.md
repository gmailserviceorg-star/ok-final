# ClaudeOS (bare metal)

Files in this repo: kernel (kernel.c, boot.S), GRUB config, boot.cpp (the Windows installer, built into boot.exe) and build.yml.
Run ./build.sh (Linux/WSL/Bash with the tools) to make claudeos.elf, boot.exe and ClaudeOS.iso (boot.exe and autorun.inf are inside the ISO).
build.yml does the same on GitHub if you ever want it.

## Use the ISO
Flash ClaudeOS.iso to a USB stick (Rufus: DD mode) or attach it to a VM (32-bit "Other", BIOS, not EFI, 256 MB, optional blank IDE disk).
First start runs the setup wizard. Login afterwards: the account you create, or claude / claude if you skip it.

## Use boot.exe (only when you cannot use a USB stick)
Mount the ISO in Windows, run boot.exe. It lists disks (empty disks over 1 GB are marked Recommended), can create a 2 GB drive from free space,
asks for administrator permission, copies ClaudeOS and writes GRUB so the PC shows a Windows / ClaudeOS menu.
Only for legacy-BIOS (CSM) PCs with an MBR Windows disk. UEFI/GPT is refused. It backs up the first 1 MB of the disk first.
Try `boot.exe --dry-run` first. Restore: `boot.exe --restore <backup.bin> <disk>`; or `bootrec /fixmbr` from a Windows recovery disk.

## Testing
`./test-qemu.sh` boots ClaudeOS.iso in QEMU without a window, sends real keystrokes through the setup skip path and the login, and checks the serial log. It also saves a screenshot to /tmp/cos-desktop.ppm. It has not been run by the author: the build environment had no QEMU.

## Storage
Disks: ATA/IDE and AHCI (SATA) drivers. ClaudeOS only uses a disk that is blank or already ClaudeOS-formatted, so setup and your files persist.
No disk attached = RAM only, and setup runs every boot. In VirtualBox add a blank virtual disk (SATA or IDE).

## What the ISO has
GRUB boot, framebuffer graphics, PS/2 keyboard and mouse, timer, clock, ATA disk persistence, filesystem, setup wizard (keyboard layouts with optional second layout,
offline network page, license and privacy that must be scrolled and agreed, wallpaper and accent, features, account with hint, boot message, 10 minute finish),
login, desktop, windows with smooth open/close/maximize animations (Windows.json), Terminal (about 25 commands, exec del forms), Files, Editor, Claudebrowse
(own HTML tag renderer + search engine over /www), Recovery (Shift+F12), admin terminal (Shift+F10, works in setup), password reset (Shift+F11),
Ctrl+X shutdown, hold Ctrl+X restart, claude\run /f *.skip \else.

## Not done yet
USB, network and audio-chip drivers, a real internet browser, JavaScript engine, apps/.ui/.compile system, python.spm, image/video/music players, borderless GUI boot.exe.

## Also missing (the HTML version has these)
Network/Wi-Fi/USB drivers, JavaScript and CSS in the browser, .app/.ui/.link/.compile/.spm app system, USB Drive A, bootf, display languages other than English,
wallpaper upload, the System/Data log and binary-format files, and most of the customization depth. Untested on real hardware.
