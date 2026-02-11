#include <stdint.h>

// Struktur Info Grafik dari Bootloader (Multiboot2 / UEFI)
struct FramebufferInfo {
    uint32_t* address; // Pointer ke VRAM Kartu Grafis
    uint32_t pitch;    // Lebar baris dalam bytes
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
};

// Fungsi Menggambar Pixel (Tanpa Driver GPU, murni CPU rendering)
void put_pixel(struct FramebufferInfo* fb, int x, int y, uint32_t color) {
    // Menghitung lokasi memori piksel
    // Ini membuktikan kita tidak pakai mode teks 1980an lagi.
    uint32_t offset = y * (fb->pitch / 4) + x;
    fb->address[offset] = color;
}

// Entry Point
void _start(struct FramebufferInfo* fb) {
    // 1. Isi Layar dengan Warna "Genesis Blue" (Background)
    for (int y = 0; y < fb->height; y++) {
        for (int x = 0; x < fb->width; x++) {
            put_pixel(fb, x, y, 0xFF101040); // Format: ARGB
        }
    }

    // 2. Gambar Kotak Merah (Indikator Kernel Hidup)
    for (int y = 100; y < 200; y++) {
        for (int x = 100; x < 200; x++) {
            put_pixel(fb, x, y, 0xFFFF0000); // Merah
        }
    }
    
    // Disinilah "Quantum Resource Management" (sekarang Energy Accounting) akan berjalan
    // Loop kosong yang dioptimalkan agar CPU tidak panas (HLT)
    while (1) {
        __asm__("hlt");
    }
}


