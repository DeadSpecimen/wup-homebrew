#pragma once
#include <stdint.h>
#define LCD_W 854
#define LCD_H 480
void lcd_init(void);
void lcd_render(uint32_t *out);
