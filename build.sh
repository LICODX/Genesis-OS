#!/bin/bash
set -e

echo "[*] Installing dependencies..."
sudo apt-get update
sudo apt-get install -y lld llvm clang nasm mtools xorriso

echo "[*] Downloading Limine Bootloader..."
git clone https://github.com/limine-bootloader/limine.git --branch=v5.x-branch-binary --depth=1
make -C limine

echo "[*] Compiling Kernel (64-bit)..."
# Kita compile tanpa standard library (-ffreestanding)
# Tanpa interupsi merah (-mno-red-zone) karena kita di kernel mode
clang -target x86_64-unknown-none-elf -ffreestanding -mno-red-zone \
      -c kernel.c -o kernel.o

echo "[*] Linking..."
ld.lld -nostdlib -z max-page-size=0x1000 -Ttext=0xffffffff80000000 \
       kernel.o -o genesis.bin

echo "[*] Creating ISO..."
mkdir -p iso_root
cp genesis.bin iso_root/
cp limine.cfg iso_root/
cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/

xorriso -as mkisofs -b limine-bios-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot limine-uefi-cd.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        iso_root -o genesis.iso

./limine/limine bios-install genesis.iso

echo "SUCCESS! Download genesis.iso now."
