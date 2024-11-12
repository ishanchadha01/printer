#include "io.h"

void main() {
    uart_init();
    uart_write_text("What's good\n");

    while (1);
}