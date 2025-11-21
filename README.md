# Goal
Create a self-sustaining, multi-end effector system with different modes of actuation for managing a garden

# Motion
## completed
- start motor from Arduino script that interfaces with RAMPS board
- create custom base kernel to Raspberry Pi for RTOS
- communicate over UART to Pi
- write visual output via frame buffer and mailbox
- send signals from Pi => Arduino => RAMPS board for motor control

- build frame of Core XY 3D printer
![Core XY Print Frame](./img/corexy.png)

## todo
- solder level shifter module to Pi so that it can write outputs to RAMPS directly
    - this enables real-time communication instead of hop through Arduino


# Planning
## completed
- vector and triangle primitives
- stl parser
- compute line segments of intersecting mesh triangles with each layer

## todo
- binary stl parser
- contouring to turn each layer into path
- path visualization tool

## todo


# Sensing
## todo
- start with cameras probably