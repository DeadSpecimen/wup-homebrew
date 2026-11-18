#pragma once
#include <stdint.h>
typedef int KPADChan;
typedef int KPADError;
enum { KPAD_ERROR_OK = 0 };
typedef struct { uint32_t trigger; uint8_t extensionType; struct { uint32_t trigger; } classic; } KPADStatus;
static inline int KPADReadEx(KPADChan, KPADStatus *, int, KPADError *) { return 0; }
static inline void KPADInit(void) {}
