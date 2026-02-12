/* GENESIS OS v3.0 - FLUX GUI EDITION
 * Kernel Bare-metal dengan UI Jendela, Mouse, & Aplikasi Paint
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- I/O PORTS ---
static inline void outb(uint16_t port, uint8_t val) { __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) ); }
static inline uint8_t inb(uint16_t port) { uint8_t ret; __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) ); return ret; }

// --- LIMINE BOOT PROTOCOL ---
struct limine_framebuffer {
    uint64_t address; uint64_t width; uint64_t height; uint64_t pitch;
    uint16_t bpp; uint8_t memory_model; uint8_t red_mask_size; uint8_t red_mask_shift;
    uint8_t green_mask_size; uint8_t green_mask_shift; uint8_t blue_mask_size; uint8_t blue_mask_shift;
};
struct limine_framebuffer_response { uint64_t revision; uint64_t framebuffer_count; struct limine_framebuffer **framebuffers; };
struct limine_framebuffer_request { uint64_t id[4]; uint64_t revision; struct limine_framebuffer_response *response; };

static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = {0xc7b1dd30df4c8b88, 0x0a82e883a194f07b}, .revision = 0
};

// --- GRAPHICS ENGINE ---
struct limine_framebuffer *fb = NULL;

void plot(int x, int y, uint32_t color) {
    if (!fb || x < 0 || x >= (int)fb->width || y < 0 || y >= (int)fb->height) return;
    *(uint32_t *)(fb->address + y * fb->pitch + x * 4) = color;
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i=0; i<h; i++) for (int j=0; j<w; j++) plot(x+j, y+i, color);
}

// Font 5x7 Minimalis
const uint8_t font[] = {
    0x7C,0x12,0x11,0x12,0x7C, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22, 0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01,
    0x3E,0x41,0x49,0x49,0x7A, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00, 0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40,
    0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E, 0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46,
    0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F, 0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63,
    0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43
};

void draw_char(char c, int x, int y, uint32_t color) {
    if (c == ' ') return; if (c < 'A' || c > 'Z') c = '?';
    int index = (c - 'A') * 5;
    for (int i=0; i<5; i++) {
        uint8_t line = font[index + i];
        for (int j=0; j<7; j++) {
            if (line & (1 << j)) { // Scale 2x
                plot(x+i*2, y+j*2, color); plot(x+i*2+1, y+j*2, color);
                plot(x+i*2, y+j*2+1, color); plot(x+i*2+1, y+j*2+1, color);
            }
        }
    }
}

void draw_string(const char* str, int x, int y, uint32_t color) {
    while (*str) { draw_char(*str, x, y, color); x += 12; str++; }
}

// --- DATA MEMORY PAINT ---
#define CANVAS_W 100
#define CANVAS_H 80
uint32_t paint_canvas[CANVAS_H][CANVAS_W];

// --- MOUSE DRIVER ---
int mouse_x = 320, mouse_y = 240;
uint8_t mouse_cycle = 0; int8_t mouse_byte[3];
bool is_left_click = false; uint32_t current_paint_color = 0x000000;

void init_mouse() {
    outb(0x64, 0xA8); outb(0x64, 0x20);
    uint8_t status = inb(0x60) | 2;
    outb(0x64, 0x60); outb(0x60, status);
    outb(0x64, 0xD4); outb(0x60, 0xF4); inb(0x60);
}

void poll_mouse() {
    if ((inb(0x64) & 1) && (inb(0x64) & 0x20)) {
        mouse_byte[mouse_cycle++] = inb(0x60);
        if (mouse_cycle == 3) {
            mouse_cycle = 0;
            if ((mouse_byte[0] & 0xC0) == 0) {
                mouse_x += mouse_byte[1]; mouse_y -= mouse_byte[2];
                is_left_click = (mouse_byte[0] & 1);
                if (mouse_x < 0) mouse_x = 0; if (mouse_x > (int)fb->width - 10) mouse_x = (int)fb->width - 10;
                if (mouse_y < 0) mouse_y = 0; if (mouse_y > (int)fb->height - 10) mouse_y = (int)fb->height - 10;
            }
        }
    }
}

// --- WINDOW MANAGER ---
typedef struct { int x, y, w, h; const char* title; uint32_t bg_color; } Window;
Window apps[4];

void init_apps() {
    apps[0] = (Window){30, 40, 260, 160, "FILE MGR", 0xDDDDDD};
    apps[1] = (Window){310, 40, 260, 160, "BROWSER", 0x303080};
    apps[2] = (Window){30, 220, 260, 160, "EDITOR", 0x151515};
    apps[3] = (Window){310, 220, 260, 160, "PAINT", 0xEEEEEE};
}

void render_gui() {
    // Desktop Background
    draw_rect(0, 0, fb->width, fb->height, 0x005555); 
    draw_string("GENESIS OS V3", 10, 10, 0x00FF00);

    for (int i = 0; i < 4; i++) {
        Window* w = &apps[i];
        draw_rect(w->x, w->y, w->w, w->h, w->bg_color); // Window bg
        draw_rect(w->x, w->y, w->w, 20, 0x222222);      // Titlebar
        draw_string(w->title, w->x + 5, w->y + 4, 0xFFFFFF);

        if (i == 0) { // FILE MGR
            draw_rect(w->x+10, w->y+30, 20, 16, 0xFFD700); draw_string("SYS", w->x+40, w->y+35, 0x000000);
            draw_rect(w->x+10, w->y+60, 20, 16, 0xFFD700); draw_string("USR", w->x+40, w->y+65, 0x000000);
        } else if (i == 1) { // BROWSER
            draw_rect(w->x+10, w->y+30, w->w-20, 16, 0xFFFFFF); draw_string("WWW GENESIS ORG", w->x+15, w->y+35, 0x000000);
            draw_string("HELLO INTERNET", w->x+10, w->y+70, 0xFFFFFF);
        } else if (i == 2) { // EDITOR
            draw_string("LET X EQUALS TEN", w->x+10, w->y+30, 0x00FF00);
            draw_rect(w->x+10, w->y+50, 8, 14, 0x00FF00); // Cursor
        } else if (i == 3) { // PAINT
            draw_rect(w->x+10, w->y+30, 20, 20, 0xFF0000); // Palet Merah
            draw_rect(w->x+40, w->y+30, 20, 20, 0x00FF00); // Palet Hijau
            draw_rect(w->x+70, w->y+30, 20, 20, 0x0000FF); // Palet Biru
            draw_string("PICK", w->x+100, w->y+35, 0x000000);
            
            // Logika Pilih Warna (Klik Kiri + Posisi Mouse)
            if (is_left_click && mouse_y >= w->y+30 && mouse_y <= w->y+50) {
                if (mouse_x >= w->x+10 && mouse_x <= w->x+30) current_paint_color = 0xFF0000;
                if (mouse_x >= w->x+40 && mouse_x <= w->x+60) current_paint_color = 0x00FF00;
                if (mouse_x >= w->x+70 && mouse_x <= w->x+90) current_paint_color = 0x0000FF;
            }

            int cx = w->x + 10, cy = w->y + 60;
            draw_rect(cx, cy, CANVAS_W, CANVAS_H, 0xFFFFFF); // Kertas putih

            // Logika Menggambar
            if (is_left_click && mouse_x >= cx && mouse_x < cx+CANVAS_W && mouse_y >= cy && mouse_y < cy+CANVAS_H) {
                paint_canvas[mouse_y - cy][mouse_x - cx] = current_paint_color;
                if (mouse_y - cy + 1 < CANVAS_H) paint_canvas[mouse_y - cy + 1][mouse_x - cx] = current_paint_color;
                if (mouse_x - cx + 1 < CANVAS_W) paint_canvas[mouse_y - cy][mouse_x - cx + 1] = current_paint_color;
            }

            // Render coretan
            for (int py = 0; py < CANVAS_H; py++) {
                for (int px = 0; px < CANVAS_W; px++) {
                    if (paint_canvas[py][px] != 0) plot(cx + px, cy + py, paint_canvas[py][px]);
                }
            }
        }
    }
}

// --- ENTRY KERNEL ---
void _start(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) for (;;) __asm__("hlt");
    fb = framebuffer_request.response->framebuffers[0];

    for(int i=0; i<CANVAS_H; i++) for(int j=0; j<CANVAS_W; j++) paint_canvas[i][j] = 0;
    init_mouse();
    init_apps();

    while (1) {
        poll_mouse();
        render_gui();
        
        // Render Cursor Mouse (Merah saat diklik, Putih saat dilepas)
        draw_rect(mouse_x, mouse_y, 6, 6, is_left_click ? 0xFF0000 : 0xFFFFFF);
        draw_rect(mouse_x+2, mouse_y+2, 2, 2, 0x000000);
        
        // Perlambat sedikit agar emulator di HP tidak kepanasan
        for (volatile int wait = 0; wait < 100000; wait++);
    }
}    *(uint32_t *)(fb->address + y * fb->pitch + x * 4) = color;
}

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i=0; i<h; i++) {
        for (int j=0; j<w; j++) plot(x+j, y+i, color);
    }
}

// Font 5x7 Sederhana (A-Z)
const uint8_t font[] = {
    0x7C,0x12,0x11,0x12,0x7C, 0x7F,0x49,0x49,0x49,0x36, 0x3E,0x41,0x41,0x41,0x22,
    0x7F,0x41,0x41,0x22,0x1C, 0x7F,0x49,0x49,0x49,0x41, 0x7F,0x09,0x09,0x09,0x01,
    0x3E,0x41,0x49,0x49,0x7A, 0x7F,0x08,0x08,0x08,0x7F, 0x00,0x41,0x7F,0x41,0x00,
    0x20,0x40,0x41,0x3F,0x01, 0x7F,0x08,0x14,0x22,0x41, 0x7F,0x40,0x40,0x40,0x40,
    0x7F,0x02,0x0C,0x02,0x7F, 0x7F,0x04,0x08,0x10,0x7F, 0x3E,0x41,0x41,0x41,0x3E,
    0x7F,0x09,0x09,0x09,0x06, 0x3E,0x41,0x51,0x21,0x5E, 0x7F,0x09,0x19,0x29,0x46,
    0x46,0x49,0x49,0x49,0x31, 0x01,0x01,0x7F,0x01,0x01, 0x3F,0x40,0x40,0x40,0x3F,
    0x1F,0x20,0x40,0x20,0x1F, 0x3F,0x40,0x38,0x40,0x3F, 0x63,0x14,0x08,0x14,0x63,
    0x07,0x08,0x70,0x08,0x07, 0x61,0x51,0x49,0x45,0x43
};

void draw_char(char c, int x, int y, uint32_t color) {
    if (c == ' ') return;
    if (c < 'A' || c > 'Z') c = '?';
    int index = (c - 'A') * 5;
    for (int i=0; i<5; i++) {
        uint8_t line = font[index + i];
        for (int j=0; j<7; j++) {
            if (line & (1 << j)) {
                plot(x+i*2, y+j*2, color); plot(x+i*2+1, y+j*2, color);
                plot(x+i*2, y+j*2+1, color); plot(x+i*2+1, y+j*2+1, color);
            }
        }
    }
}

void draw_string(const char* str, int x, int y, uint32_t color) {
    while (*str) {
        draw_char(*str, x, y, color);
        x += 12; str++;
    }
}

// --- 4. DATA CANVAS UNTUK APLIKASI PAINT ---
#define CANVAS_W 100
#define CANVAS_H 80
uint32_t paint_canvas[CANVAS_H][CANVAS_W];

// --- 5. DRIVER MOUSE PS/2 ---
int mouse_x = 400, mouse_y = 300;
uint8_t mouse_cycle = 0;
int8_t mouse_byte[3];
bool is_left_click = false;
uint32_t current_paint_color = 0x000000; // Default cat hitam

void init_mouse() {
    outb(0x64, 0xA8); // Enable auxiliary device
    outb(0x64, 0x20); // Enable IRQ12
    uint8_t status = inb(0x60) | 2;
    outb(0x64, 0x60); outb(0x60, status);
    outb(0x64, 0xD4); outb(0x60, 0xF4); // Enable packet transmission
    inb(0x60); // Acknowledge
}

void poll_mouse() {
    uint8_t status = inb(0x64);
    if ((status & 1) && (status & 0x20)) { // Data siap dari mouse
        mouse_byte[mouse_cycle++] = inb(0x60);
        if (mouse_cycle == 3) {
            mouse_cycle = 0;
            if ((mouse_byte[0] & 0xC0) == 0) { // Valid data
                mouse_x += mouse_byte[1];
                mouse_y -= mouse_byte[2];
                is_left_click = (mouse_byte[0] & 1);
                
                // Batas layar
                if (mouse_x < 0) mouse_x = 0; if (mouse_x > 795) mouse_x = 795;
                if (mouse_y < 0) mouse_y = 0; if (mouse_y > 595) mouse_y = 595;
            }
        }
    }
}

// --- 6. WINDOW MANAGER & FLUX APPS ---
typedef struct { int x, y, w, h; const char* title; uint32_t bg_color; } Window;
Window apps[4];

// Penerjemah Sintaks Flux 
void flux_interpreter_launch(int id, const char* object_name, int x, int y, uint32_t color) {
    apps[id].x = x; apps[id].y = y;
    apps[id].w = 260; apps[id].h = 160;
    apps[id].title = object_name;
    apps[id].bg_color = color;
}

void render_desktop_and_apps() {
    // Background Desktop
    draw_rect(0, 0, fb->width, fb->height, 0x004040); // Warna Teal Gelap
    draw_string("GENESIS OS FLUX ENGINE ACTIVE", 10, 10, 0x00FF00);

    for (int i = 0; i < 4; i++) {
        Window* w = &apps[i];
        // Gambar Window Box & Title Bar
        draw_rect(w->x, w->y, w->w, w->h, w->bg_color);
        draw_rect(w->x, w->y, w->w, 20, 0x202020); 
        draw_string(w->title, w->x + 5, w->y + 4, 0xFFFFFF);

        // A. FILE MANAGER LOGIC
        if (i == 0) {
            draw_rect(w->x+10, w->y+30, 24, 20, 0xFFD700); draw_string("SYSTEM", w->x+40, w->y+35, 0x000000);
            draw_rect(w->x+10, w->y+60, 24, 20, 0xFFD700); draw_string("DATA", w->x+40, w->y+65, 0x000000);
            draw_rect(w->x+10, w->y+90, 24, 20, 0xFFD700); draw_string("USER", w->x+40, w->y+95, 0x000000);
        }
        // B. BROWSER LOGIC
        else if (i == 1) {
            draw_rect(w->x+10, w->y+30, w->w-20, 20, 0xFFFFFF); 
            draw_string("WWW GENESIS ORG", w->x+15, w->y+35, 0x000000);
            draw_string("WELCOME TO THE WEB", w->x+10, w->y+70, 0xFFFFFF);
            draw_string("NO ADS FOUND", w->x+10, w->y+90, 0x00FF00);
        }
        // C. TEXT EDITOR LOGIC
        else if (i == 2) {
            draw_string("FLUX CODE", w->x+10, w->y+30, 0x00FF00);
            draw_string("LET X EQUALS TEN", w->x+10, w->y+50, 0x00FF00);
            draw_string("PRINT X", w->x+10, w->y+70, 0x00FF00);
            draw_rect(w->x+95, w->y+70, 8, 14, 0x00FF00); // Kursor kedip
        }
        // D. PAINT LOGIC
        else if (i == 3) {
            // Gambar Palet Warna
            draw_rect(w->x+10, w->y+30, 20, 20, 0xFF0000); // Merah
            draw_rect(w->x+40, w->y+30, 20, 20, 0x00FF00); // Hijau
            draw_rect(w->x+70, w->y+30, 20, 20, 0x0000FF); // Biru
            draw_string("PICK COLOR", w->x+100, w->y+35, 0x000000);
            
            // Logika Klik Palet
            if (is_left_click && mouse_y >= w->y+30 && mouse_y <= w->y+50) {
                if (mouse_x >= w->x+10 && mouse_x <= w->x+30) current_paint_color = 0xFF0000;
                if (mouse_x >= w->x+40 && mouse_x <= w->x+60) current_paint_color = 0x00FF00;
                if (mouse_x >= w->x+70 && mouse_x <= w->x+90) current_paint_color = 0x0000FF;
            }

            // Area Canvas (Background Putih)
            int cx = w->x + 10; int cy = w->y + 60;
            draw_rect(cx, cy, CANVAS_W, CANVAS_H, 0xFFFFFF);

            // Logika Mencoret Canvas
            if (is_left_click && mouse_x >= cx && mouse_x < cx+CANVAS_W && mouse_y >= cy && mouse_y < cy+CANVAS_H) {
                paint_canvas[mouse_y - cy][mouse_x - cx] = current_paint_color;
                // Buat kuas lebih tebal (3x3 piksel)
                if (mouse_y - cy + 1 < CANVAS_H) paint_canvas[mouse_y - cy + 1][mouse_x - cx] = current_paint_color;
                if (mouse_x - cx + 1 < CANVAS_W) paint_canvas[mouse_y - cy][mouse_x - cx + 1] = current_paint_color;
            }

            // Tampilkan Canvas ke Layar
            for (int py = 0; py < CANVAS_H; py++) {
                for (int px = 0; px < CANVAS_W; px++) {
                    if (paint_canvas[py][px] != 0) { // Jika ada coretan
                        plot(cx + px, cy + py, paint_canvas[py][px]);
                    }
                }
            }
        }
    }
}

// --- 7. KERNEL ENTRY POINT ---
void _start(void) {
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) for (;;) __asm__("hlt");
    fb = framebuffer_request.response->framebuffers[0];

    // Persiapan Canvas Paint (Isi dengan angka 0 agar tidak acak)
    for(int i=0; i<CANVAS_H; i++) for(int j=0; j<CANVAS_W; j++) paint_canvas[i][j] = 0;

    init_mouse();

    // MENJALANKAN KODE FLUX YANG DIMINTA USER
    flux_interpreter_launch(0, "FILE MANAGER", 50, 50, 0xCCCCCC); // Abu-abu
    flux_interpreter_launch(1, "BROWSER", 330, 50, 0x202080);     // Biru Navigasi
    flux_interpreter_launch(2, "TEXT EDITOR", 50, 230, 0x101010); // Hitam Hacker
    flux_interpreter_launch(3, "PAINT APP", 330, 230, 0xEEEEEE);  // Putih Terang

    // Loop Utama OS (The Heartbeat)
    while (1) {
        // 1. Baca input dari mouse
        poll_mouse();
        
        // 2. Gambar ulang seluruh layar dan aplikasi
        render_desktop_and_apps();

        // 3. Gambar Cursor Mouse di atas segalanya
        uint32_t cursor_color = is_left_click ? 0xFF0000 : 0xFFFFFF; // Merah jika diklik
        draw_rect(mouse_x, mouse_y, 6, 6, cursor_color);
        draw_rect(mouse_x+2, mouse_y+2, 2, 2, 0x000000); // Titik hitam di tengah cursor

        // 4. Istirahatkan CPU sejenak agar Limbo Emulator tidak crash/hang
        for (volatile int wait = 0; wait < 500000; wait++); 
    }
}
