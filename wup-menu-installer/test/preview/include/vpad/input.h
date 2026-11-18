#pragma once
#include <stdint.h>
enum { VPAD_BUTTON_UP = 1, VPAD_BUTTON_DOWN = 2, VPAD_BUTTON_A = 4, VPAD_BUTTON_B = 8, VPAD_BUTTON_HOME = 16 };
enum { VPAD_CHAN_0 = 0 };
typedef int VPADReadError;
enum { VPAD_READ_SUCCESS = 0 };
typedef struct { uint32_t trigger; } VPADStatus;
static inline int VPADRead(int, VPADStatus *, int, VPADReadError *) { return 0; }
