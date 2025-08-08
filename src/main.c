
#include "chip8.h"



int main() {
    chip8_init();
    chip8_load_rom("G:/MiscProjects/Chip-8/roms/Pong (1 player).ch8");

    while(1) {
        chip8_update();
    }
        

    return 0;
}