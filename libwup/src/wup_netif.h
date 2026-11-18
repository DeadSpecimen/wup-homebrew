/* wup_netif: the GamePad's Wi-Fi as an lwIP interface inside an app (see wup_netif.c) */
#pragma once
#include <stdint.h>
typedef struct { uint32_t rx_frames, rx_dropped, tx_frames, tx_errors, tx_queue_full, tx_queue_max; } wup_netif_stats_t;
int wup_netif_start(const uint8_t *mac, uint32_t ip, uint32_t mask, uint32_t gw, uint32_t dns);  /* lwIP byte order */
void wup_netif_poll(void);                /* received frames into lwIP, timers */
void wup_netif_stop(void);
void wup_netif_on_tick(void (*fn)(void));   /* called every time the radio is served, with lwIP's lock held (libwup: the log) */
void wup_netif_tx_pacing(unsigned rate_x10, unsigned burst);   /* frames/ms x10, frames at once (0: keep) */                /* unhook: the radio's frames are dropped again */
int wup_netif_dhcp(void);                 /* started with address 0: ask for one */
void wup_netif_netinfo(uint32_t *ip, uint32_t *mask, uint32_t *gw);
int wup_netif_up(void);
uint32_t wup_netif_ip(void);
const wup_netif_stats_t *wup_netif_stats(void);
