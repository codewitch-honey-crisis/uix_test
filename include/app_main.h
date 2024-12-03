#pragma once
#define SCREEN_RGB565
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480
#include <stdint.h>
extern void flush_bitmap(int x1, int y1, int w, int h, const void* bmp);
extern bool read_mouse(int* out_x, int* out_y);
extern void setup();
extern void loop();
extern uint32_t millis();
extern uint32_t micros();
extern void delay(uint32_t us);
extern void delayMicroseconds(uint32_t us);