// GPIO config functions
#include "io.h"

enum {
    PERIPHERAL_BASE = 0x3F000000, // starting addr of memory-mapped IO
    GPFSEL0         = PERIPHERAL_BASE + 0x200000, // gpio function select 0
    GPFSEL1         = PERIPHERAL_BASE + 0x200004, // gpio function select 1
    GPSET0          = PERIPHERAL_BASE + 0x20001C, // gpio pin output set 0
    GPCLR0          = PERIPHERAL_BASE + 0x200028, // gpio pin output clear 0
    // GPPUPPDN0       = PERIPHERAL_BASE + 0x2000E4 // gpio pull up / pull down register 0, only for rpi4+
    GPPUD           = PERIPHERAL_BASE + 0x200094,
    GPPUDCLK0       = PERIPHERAL_BASE + 0x200098
};

enum {
    Pull_None = 0,
};

unsigned int mmio_read(long reg) { return *(volatile unsigned int *)reg; }

void mmio_write(long reg, unsigned int val) { *(volatile unsigned int *)reg = val; }


unsigned int gpio_call(unsigned int pin_number, unsigned int value, unsigned int base, unsigned int field_size, unsigned int field_max) {
    unsigned int field_mask = (1 << field_size) - 1;
  
    if (pin_number > field_max) return 0;
    if (value > field_mask) return 0; 

    unsigned int num_fields = 32 / field_size;
    unsigned int reg = base + ((pin_number / num_fields) * 4);
    unsigned int shift = (pin_number % num_fields) * field_size;

    unsigned int curval = mmio_read(reg);
    curval &= ~(field_mask << shift);
    curval |= value << shift;
    mmio_write(reg, curval);

    return 1;
}

unsigned int gpio_set     (unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPSET0, 1, GPIO_MAX_PIN); }
unsigned int gpio_clear   (unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPCLR0, 1, GPIO_MAX_PIN); }

void gpio_pull(unsigned int pin_number, unsigned int value) {
    mmio_write(GPPUD, value);        // Set the desired pull-up/pull-down control signal
    for (int i = 0; i < 150; i++) {} // Short delay
    
    mmio_write(GPPUDCLK0, (1 << pin_number)); // Clock the control signal into the selected GPIO pin
    for (int i = 0; i < 150; i++) {} // Short delay

    mmio_write(GPPUD, 0);            // Remove the control signal
    mmio_write(GPPUDCLK0, 0);        // Remove the clock
}

// unsigned int gpio_function(unsigned int pin_number, unsigned int value) { return gpio_call(pin_number, value, GPFSEL1, 3, GPIO_MAX_PIN); }
unsigned int gpio_function(unsigned int pin_number, unsigned int function) {
    unsigned int reg = GPFSEL0 + (pin_number / 10) * 4;  // Calculate correct GPFSEL register
    unsigned int shift = (pin_number % 10) * 3;          // Calculate bit shift for specific pin
    unsigned int mask = ~(0x7 << shift);                 // Mask to clear bits for this pin

    unsigned int curval = mmio_read(reg);
    curval &= mask;                                      // Clear current bits
    curval |= (function << shift);                       // Set new function bits
    mmio_write(reg, curval);

    return 1;                                            // Indicate success
}

void gpio_use_as_alt0(unsigned int pin_number) {
    gpio_pull(pin_number, Pull_None);
    gpio_function(pin_number, GPIO_FUNCTION_ALT0); // maps alternative function 5 (Tx/Rx) to this pin
}

// UART config functions

enum {
    AUX_BASE        = PERIPHERAL_BASE + 0x215000,
    AUX_ENABLES     = AUX_BASE + 4,
    AUX_MU_IO_REG   = AUX_BASE + 64,
    AUX_MU_IER_REG  = AUX_BASE + 68,
    AUX_MU_IIR_REG  = AUX_BASE + 72,
    AUX_MU_LCR_REG  = AUX_BASE + 76,
    AUX_MU_MCR_REG  = AUX_BASE + 80,
    AUX_MU_LSR_REG  = AUX_BASE + 84,
    AUX_MU_CNTL_REG = AUX_BASE + 96,
    AUX_MU_BAUD_REG = AUX_BASE + 104,
    AUX_UART_CLOCK  = 250000000,
    UART_MAX_QUEUE  = 16 * 1024
};

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

void uart_init() {
    mmio_write(AUX_ENABLES, 1); //enable UART1
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_CNTL_REG, 0);
    mmio_write(AUX_MU_LCR_REG, 3); //8 bits
    mmio_write(AUX_MU_MCR_REG, 0);
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_IIR_REG, 0xC6); //disable interrupts
    mmio_write(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
    gpio_use_as_alt0(14);
    gpio_use_as_alt0(15);
    mmio_write(AUX_MU_CNTL_REG, 3); //enable RX/TX
}

unsigned int uart_is_write_byte_ready() { return mmio_read(AUX_MU_LSR_REG) & 0x20; }

void uart_write_byte_blocking_actual(unsigned char ch) {
    while (!uart_is_write_byte_ready()); 
    mmio_write(AUX_MU_IO_REG, (unsigned int)ch);
}

void uart_write_text(char *buffer) {
    while (*buffer) {
       if (*buffer == '\n') uart_write_byte_blocking_actual('\r');
       uart_write_byte_blocking_actual(*buffer++);
    }
}