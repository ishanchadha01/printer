#include "io.h"
#include "frame_buffer.h"

void wait_msec(unsigned int n)
{
    register unsigned long f, t, r;

    // Get the current counter frequency in assembly
    asm volatile ("mrs %0, cntfrq_el0" : "=r"(f));
    // Read the current counter
    asm volatile ("mrs %0, cntpct_el0" : "=r"(t));
    // Calculate expire value for counter
    t+=((f/1000)*n)/1000;
    do{asm volatile ("mrs %0, cntpct_el0" : "=r"(r));}
    while(r<t);
}

void main() {
    // Communicate over UART to laptop
    uart_init();
    uart_write_text("What's good\n");

    // Communicate with HDMI monitor with frame buffer
    frame_buffer_init();
    draw_pixel(10,10,0x0e);
    draw_char('O',20,20,0x05);
    draw_string_scaled(30,30,"What's good?",0x0f, 3);

    for (int i=0; i<5; i++) {
        // uart_write_text("What's up\n");
        draw_string_scaled(40,30,"check a",0x0f, 3);
        wait_msec(500000);
        // uart_write_text("What's good\n");
        draw_string_scaled(50,30,"check b",0x0f, 3);
        wait_msec(500000);
    }

    draw_string_scaled(90,30,"check c",0x0f, 3);
    // uart_write_text("Check c\n");

    // Configure GPIO pins for output
    gpio_function(23, GPIO_FUNCTION_OUT); // Set pin as output step pin
    gpio_function(5, GPIO_FUNCTION_OUT);  // Set pin as output dir pin
    gpio_function(6, GPIO_FUNCTION_OUT); // Set pin as output enable pin

    // uart_write_text("check b\n");
    draw_string_scaled(120,30,"check d",0x0f, 3);

    // Actuate motor
    gpio_set(6, 1); // Enable the motor driver (assuming '1' is HIGH)
    gpio_set(5, 1); // Set direction (assuming '1' is HIGH)

    // uart_write_text("Check c\n");
    draw_string_scaled(150,60,"check e",0x0f,3);
    for (int i = 0; i < 2000; i++) { // 200 steps for 1 revolution
        gpio_set(23, 1); // Step pin HIGH
        // uart_write_text("check d\n");
        draw_string_scaled(180,60,"check f",0x0f, 3);
        wait_msec(500000); // Adjust delay for speed
        gpio_clear(23, 1); // Step pin LOW
        draw_string_scaled(210,60,"check g",0x0f, 3);
        // uart_write_text("check e\n");
        wait_msec(500000);
    }
    wait_msec(1000000); // Wait for 1 second
}