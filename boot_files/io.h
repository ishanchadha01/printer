void uart_init();
void uart_write_text(char* buffer);
void uart_write_byte_blocking_actual(unsigned char ch);
void mmio_write(long reg, unsigned int val);
unsigned int mmio_read(long reg);