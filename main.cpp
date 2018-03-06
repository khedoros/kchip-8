#include<stdlib.h>
#include<time.h>
#include<stdint.h>
#include<stdio.h>
#include<assert.h>
#include<sys/time.h>
#include<SDL2/SDL.h>

#define DIGIT1 0xF000
#define DIGIT2 0x0F00
#define DIGIT3 0x00F0
#define DIGIT4 0x000F

//Memory areas
uint8_t memory[4096] = {0};
bool screen[64][32] = {{false}};
uint16_t stack[128] = {0};
bool keys[16] = {false};

bool font[16 * 5] = {0xf0,0x90,0x90,0x90,0xf0, //0
                     0x20,0x60,0x20,0x20,0x70, //1
                     0xf0,0x10,0xf0,0x80,0xf0, //2
                     0xf0,0x10,0xf0,0x10,0xf0, //3
                     0x90,0x90,0xf0,0x10,0x10, //4
                     0xf0,0x80,0xf0,0x10,0xf0, //5
                     0xf0,0x80,0xf0,0x90,0xf0, //6
                     0xf0,0x10,0x20,0x40,0x40, //7
                     0xf0,0x90,0xf0,0x90,0xf0, //8
                     0xf0,0x90,0xf0,0x10,0xf0, //9
                     0xf0,0x90,0xf0,0x90,0x90, //A
                     0xe0,0x90,0xe0,0x90,0xe0, //B
                     0xf0,0x80,0x80,0x80,0xf0, //C
                     0xe0,0x90,0x90,0x90,0xe0, //D
                     0xf0,0x80,0xf0,0x80,0xf0, //E
                     0xf0,0x80,0xf0,0x80,0x80};//F

SDL_Window * window = NULL;
SDL_Renderer * renderer = NULL;
SDL_Surface * buffer = NULL;
SDL_Texture * texture = NULL;
uint32_t col[2] = {0};
uint8_t keymap[512] = {255};

//Registers
uint8_t registers[16] = {0};
uint8_t sp = 127;
uint16_t pc = 0x200; //"Program Counter", which tracks where in the program we are
uint16_t I = 0x00;   //"I", which stores a memory address.
uint8_t delay_timer = 0x00;
uint8_t sound_timer = 0x00;

uint8_t wait_key = 0x00;
bool waiting = false;

bool quit = false;

uint16_t read16(uint16_t addr) {
    return uint16_t(256 * uint16_t(memory[addr]) + uint16_t(memory[addr+1]));
}

void pset(int x, int y) {
    uint32_t * pixels = ((uint32_t *)buffer->pixels);
    pixels[y*64+x] = col[screen[x][y]];
}

void disp_clear() {
    for(int x=0;x<64;x++) {
        for(int y=0;y<32;y++) {
            screen[x][y] = false;
            pset(x,y);
        }
    }
}

void draw(uint8_t x, uint8_t y, uint8_t h) {
    bool flipped = false;
    for(int i=0;i<h&&i+y<32;i++) {
        uint8_t ypos = y+i;
        uint8_t val = memory[I+i];
        for(int j=0;j<8 && j+x<64;j++) {
            uint8_t xpos = x+j;
            bool pixel = ((val & (1<<(7-j))) > 0);
            if(pixel && screen[xpos][ypos]) {
                flipped = true;
            }
            screen[xpos][ypos] ^= pixel;
            pset(xpos,ypos);
        }
    }
}

bool load_rom(int argc, char * argv[]) {
    if(argc==2) {
        FILE * rom = fopen(argv[1], "r");
        if(rom) {
            fseek(rom, 0, SEEK_END);
            long int rom_size = ftell(rom);
            fseek(rom, 0, SEEK_SET);
            if(rom_size > 0 && rom_size <= 4096 - 512) {
                long int read_data = fread(&(memory[512]), 1, rom_size, rom);
                if(read_data == rom_size) {
                    printf("Read %ld bytes from file %s.\n", read_data, argv[1]);
                    fclose(rom);
                    return true;
                }
                else {
                    fprintf(stderr, "Read %ld bytes, but expected %ld bytes.\n", read_data, rom_size);
                    fclose(rom);
                    return false;
                }
            }
            else {
                fprintf(stderr, "Saw a file of %ld bytes, but expected between %d bytes and %d bytes.\n", rom_size, 1, 4096-512);
                fclose(rom);
                return false;
            }
        }
        else {
            fprintf(stderr, "Failed to open the file at %s.\n", argv[1]);
            return false;
        }
    }
    else {
        fprintf(stderr, "Provide a single argument: the ROM to load.\n");
        return false;
    }
}

