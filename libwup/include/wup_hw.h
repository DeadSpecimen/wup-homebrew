/* wup_hw: the GamePad's screen brightness, rumble motor and microphone, through the stock firmware (LVC rev 7677).
 * Shared by the menu and libwup. Found and measured on the real pad in wupstream (its pad/stream/stream.c). */
#pragma once
#include <stdint.h>

/* the backlight, 0..100 % of its PWM range (0: dark). The console uses only five levels of the pad's own table (its
   power chip's EEPROM, a copy at 0xbdc34: {PWM level, frequency} pairs 21 45 77 122 183 255); the driver takes any. */
void hw_backlight(int percent);          /* 0..100 */
int hw_backlight_percent(int level);     /* one of the table's levels (1..6) as a percentage, to the nearest 10 */
int hw_backlight_default(void);          /* the level Nintendo's mode uses (the EEPROM's) as a percentage, or -1 if unknown */
void hw_screen_power(int on);            /* the backlight off (0) and on (1), as the firmware does for the console */

/* the rumble motor: a GPIO pin (the firmware's vibrate module, LVC 0x22146) */
void hw_rumble(int on);

/* the microphone: 16-bit mono, 48 kHz, from the firmware's capture ring (drvacapt), read without its interrupt */
void hw_mic(int on);
int hw_mic_read(int16_t *out, int max);  /* samples recorded since the last read (up to max) */
