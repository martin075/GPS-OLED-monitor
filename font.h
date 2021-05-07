/*
 *  font.h
 *  i2c
 *
 *  Created by Michael Kohler on 13.09.18.
 *  Copyright 2018 Skie-Systems. All rights reserved.
 *
 */
#ifndef _font_h_
#define _font_h_
#include <avr/pgmspace.h>

//5x7 font
extern const char ssd1306oled_font[][6] PROGMEM;

//5x7 font
//extern const char ssd1306oled_font[][5] PROGMEM;

//7x8 font
//extern const char ssd1306oled_font[][7] PROGMEM;

//5x7 special
extern const char special_char[][2] PROGMEM;
#endif
