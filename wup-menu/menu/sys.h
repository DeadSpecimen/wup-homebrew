#pragma once
typedef struct { int ac, charging, headphones, level, percent, raw; } power_t;
int uic_query(int cmd);        /* one-byte UIC query, -1..-3 = driver not ready / busy */
void sys_power(power_t *p);    /* charge state (input report byte 4) + level (UIC 0x10, 0..6) */
