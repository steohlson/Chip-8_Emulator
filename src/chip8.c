
#include "chip8.h"
#include <string.h>

#define CLK_HZ 500
#define CLK_PER (1000 / CLK_HZ)

//Screen dimensions
#define WIDTH 64
#define HEIGHT 32
#define SCALE 10

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
uint16_t stack[16];
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

    srand((unsigned int)SDL_GetTicks());
    

}


void chip8_load_rom(const char *filename) {
    FILE *fp = fopen(filename, "rb");
    
    if (fp == NULL) {
        SDL_Log("Failed to open ROM");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    fread(&memory[MM_PROGRAM], 1, file_size, fp);

    fclose(fp);

}

void chip8_update() {
    uint64_t start_time = SDL_GetTicks();

    SDL_Event event;
    if(SDL_PollEvent(&event)) {
        uint8_t keypad_index;
        switch (event.key.key)
            {
            case SDLK_1:
                keypad_index = 0x1;
                break;
            case SDLK_2:
                keypad_index = 0x2;
                break;
            case SDLK_3:
                keypad_index = 0x3;
                break;
            case SDLK_4:
                keypad_index = 0xc;
                break;
            case SDLK_Q:
                keypad_index = 0x4;
                break;
            case SDLK_W:
                keypad_index = 0x5;
                break;
            case SDLK_E:
                keypad_index = 0x6;
                break;
            case SDLK_R:
                keypad_index = 0xd;
                break;
            case SDLK_A:
                keypad_index = 0x7;
                break;
            case SDLK_S:
                keypad_index = 0x8;
                break;
            case SDLK_D:
                keypad_index = 0x9;
                break;
            case SDLK_F:
                keypad_index = 0xe;
                break;
            case SDLK_Z:
                keypad_index = 0xa;
                break;
            case SDLK_X:
                keypad_index = 0x0;
                break;
            case SDLK_C:
                keypad_index = 0xb;
                break;
            case SDLK_V:
                keypad_index = 0xf;
                break;
        
            }
        if(event.type == SDL_EVENT_KEY_DOWN) {
           keypad[keypad_index] = 1;
        } else if (event.type == SDL_EVENT_KEY_UP) {
            keypad[keypad_index] = 0;
        }


    }

    opcode = memory[pc] << 8 | memory[pc + 1];
    
    SDL_Log("0x%04X", opcode);

    switch(opcode & 0xF000) {


        case 0x1000: // 1NNN JP addr - jump to address
            pc = opcode & 0x0FFF;
        break;

        case 0x2000: // 2NNN CALL addr - call subroutine at nnn
            stack[sp++] = pc;
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
            Vx = opcode & 0x00FF;
            pc += 2;
        break;
        
        case 0x7000: //7xkk ADD Vx, byte
            Vx += opcode & 0x00FF;
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
                    V[0xF] = 0;
                    pc += 2;
                break;

                case 0x0002: //8xy2 AND Vx, Vy
                    Vx &= Vy;
                    V[0xF] = 0;
                    pc += 2;
                break;

                case 0x0003: //8xy3 XOR Vx, Vy
                    Vx ^= Vy;
                    V[0xF] = 0;
                    pc += 2;
                break;

                case 0x0004: //8xy4 ADD Vx, Vy
                    uint16_t result = (uint16_t)Vx + (uint16_t)Vy;
                    Vx = (uint8_t)result;
                    if (result > 255) {
                        V[0xF] = 1;
                    } else {
                        V[0xF] = 0;
                    }
                    
                    
                    pc += 2;
                break;
                uint8_t temp_vf = 0;
                case 0x0005: //8xy5 SUB Vx, Vy
                    
                    if (Vx >= Vy) {
                        temp_vf = 1;
                    } else {
                        temp_vf = 0;
                    }
                    
                    Vx -= Vy;
                    V[0xF] = temp_vf;
                    pc += 2;
                break;

                case 0x0006: //8xy6 SHR Vx {, Vy} - divide by 2
                    temp_vf = Vx & 0x01;
                    Vx >>= 1;
                    V[0xF] = temp_vf;
                    
                    pc += 2;
                break;

                case 0x0007: //8xy7 SUBN Vx, Vy - Vx = Vy-Vx
                    if (Vx <= Vy) {
                        temp_vf = 1;
                    } else {
                        temp_vf = 0;
                    }
                    Vx = Vy - Vx;
                    V[0xF] = temp_vf;
                    
                    pc += 2;
                break;

                case 0x000E: //8xyE SHL Vx {, Vy} - mult by two
                    temp_vf = (Vx & 0x80) >> 7;
                    Vx <<= 1;
                    V[0xF] = temp_vf;
                    
                    pc += 2;
                break;

                default:
                    SDL_Log("Unknown opcode: 0x%04X", opcode);
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
            uint8_t collision = 0;
            
            for(int y=0; y < height; y++) {
                line = memory[I + y];
                uint16_t py = Vy + y;
                if(py >= HEIGHT) break;
                for(int x=0; x < 8; x++) {
                    uint16_t px = Vx + x;
                    if(px >= WIDTH) continue;
                    if((line & (0x80 >> x)) != 0) {
                        
                        
                        if(display[(px + (py * WIDTH))] == 1) {
                            collision = 1;
                        }

                        display[(px + (py * WIDTH))] ^= 1;
                    }
                }
            }

            V[0xF] = collision;

            draw_flag = 1;
            pc += 2;
        break;

        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x009E: // Ex9E SKP Vx - skip next instruction if key with the value of Vx is pressed
                    //check keyboard
                    if(keypad[Vx] != 0) { //is pressed
                        pc += 2;
                    }
                    pc += 2;
                break;

                case 0x00A1: // ExA1 SKNP Vx - Skip next instruction if key with value of Vx is not pressed
                    //check keyboard
                    if(keypad[Vx] == 0) { //is not pressed
                        pc += 2;
                    }
                    pc += 2;
                break;
                
                default:
                    SDL_Log("Unknown opcode: 0x%04X", opcode);
            }
        break;

        case 0xF000:
            switch (opcode & 0x00FF) {
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


                case 0x0018: // Fx18 LD ST, Vx - set sound timer = Vx
                    sound_timer = Vx;
                    pc += 2;
                break;

                case 0x001E: // Fx1E Add I, Vx - set I = I + Vx
                    I += Vx;
                    pc += 2;
                break;

                case 0x0029: // Fx29 LD F, Vx - set I = location of sprite for digit Vx
                    I = MM_FONT + 5 * Vx;
                    pc += 2;
                break;

                case 0x0033: // Fx33 LD B, Vx - store BCD representation of Vx in memory locations I, I+1, and I+2
                    memory[I] =  Vx / 100;
                    memory[I+1] = (Vx % 100) / 10;
                    memory[I+2] = Vx % 10;
                
                    pc += 2;
                break;


                case 0x0015: // Fx15 LD DT, Vx - set delay timer = Vx
                    delay_timer = Vx;
                    pc += 2;
                break;
                
                case 0x0055: // Fx55 LD [I], Vx - Store registers V0 through Vx in memory starting at location I
                    for(int j=0; j <= (opcode & 0x0F00) >> 8; j++, I++) {
                        memory[I] = V[j];
                    }
                    pc += 2;
                break;

                case 0x0065: // Fx65 LD Vx, [I] - Read registers V0 through Vx from memory starting at location I.
                    for(int j=0; j <= (opcode & 0x0F00) >> 8; j++, I++) {
                        V[j] = memory[I];
                    }
                    pc += 2;
                break;


                default:
                    SDL_Log("Unknown opcode: 0x%04X", opcode);
            }
        break;


        case 0x0000: //Other opcode
            switch(opcode & 0x00FF){
                case 0x00E0: // Clear screen
                    //clear screen
                    for(int i=0; i < sizeof(display) / sizeof(display[0]); i++)   { display[i] = 0; }
                    pc += 2;
                break;

                case 0x00EE: // RET, Returns from subroutine
                    pc = stack[--sp] + 2; //+2 since otherwise it would run the call instruction again
                break;

                default: // 0x0nnn - ignored by modern interpreters
                    pc += 2;
            }


        default:
            SDL_Log("Unknown opcode: 0x%04X", opcode);
    }


    if(draw_flag) {
        display_draw();
    }
    
    //timers run at 60 hz
    static uint64_t prev_timer_tick = 0;
    uint64_t cur_timer_tick = SDL_GetTicks();
    if(cur_timer_tick - prev_timer_tick >= 1000 / 60){
        if(delay_timer > 0) {delay_timer--;}
        if(sound_timer > 0) {sound_timer--;}

        prev_timer_tick = cur_timer_tick;
    }
    



    uint64_t delta_time = SDL_GetTicks() - start_time;
    if(delta_time < CLK_PER) {
        SDL_Delay(CLK_PER - (uint32_t)delta_time);
    }

}




////////////////// DISPLAY CODE //////////////////



SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* texture;


void display_init() {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Log("SDL_Init Error: %s", SDL_GetError());


    
    window = SDL_CreateWindow("Chip-8", WIDTH * SCALE, HEIGHT * SCALE, 0);
    if (!window) {
        SDL_Log("SDL_CreateWindow Error: %s", SDL_GetError());
        exit(1);
    }
    renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer Error: %s", SDL_GetError());
        exit(1);
    }
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    if (!texture) {
        SDL_Log("SDL_CreateTexture Error: %s", SDL_GetError());
        exit(1);
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);


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
}