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
    draw_string(30,30,"What's good?",0x0f);

    for (int i=0; i<100; i++) {
        uart_write_text("What's up\n");
        wait_msec(300000);
        uart_write_text("What's good\n");
        wait_msec(300000);
    }

    // Actuate motor
    int stepPin = 4; // X-axis step pin
    int dirPin = 5;  // X-axis direction pin
    int enablePin = 6; // Enable pin
    // Configure GPIO pins for output
    gpio_function(stepPin, GPIO_FUNCTION_OUT); // Set pin as output
    gpio_function(dirPin, GPIO_FUNCTION_OUT);  // Set pin as output
    gpio_function(enablePin, GPIO_FUNCTION_OUT); // Set pin as output

    // Actuate motor
    gpio_set(enablePin, 1); // Enable the motor driver (assuming '1' is HIGH)
    gpio_set(dirPin, 1); // Set direction (assuming '1' is HIGH)
    for (int i = 0; i < 200; i++) { // 200 steps for 1 revolution
        gpio_set(stepPin, 1); // Step pin HIGH
        wait_msec(500000); // Adjust delay for speed
        gpio_clear(stepPin, 1); // Step pin LOW
        wait_msec(500000);
    }
    wait_msec(1000000); // Wait for 1 second

    // digitalWrite(enablePin, LOW); // Enable the motor driver
    // digitalWrite(dirPin, HIGH); // Set direction
    // for (int i = 0; i < 200; i++) { // 200 steps for 1 revolution (depends on your motor)
    //     digitalWrite(stepPin, HIGH);
    //     delayMicroseconds(500); // Adjust delay for speed
    //     digitalWrite(stepPin, LOW);
    //     delayMicroseconds(500);
    // }
    // delay(1000);
}