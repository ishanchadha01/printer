#include "io.h"

// Align buffer with 16 byte memory, only upper 28 bits of addr can be passed
// This ensures that the lower 4 bits of the addr are always 0, so upper 28 bits out of 32 are passed
volatile unsigned int __attribute__((aligned(16))) mailbox[36];

// Base addr for peripherals in RPi 3b+
#define PERIPHERAL_BASE 0x3F000000

enum {
    VIDEOCORE_MBOX = (PERIPHERAL_BASE + 0x0000B880),
    MBOX_READ      = (VIDEOCORE_MBOX + 0x0),
    MBOX_POLL      = (VIDEOCORE_MBOX + 0x10),
    MBOX_SENDER    = (VIDEOCORE_MBOX + 0x14),
    MBOX_STATUS    = (VIDEOCORE_MBOX + 0x18),
    MBOX_CONFIG    = (VIDEOCORE_MBOX + 0x1C),
    MBOX_WRITE     = (VIDEOCORE_MBOX + 0x20),
    MBOX_RESPONSE  = 0x80000000,
    MBOX_FULL      = 0x80000000,
    MBOX_EMPTY     = 0x40000000
};

unsigned int mailbox_call(unsigned char ch) {
    // separate out upper 32 bits by masking with 0xFFFFFFF0
    long addr_long = ((long) &mailbox) & (~0xF);

    // convert to unsigned int so we can get lower 4 bits too
    unsigned int addr_and_ch = (unsigned int) addr_long | (ch & 0xF);

    // write addr of buffer to mailbox with DMA channel appended
    mmio_write(MBOX_WRITE, addr_and_ch);

    while (1) {
        // listen for reply
        while (mmio_read(MBOX_STATUS) & MBOX_EMPTY);

        // check if reply is successful
        if (addr_and_ch == mmio_read(MBOX_READ)) {
            return mailbox[1] == MBOX_RESPONSE;
        }
    }
    return 0;
}