void flip() {
    SDL_DestroyTexture(texture);
    texture = NULL;
    texture = SDL_CreateTextureFromSurface(renderer, buffer);
    SDL_RenderCopy(renderer,texture,NULL,NULL);
    SDL_RenderPresent(renderer);
}


uint32_t timer_callback(uint32_t interval, void *param) {
    //printf("Callback: ");
    if(delay_timer) {
        delay_timer--;
        //printf("Decremented delay to %d, ", delay_timer);
    }
    if(sound_timer) {
        sound_timer--;
        //printf("Decremented sound to %d, ", sound_timer);
    }

    SDL_Event event;
    int events = 0;
    while(SDL_PollEvent(&event)) {
        events++;
        switch(event.type) {
            case SDL_KEYDOWN:  /* Handle a KEYDOWN event */
                //printf("util::Keydown\n");
                assert(event.key.keysym.scancode < 512);
                if(event.key.keysym.scancode==SDL_SCANCODE_ESCAPE) {
                    quit = true;
                    SDL_Quit();
                    return 1;
                }
                else if(keymap[event.key.keysym.scancode] != 255) {
                    printf("Key[%d]: %d\n", event.key.keysym.scancode, keymap[event.key.keysym.scancode]);
                    assert(keymap[event.key.keysym.scancode] < 16);
                    keys[keymap[event.key.keysym.scancode]] = true;
                }
                break;
            case SDL_KEYUP: /* Handle a KEYUP event*/
                assert(event.key.keysym.scancode < 512);
                if(keymap[event.key.keysym.scancode] != 255) {
                    printf("Key[%d]: %d\n", event.key.keysym.scancode, keymap[event.key.keysym.scancode]);
                    assert(keymap[event.key.keysym.scancode] < 16);
                    keys[keymap[event.key.keysym.scancode]] = false;
                }
                //printf("util::Keyup\n");
                break;
            case SDL_WINDOWEVENT:
                //printf("util::Window event: ");
                switch(event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
                        //printf("closed\n");
                        SDL_Quit();
                        quit = true;
                        return 1;
                        break;
                    default:
                        SDL_FlushEvent(event.type);
                }
                break;
            case SDL_QUIT:
                //printf("util::SDL Quit triggered\n");
                SDL_Quit();
                quit = true;
                break;
            default: /* Report an unhandled event */
                SDL_FlushEvent(event.type);
                break;
        }
    }
    //printf("Processed %d events\n", events);
    flip();
    SDL_Delay(15);
    SDL_AddTimer(16, timer_callback, NULL);
}

bool vid_init() {
        /* Initialize the SDL library */
        window = SDL_CreateWindow("KhedGB",
                                  SDL_WINDOWPOS_CENTERED,
                                  SDL_WINDOWPOS_CENTERED,
                                  640, 320,
                                  SDL_WINDOW_RESIZABLE);
        if ( window == NULL ) {
            fprintf(stderr, "lcd::Couldn't set 640x320x32 video mode: %s\nStarting without video output.\n", SDL_GetError());
            //exit(1);
            return false;
        }

        SDL_SetWindowMinimumSize(window, 640, 320);

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE/*|SDL_RENDERER_PRESENTVSYNC*/);
        if(!renderer) {
            fprintf(stderr, "lcd::Couldn't create a renderer: %s\nStarting without video output.\n",
                    SDL_GetError());
            SDL_DestroyWindow(window);
            window = NULL;
            return false;
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,64,32);
        if(!texture) {
            fprintf(stderr, "lcd::Couldn't create a texture: %s\nStarting without video output.\n", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            renderer = NULL;
            SDL_DestroyWindow(window);
            window = NULL;
            return false;
        }

        buffer = SDL_CreateRGBSurface(0,64,32,32,0,0,0,0);

        if(buffer) {
            assert(buffer->pitch == 64*4);
        }

        //printf("lcd::Setting render draw color to black\n");
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        //printf("lcd::Clearing rendering output\n");
        SDL_RenderClear(renderer);
        //printf("lcd::Pushing video update to renderer\n");
        SDL_RenderPresent(renderer);
        //printf("lcd::constructor reached end\n");
        col[0] = SDL_MapRGB(buffer->format,0,0,0);
        col[1] = SDL_MapRGB(buffer->format,255,255,255);
        return true;
}

