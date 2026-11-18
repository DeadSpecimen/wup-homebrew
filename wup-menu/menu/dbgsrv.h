#pragma once
#include <stdint.h>
int dbgsrv_open(void);          /* listen on TCP 28802 */
void dbgsrv_close(void);
void dbgsrv_step(void);         /* run queued commands: call from the menu loop, outside lwIP callbacks */
int dbgsrv_active(void);
uint32_t dbgsrv_commands(void);
