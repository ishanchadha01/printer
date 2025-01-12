void uart_init();
void uart_write_text(char* buffer);
void uart_write_byte_blocking_actual(unsigned char ch);
void mmio_write(long reg, unsigned int val);
unsigned int mmio_read(long reg);

enum {
    GPIO_MAX_PIN       = 27,
    GPIO_FUNCTION_OUT  = 1,
    GPIO_FUNCTION_ALT0 = 4,
};

unsigned int gpio_set     (unsigned int pin_number, unsigned int value);
unsigned int gpio_clear   (unsigned int pin_number, unsigned int value);
unsigned int gpio_function(unsigned int pin_number, unsigned int function);