int main(int argc, char * argv[]) {

    if(!load_rom(argc, argv)) {
        fprintf(stderr, "Failed to load. Exiting.\n");
        return 1;
    }

    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER|SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "Unable to init SDL: %s", SDL_GetError());
        return 1;
    }

    if(!vid_init()) {
        fprintf(stderr, "Video failed to init.\n");
        return 1;
    }
                                //    0                1             2                3             4                5                6             7
    uint16_t key_settings[16] = {SDL_SCANCODE_X, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_A,
                                //    8                9             A                B             C                D                E             F
                                 SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_Z, SDL_SCANCODE_C, SDL_SCANCODE_4, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_V};
    for(int i=0;i<512;i++) {
        keymap[i] = 255;
    }

    for(int i=0;i<16;i++) {
        assert(keymap[key_settings[i]] == 255);
        keymap[key_settings[i]] = i;
    }

    for(int i=0;i<5*16;i++) {
        memory[i] = font[i];
    }

    SDL_AddTimer(16, timer_callback, NULL);

    srand(time(NULL));

    struct timeval start;
    gettimeofday(&start, NULL);

    while(!quit) {
        uint16_t instruction = read16(pc);
        printf("%04x %04x ", pc, instruction);
        pc+=2;
        uint8_t digit1 = instruction>>12;
        uint8_t digit2 = (instruction & DIGIT2)>>8;
        uint8_t digit3 = (instruction & DIGIT3)>>4;
        uint8_t digit4 = (instruction & DIGIT4);
        uint8_t digit34 = (instruction & (DIGIT3 | DIGIT4));
        uint16_t digit234 = (instruction & (DIGIT2 | DIGIT3 | DIGIT4));
        if(digit1 == 0) {
            switch(digit34) {
                case 0xe0:
                    printf("disp_clear()\n");
                    disp_clear();
                    break;
                case 0xee:
                    printf("return\n");
                    sp++;
                    pc = stack[sp];
                    break;
                default:
                    printf("Call to RCA 1802 program at address 0x%03x: Not implemented\n", digit234);
                    return 1;
            }
        }
        else if(digit1 == 1) {
            printf("goto 0x%03x\n", digit234);
            pc = digit234;
        }
        else if(digit1 == 2) {
            printf("call 0x%03x\n", digit234);
            stack[sp] = pc;
            sp--;
            pc = digit234;
        }
        else if(digit1 == 3) {
            printf("V%X == 0x%02x?\n", digit2, digit34);
            if(registers[digit2] == digit34) {
                pc+=2;
            }
        }
        else if(digit1 == 4) {
            printf("V%X != 0x%02x?\n", digit2, digit34);
            if(registers[digit2] != digit34) {
                pc+=2;
            }
        }
        else if(digit1 == 5) {
            printf("V%X == V%X?\n", digit2, digit3);
            if(registers[digit2] == registers[digit3]) {
                pc+=2;
            }
        }
        else if(digit1 == 6) {
            printf("V%X = 0x%02x\n", digit2, digit34);
            registers[digit2] = digit34;
        }
        else if(digit1 == 7) {
            printf("V%X += 0x%02x\n", digit2, digit34);
            registers[digit2] += digit34;
        }
        else if(digit1 == 8) {
            switch(digit4) {
                case 0:
                    printf("V%X = V%X\n", digit2, digit3);
                    registers[digit2] = registers[digit3];
                    break;
                case 1:
                    printf("V%X |= V%X\n", digit2, digit3);
                    registers[digit2] |= registers[digit3];
                    break;
                case 2:
                    printf("V%X &= V%X\n", digit2, digit3);
                    registers[digit2] &= registers[digit3];
                    break;
                case 3:
                    printf("V%X ^= V%X\n", digit2, digit3);
                    registers[digit2] ^= registers[digit3];
                    break;
                case 4:
                    printf("V%X += V%X (set carry?)\n", digit2, digit3);
                    if(uint16_t(registers[digit2]) + uint16_t(registers[digit3]) > 255) {
                        registers[0x0f] = 1;
                    }
                    else {
                        registers[0x0f] = 0;
                    }
                    registers[digit2] += registers[digit3];
                    break;
                case 5:
                    if(registers[digit2] < registers[digit3]) {
                        registers[0x0f] = 0;
                    }
                    else {
                        registers[0x0f] = 1;
                    }
                    printf("V%X -= V%X (set carry?)\n", digit2, digit3);
                    registers[digit2] = registers[digit3];
                    break;
                case 6:
                    printf("V%X >> 1\n", digit2);
                    registers[0x0f] = (registers[digit2] & 1);
                    registers[digit2] >>= 1;
                    break;
                case 7:
                    if(registers[digit3] < registers[digit2]) {
                        registers[0x0f] = 0;
                    }
                    else {
                        registers[0x0f] = 1;
                    }
                    printf("V%X = V%X - V%X\n", digit2, digit3, digit2);
                    registers[digit2] = registers[digit3] - registers[digit2];
                    break;
                case 0xe:
                    printf("V%X (0x%02x) << 1\n", digit2, registers[digit2]);
                    registers[0x0f] = ((registers[digit2] & 0x80)>>7);
                    registers[digit2] <<= 1;
                    break;
                default:
                    fprintf(stderr, "Unknown instruction \"%04x\".\n", instruction);
                    return 1;
            }
        }
        else if(digit1 == 9) {
            printf("V%X (0x%02x) != V%X (0x%02x)?\n", digit2, registers[digit2], digit3, registers[digit3]);
            if(registers[digit2] != registers[digit3]) {
                pc+=2;
            }
        }
        else if(digit1 == 0xa) {
            printf("I = 0x%03x\n", digit234);
            I = digit234;
        }
        else if(digit1 == 0xb) {
            printf("Jump V0 + 0x%03x\n", digit234);
            pc = registers[0] + digit234;
        }
        else if(digit1 == 0xc) {
            uint8_t rval = (rand() % 256);
            printf("V%X = rand() (0x%02x) & 0x%02x = 0x%02x\n", digit2, rval, digit34, (rval & digit34));
            registers[digit2] = (rval & digit34);
        }
        else if(digit1 == 0xd) {
            printf("draw(%d, %d, %d)\n", digit2, digit3, digit4);
            draw(digit2, digit3, digit4);
        }
        else if(digit1 == 0xe) {
            switch(digit34) {
                case 0xa1:
                    printf("Key V%X (0x%x) not pressed?\n", digit2, registers[digit2]);
                    if(!keys[registers[digit2]]) {
                        pc+=2;
                    }
                    break;
                case 0x9e:
                    printf("Key V%X (0x%x) pressed?\n", digit2, registers[digit2]);
                    if(keys[registers[digit2]]) {
                        pc+=2;
                    }
                    break;
                default:
                    fprintf(stderr, "Unknown instruction \"%04x\".\n", instruction);
                    return 1;
            }
        }
        else if(digit1 == 0xf) {
            switch(digit34) {
                case 0x07:
                    printf("V%X = timer (0x%02x)\n", digit2, delay_timer);
                    registers[digit2] = delay_timer;
                    break;
                case 0x18:
                    printf("Sound = V%X (0x%02x)\n", digit2, registers[digit2]);
                    sound_timer = registers[digit2];
                    break;
                case 0x15:
                    printf("timer = V%X\n (0x%02x)\n", digit2, registers[digit2]);
                    delay_timer = registers[digit2];
                    break;
                case 0x1e:
                    printf("I (0x%02x) += V%X (0x%02x)\n", I, digit2, registers[digit2]);
                    I+=registers[digit2];
                    break;
                case 0x33:
                    printf("I = BCD(V%X) (%d)\n", digit2, registers[digit2]);
                    memory[I] = registers[digit2] / 100;
                    memory[I+1] = (registers[digit2] % 100) / 10;
                    memory[I+2] = (registers[digit2] % 10);
                    break;
                case 0x65:
                    printf("Read V0 to V%X from memory at I\n", digit2);
                    for(int i=0;i<digit2;i++) {
                        registers[i] = memory[I+i];
                    }
                    break;
                case 0x55:
                    printf("Write V0 to V%X to memory at I\n", digit2);
                    for(int i=0;i<digit2;i++) {
                        memory[I+i] = registers[i];
                    }
                    break;
                case 0x29:
                    printf("I = addr(font %X)\n", digit2);
                    I = digit2 * 5;
                    break;
                case 0x0a:
                default:
                    fprintf(stderr, "Unknown instruction \"%04x\".\n", instruction);
                    return 1;
            }
        }
        else {
            fprintf(stderr, "Unknown instruction \"%04x\".\n", instruction);
            return 1;
        }
    }
    return 0;
}
