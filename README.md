# kchip-8
Chip-8 emulator

Because...why not? I'm thinking about writing a bunch of blog articles about emulation, and it might be a good idea to have a simplistic emulator of an easy system to display the principles.

Keys are:

1 2 3 4                                  1 2 3 C

Q W E R                                  4 5 6 D
         matching the original system's
A S D F                                  7 8 9 E

Z X C V                                  A 0 B F

"esc" exits, although I think some games will still wait for input, or something. I usually just ctrl-c out of them, after using esc to shutdown SDL.

The system runs at 600Hz, which seems to be just fine for most software.
