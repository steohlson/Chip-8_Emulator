
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "SDL3/SDL.h"


void chip8_init();

void chip8_load_rom(const char *filename);

void chip8_update();

void display_init();

void display_draw();
