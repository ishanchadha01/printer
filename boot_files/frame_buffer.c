#include "io.h"
#include "mailbox.h"
#include "frame_buffer.h"
#include "terminal.h"

unsigned int width, height, pitch, isrgb;
unsigned char *frame_buffer;

void frame_buffer_init() {
    mailbox[0] = 35*4; // Length of message in bytes
    mailbox[1] = MBOX_REQUEST;

    mailbox[2] = MBOX_TAG_SETPHYWH; // Tag identifier
    mailbox[3] = 8; // Value size in bytes
    mailbox[4] = 0;
    mailbox[5] = 1920; // Value(width)
    mailbox[6] = 1080; // Value(height)

    mailbox[7] = MBOX_TAG_SETVIRTWH;
    mailbox[8] = 8;
    mailbox[9] = 8;
    mailbox[10] = 1920;
    mailbox[11] = 1080;

    mailbox[12] = MBOX_TAG_SETVIRTOFF;
    mailbox[13] = 8;
    mailbox[14] = 8;
    mailbox[15] = 0; // Value(x)
    mailbox[16] = 0; // Value(y)

    mailbox[17] = MBOX_TAG_SETDEPTH;
    mailbox[18] = 4;
    mailbox[19] = 4;
    mailbox[20] = 32; // Bits per pixel

    mailbox[21] = MBOX_TAG_SETPXLORDR;
    mailbox[22] = 4;
    mailbox[23] = 4;
    mailbox[24] = 1; // RGB

    mailbox[25] = MBOX_TAG_GETFB;
    mailbox[26] = 8;
    mailbox[27] = 8;
    mailbox[28] = 4096; // FrameBufferInfo.pointer
    mailbox[29] = 0;    // FrameBufferInfo.size

    mailbox[30] = MBOX_TAG_GETPITCH;
    mailbox[31] = 4;
    mailbox[32] = 4;
    mailbox[33] = 0; // Bytes per line

    mailbox[34] = MBOX_TAG_LAST;

    // Check call is successful and we have a pointer with depth 32
    if (mailbox_call(MBOX_CH_PROP) && mailbox[20] == 32 && mailbox[28] != 0) {
        mailbox[28] &= 0x3FFFFFFF; // Convert GPU address to ARM address
        width = mailbox[10];       // Actual physical width
        height = mailbox[11];      // Actual physical height
        pitch = mailbox[33];       // Number of bytes per line
        isrgb = mailbox[24];       // Pixel order
        frame_buffer = (unsigned char *)((long)mailbox[28]);
    }
}

void draw_pixel(int x, int y, unsigned char attr) {
    int offsets = pitch * y + 4 * x;
    *((unsigned int*) (frame_buffer + offsets)) = vgapal[attr & 0x0f];
}

void draw_char(unsigned char ch, int x, int y, unsigned char attr) {
    unsigned char* glyph = (unsigned char*) &font + (ch<FONT_NUMGLYPHS ? ch : 0) * FONT_BPG;
    for (int i=0; i<FONT_HEIGHT; i++) {
        for (int j=0; j<FONT_WIDTH; j++) {
            unsigned char mask = 1 << j;
            unsigned char col =  (*glyph & mask) ? attr & 0x0f : (attr & 0xf0) >> 4;
            draw_pixel(x+j, y+i, col);
        }
        glyph += FONT_BPL;
    }
}

void draw_char_scaled(unsigned char ch, int x, int y, unsigned char attr, int scale) {
    unsigned char* glyph = font[ch];  // Access the glyph for character 'ch'
    for (int i = 0; i < FONT_HEIGHT; i++) {
        for (int j = 0; j < FONT_WIDTH; j++) {
            unsigned char mask = 1 << j;  // Match the mask calculation with the original function
            unsigned char col = (glyph[i] & mask) ? (attr & 0x0f) : ((attr & 0xf0) >> 4);  // Apply foreground/background color
            // Draw each pixel in the bit as a scale x scale block
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    draw_pixel(x + j * scale + dx, y + i * scale + dy, col);
                }
            }
        }
    }
}

void draw_string(int x, int y, char* s, unsigned char attr) {
    while (*s) {
        if (*s == '\r') {
            x = 0;
        } else if (*s == '\n') {
            x = 0; y += FONT_HEIGHT;
        } else {
        draw_char(*s, x, y, attr);
            x += FONT_WIDTH;
        }
        s++;
    }
}

void draw_string_scaled(int x, int y, char* s, unsigned char attr, int scale) {
    int original_x = x; // Save the original starting x position
    while (*s) {
        if (*s == '\r') {
            x = original_x; // Reset x to the start of the line
        } else if (*s == '\n') {
            x = original_x; // Reset x to the start of the line
            y += FONT_HEIGHT * scale; // Move down by the scaled font height
        } else {
            draw_char_scaled(*s, x, y, attr, scale);
            x += FONT_WIDTH * scale; // Move right by the scaled font width
        }
        s++; // Move to the next character
    }
}
