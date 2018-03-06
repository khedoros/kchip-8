#include<stdlib.h>
#include<time.h>
#include<stdint.h>
#include<stdio.h>

#define DIGIT1 0xF000
#define DIGIT2 0x0F00
#define DIGIT3 0x00F0
#define DIGIT4 0x000F

uint8_t memory[4096] = {0};
bool screen[64][32] = {{0}};
uint8_t registers[16] = {0};

uint16_t read16(uint16_t addr) {
    return uint16_t(256 * uint16_t(memory[addr]) + uint16_t(memory[addr+1]));
}

void disp_clear() {
    for(int x=0;x<64;x++) {
        for(int y=0;y<32;y++) {
            screen[x][y] = 0;
        }
    }
}

void draw(uint8_t x, uint8_t y, uint8_t h) {

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

int main(int argc, char * argv[]) {

    if(!load_rom(argc, argv)) {
        fprintf(stderr, "Failed to load. Exiting.\n");
        return 1;
    }

    srand(time(NULL));

    uint16_t pc = 0x200; //"Program Counter", which tracks where in the program we are
    uint16_t I = 0x00;   //"I", which stores a memory address.
    while(1==1) {
        uint16_t instruction = read16(pc);
        printf("%04x %04x ", pc, instruction);
        pc+=2;
        uint8_t digit1 = instruction>>12;
        uint8_t digit2 = (instruction & DIGIT2)>>8;
        uint8_t digit3 = (instruction & DIGIT3)>>4;
        uint8_t digit4 = (instruction & DIGIT4);
        uint8_t digit34 = (instruction & (DIGIT3 | DIGIT4));
        uint16_t digit234 = (instruction & (DIGIT2 | DIGIT3 | DIGIT4));
        if(instruction == 0x00e0) {
            printf("disp_clear()\n");
            disp_clear();
        }
        else if(digit1 == 1) {
            printf("goto 0x%03x\n", digit234);
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
                    printf("V%X << 1\n", digit2);
                    registers[0x0f] = ((registers[digit2] & 0x80)>>7);
                    registers[digit2] <<= 1;
                    break;
                default:
                    fprintf(stderr, "Unknown instruction \"%04x\".\n", instruction);
                    return 1;
            }
        }
        else if(digit1 == 0xa) {
            printf("I = 0x%03x\n", digit234);
            I = digit234;
        }
        else if(digit1 == 0xc) {
            printf("V%X = rand() & 0x%02x\n", digit2, digit34);
            registers[digit2] = ((rand() % 256) & digit34);
        }
        else if(digit1 == 0xd) {
            printf("draw(%d, %d, %d)\n", digit2, digit3, digit4);
            draw(digit2, digit3, digit4);
        }
        else {
            fprintf(stderr, "Unknown instruction \"%04x\".\n", instruction);
            return 1;
        }
    }
    return 0;
}
