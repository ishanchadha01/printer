.section ".text.boot"

.global _start

_start:
    mrs     x1, MPIDR_EL1       // get value of register which stores CPU info
    and     x1, x1, 3           // get last 3 bits since its quadcore
    cbz     x1, 2f              // branch forward to 2 if cpu num is 0

1: 
    wfe                         // if not cpu 0 then prog comes here and waits for event
    b       1b                  // branch backward to 1 

2:
    ldr     x1, =_start         // resolves _start to an absolute addr and puts into x1 to bring base addr near code segment
    mov     sp, x1              // move stack ptr to start of code
    ldr     x1, =__bss_start    // move x1 to bss, where zero initialized data is stored
    ldr     w2, =__bss_size     // size of section

3:
    cbz     w2, 4f              // quit loop if 0
    str     xzr, [x1], #8       // stores value of xzr, which is always 0, into x1, and then increments x1 by 8
    sub     w2, w2, 1           // decrement size of bss left to check
    cbnz    w2, 3b              // loop if nonzero

4:
    bl      main                // look for main method in C, make sure this doesn't return right now
    b       1b                  // halt master core if it does return