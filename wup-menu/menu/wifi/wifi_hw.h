/* wifi_hw: the GamePad's radio through the stock firmware's driver (see wifi_hw.c) */
#pragma once
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef struct { char ssid[33]; u8 ssid_len, bssid[6]; int channel, rssi, secured; } wifi_net;
extern void (*wifi_wait)(int ms);                 /* how to wait (default: the firmware's task delay) */
int wifi_patch_radio(void);                       /* >= 0: sites changed; < 0: error */
int wifi_bringup(const char *country);            /* 0 = interface up */
int wifi_scan(wifi_net *out, int max);            /* 5 GHz networks, strongest first; < 0 = error */
int wifi_join(const wifi_net *net, const u8 *pmk);/* pmk = NULL for an open network */
int wifi_associated(const wifi_net *net);
int wifi_leave(void);                             /* off the network it is on */
int wifi_rssi(void);
int wifi_mac(u8 *mac);
int wifi_chip_read(u32 addr, void *out, u32 n);
void wpa_pmk(const char *pass, const u8 *ssid, u32 ssid_len, u8 out[32]);   /* PBKDF2-HMAC-SHA1, 4096 */
