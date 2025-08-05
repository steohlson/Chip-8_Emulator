
#include "chip8.h"
#include <SDL3/SDL.h>
#include <string.h>

//current opcode
uint16_t opcode;

//main memory
uint8_t memory[4096];

//Memory map
#define MM_INTERPRETER 0x000
#define MM_FONT 0x050
#define MM_PROGRAM 0x200
#define MM_RAM 0x600

//8-bit registers V0 through VE, VF is special for carry flag
uint8_t V[16];

//Index register
uint16_t I;

//Program Counter
uint16_t pc;

//64 x 32 display
uint8_t display[64 * 32];

//Timers
uint8_t delay_timer;
uint8_t sound_timer;

//Stack
uint8_t stack[16];
//Stack pointer
uint8_t sp;

uint8_t keypad[16];

//Flag denoting if the display should be updated
uint8_t draw_flag;


uint8_t chip8_fontset[80] =
{ 
  0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
  0x20, 0x60, 0x20, 0x20, 0x70, // 1
  0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
  0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
  0x90, 0x90, 0xF0, 0x10, 0x10, // 4
  0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
  0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
  0xF0, 0x10, 0x20, 0x40, 0x40, // 7
  0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
  0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
  0xF0, 0x90, 0xF0, 0x90, 0x90, // A
  0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
  0xF0, 0x80, 0x80, 0x80, 0xF0, // C
  0xE0, 0x90, 0x90, 0x90, 0xE0, // D
  0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
  0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

#define Vx (V[(opcode & 0x0F00) >> 8])
#define Vy (V[(opcode & 0x00F0) >> 4])

void chip8_init() {
    pc = MM_PROGRAM;
    opcode = 0;
    I = 0;
    sp = 0;
    draw_flag = 0;

    for(int i=0; i < sizeof(display) / sizeof(display[0]); i++)   { display[i] = 0; }
    for(int i=0; i < sizeof(stack) / sizeof(stack[0]); i++)       { stack[i] = 0; }
    for(int i=0; i < sizeof(V) / sizeof(V[0]); i++)               { V[i] = 0; }
    for(int i=0; i < sizeof(keypad) / sizeof(keypad[0]); i++)     { keypad[i] = 0; }
    for(int i=0; i < sizeof(memory) / sizeof(memory[0]); i++)     { memory[i] = 0; }

    for(int i=0; i < sizeof(chip8_fontset) / sizeof(chip8_fontset[0]); i++) {
        memory[MM_FONT + i] = chip8_fontset[i];
    }

    display_init();

}

void chip8_load_rom(const char *filename) {
    //char path[80] = "";
    //strcpy(path, "./roms/");
    //strcat(path, *filename);
    FILE *fp = fopen("../roms/Pong (1 player).ch8", "rb");
    
    if (fp == NULL) {
        perror("Failed to open ROM");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    fread(&memory[MM_PROGRAM], 1, file_size, fp);

    fclose(fp);

}

void chip8_update() {
    opcode = memory[pc] << 8 | memory[pc + 1];
    

    switch(opcode & 0xF000) {


        case 0x1000: // 1NNN JP addr - jump to address
            pc = opcode & 0x0FFF;
        break;

        case 0x2000: // 2NNN CALL addr - call subroutine at nnn
            sp++;
            stack[sp] = pc;
            pc = opcode & 0x0FFF;
        break;

        case 0x3000: //3xkk SE Vx, byte - skip next instruction if Vx = kk
            if(Vx == (opcode & 0x00FF)) {
                pc += 2;
            }
            pc+=2;
        break;

        case 0x4000: //4xkk SNE Vx, byte - skip next instruction if Vx != kk
            if(Vx != (opcode & 0x00FF)) {
                pc += 2;
            }
            pc += 2;
        break;

        case 0x5000: //5xy0 SE Vx, Vy - skip if Vx = Vy
            if(Vx == Vy) {
                pc += 2;
            }
            pc += 2;
        break;

        case 0x6000: //6xkk LD Vx, byte - Set Vx = kk
            V[((opcode & 0x0F00) >> 16)] = opcode & 0x00FF;
            pc += 2;
        break;
        
        case 0x7000: //7xkk ADD Vx, byte
            V[((opcode & 0x0F00) >> 16)] += opcode & 0x00FF;
            pc += 2;
        break;
        
        case 0x8000: 
            switch(opcode & 0x000F) {
                case 0x0000: //8xy0 LD Vx, Vy - Set Vx = Vy
                    Vx = Vy;
                    pc += 2;
                break;

                case 0x0001: //8xy1 OR Vx, Vy
                    Vx |= Vy;
                    pc += 2;
                break;

                case 0x0002: //8xy2 AND Vx, Vy
                    Vx &= Vy;
                    pc += 2;
                break;

                case 0x0003: //8xy3 XOR Vx, Vy
                    Vx ^= Vy;
                    pc += 2;
                break;

                case 0x0004: //8xy4 ADD Vx, Vy
                    uint16_t result = (uint16_t)Vx + (uint16_t)Vy;
                    if (result > 255) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    Vx = result;
                    
                    pc += 2;
                break;

                case 0x0005: //8xy5 SUB Vx, Vy
                    if (Vx > Vy) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }

                    Vx -= Vy;
                    pc += 2;
                break;

                case 0x0006: //8xy6 SHR Vx {, Vy} - divide by 2
                    V[0xF] = Vx & 0x01;
                    Vx >>= 1;
                    pc += 2;
                break;

                case 0x0007: //8xy7 SUBN Vx, Vy - Vx = Vy-Vx
                    if (Vx < Vy) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }

                    V[(opcode & 0x0F00) >> 8] -= V[(opcode & 0x00F0) >> 16];
                    pc += 2;
                break;

                case 0x000E: //8xyE SHL Vx {, Vy} - mult by two
                    V[0xF] = (Vx & 0x80) >> 7;
                    Vx <<= 1;
                    pc += 2;
                break;

                default:
                    perror("Unkown opcode: " + opcode);
            }


        break;

            

        case 0x9000: //9xy0 SNE Vx, Vy - skip next instruction if Vx != Vy
            if (Vx != Vy) {
                pc += 2;
            }
            pc += 2;
        break;


        case 0xA000: // Annn LD I, addr, Sets I to the address NNN
            I = opcode & 0x0FFF;
            pc += 2;
        break;

        case 0xB000: // Bnnn JP V0, addr - jump to location nnn + V0
            pc = (opcode & 0x0FFF) + V[0];
        break;

        case 0xC000: // Cxkk RND Vx, byte - set Vx = random byte and kk
            uint8_t rn = (uint8_t)(rand() % 256);
            Vx = rn & (opcode & 0x00FF);
            pc += 2;
        break;

        case 0xD000: // Dxyn DRW Vx, Vy, nibble - Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision
            //Draw sprite to screen
            uint8_t height = opcode & 0x000F;
            uint8_t line;

            V[0xF] = 0;
            for(int y=0; y < height; y++) {
                line = memory[I + y];
                for(int x=0; x < 8; x++) {
                    if((line & (0x80 >> x)) != 0) {
                        if(display[(Vx + x + ((Vy + y) * 64))] == 1) {
                            V[0xF] = 1;
                        }

                        display[(Vx + x + ((Vy + y) * 64))] ^= 1;
                    }
                }
            }

            draw_flag = 1;
            pc += 2;
        break;

        case 0xE000:
            switch (opcode & 0x000F) {
                case 0x000E: // Ex9E SKP Vx - skip next instruction if key with the value of Vx is pressed
                    //check keyboard
                    if(keypad[Vx] != 0) { //is pressed
                        pc += 2;
                    }
                    pc += 2;
                break;

                case 0x0001: // ExA1 SKNP Vx - Skip next instruction if key with value of Vx is not pressed
                    //check keyboard
                    if(keypad[Vx] == 0) { //is not pressed
                        pc += 2;
                    }
                    pc += 2;
                break;
                
                default:
                    perror("Unkown opcode: " + opcode);
            }
        break;

        case 0xF000:
            switch (opcode & 0x000F) {
                case 0x0007: // Fx07 LD Vx, DT - Set Vx = delay timer value
                    Vx = delay_timer;
                    pc += 2;
                break;

                case 0x000A: // Fx0A LD Vx, K - wait for key press, store the value of the key to Vx
                    //Vx = keypress;
                    
                    for (int i=0; i < sizeof(keypad) / sizeof(keypad[0]); i++) {
                        if( keypad[i] != 0) {
                            Vx = i;
                            pc += 2;
                            break;
                        }
                    }
                break;


                case 0x0008: // Fx18 LD ST, Vx - set sound timer = Vx
                    sound_timer = Vx;
                    pc += 2;
                break;

                case 0x000E: // Fx1E Add I, Vx - set I = I + Vx
                    I += Vx;
                    pc += 2;
                break;

                case 0x0009: // Fx29 LD F, Vx - set I = location of sprite for digit Vx
                    I = MM_FONT + 5 * Vx;
                    pc += 2;
                break;

                case 0x0003: // Fx33 LD B, Vx - store BCD representation of Vx in memory locations I, I+1, and I+2
                    memory[I] =  Vx / 100;
                    memory[I+1] = (Vx % 100) / 10;
                    memory[I+2] = Vx % 10;
                
                    pc += 2;
                break;

                case 0x0005:
                    switch (opcode & 0x00F0) {
                        case 0x0010: // Fx15 LD DT, Vx - set delay timer = Vx
                            delay_timer = Vx;
                            pc += 2;
                        break;
                        
                        case 0x0050: // Fx55 LD [I], Vx - Store registers V0 through Vx in memory starting at location I
                            for(int j=0; j <= Vx; j++) {
                                memory[I + j] = V[j];
                            }
                        break;

                        case 0x0060: // Fx65 LD Vx, [I] - Read registers V0 through Vx from memory starting at location I.
                            for(int j=0; j <= Vx; j++) {
                                V[j] = memory[I + j];
                            }
                        break;

                        default:
                            perror("Unkown opcode: " + opcode);

                    }

                break;

                default:
                    perror("Unkown opcode: " + opcode);
            }

        case 0x0000: //Other opcode
            switch(opcode & 0x000F){
                case 0x0000: // Clear screen
                    //clear screen
                    for(int i=0; i < sizeof(display) / sizeof(display[0]); i++)   { display[i] = 0; }
                    pc += 2;
                break;

                case 0x000E: // RET, Returns from subroutine
                    pc = stack[sp];
                    sp--;
                break;

                default: 
                    perror("Unkown opcode: " + opcode);
            }


        default:
            perror("Unkown opcode: " + opcode);
    }


    if(draw_flag) {
        display_draw();
    }
    
    if(delay_timer > 0) {delay_timer--;}
    if(sound_timer > 0) {sound_timer--;}

}




////////////////// DISPLAY CODE //////////////////

#define WIDTH 64
#define HEIGHT 32
#define SCALE 10

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;


void display_init() {
    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        perror("SDL_Init Error");
        return;
    }

    window = SDL_CreateWindow("Chip-8", WIDTH * SCALE, HEIGHT * SCALE, 0);
    renderer = SDL_CreateRenderer(window, NULL);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

}

void display_draw() {
    uint32_t pixels[WIDTH * HEIGHT];
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        pixels[i] = display[i] ? 0xFFFFFFFF : 0xFF000000;
    }

    SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    SDL_Delay(100);
}