#include "io.h"
#include "frame_buffer.h"

void main() {
    uart_init();
    uart_write_text("What's good\n");

    frame_buffer_init();
    
    while (1) {
        draw_pixel(10,10,0x0e);
        draw_char_scaled('O',20,20,0x05,2);
        draw_string_scaled(100,100,"What's good?",0x0f, 4);
    }
    draw_pixel(10,10,0x0e);
    draw_char('O',20,20,0x05);
    draw_string(30,30,"What's good?",0x0f);

    while (1);
}