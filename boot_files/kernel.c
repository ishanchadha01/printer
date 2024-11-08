#include "io.h"

void main() {
    uart_init();
    uart_write_text("What's good\n");
    while (1) {
        uart_write_byte_blocking_actual('A');
    }

    while (1);
}