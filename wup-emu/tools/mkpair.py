#!/usr/bin/env python3
"""Write a GamePad pairing record into the emulated UIC EEPROM (uic_eeprom.bin).

Layout derived from the LVC firmware (uc_config.c: GetPairingInfo 0x3b1b8 / SetPairingInfo 0x3b2e2,
connect path 0x20904, pairing writer 0x20a22):
  EEPROM 0x1101 +0x00  SSID[32]
               +0x20  u8  (credential word 0, low byte)          -- inferred: channel
               +0x21  u8  0/1 mode flag                           -- inferred
               +0x22  u8  wpa_auth (0,1,4,0x80=WPA2-PSK)
               +0x23  u8  wsec code (0 none,1 WEP,2 TKIP,3 AES)
               +0x24  u8[64] key
               +0x64  u8  key length (<= 64)
               +0x65  u8[5] console info from Wi-Fi driver state (0xbc5b4+6..+10); [0x68] must be 1..3
               +0x6a  u16 CRC16 (poly 0x8408, init 0xffff, LE) over +0x00..+0x69
  EEPROM 0x116d u8[16] + CRC16   -- read by 0x3b3c2; purpose unknown (inferred: console-specific key/id)
  EEPROM 0x117f u8[6]  + CRC16   -- read by 0x3b4b8; must be non-zero, used as a MAC address (console MAC)
  EEPROM 0x1187 u8 state (low 2 bits) + CRC16 -- 0 = not paired; 1 = paired (requires the two records above)
  EEPROM 0x11fd u8 3 (= pairing area valid) + CRC16 over that byte
"""
import sys, os, secrets

def crc16(b):
    c = 0xffff
    for x in b:
        c ^= x
        for _ in range(8):
            c = (c >> 1) ^ 0x8408 if c & 1 else c >> 1
    return c

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 'firmware', 'uic_eeprom.bin')
    mac = (sys.argv[2] if len(sys.argv) > 2 else '02:00:00:00:00:10').replace(':', '').lower()
    key = sys.argv[3] if len(sys.argv) > 3 else secrets.token_hex(32)
    chan = int(sys.argv[4]) if len(sys.argv) > 4 else 36
    state = int(sys.argv[5]) if len(sys.argv) > 5 else 1
    ee = bytearray(open(path, 'rb').read())
    rec = bytearray(0x6a)
    ssid = ('WiiU' + mac).encode()
    rec[0:len(ssid)] = ssid
    rec[0x20] = chan & 0xff
    rec[0x21] = 1
    rec[0x22] = 0x80          # WPA2-PSK
    rec[0x23] = 3             # AES
    kb = key.encode()[:64]
    rec[0x24:0x24 + len(kb)] = kb
    rec[0x64] = len(kb)
    rec[0x68] = 1
    c = crc16(rec)
    base = 0x101              # EEPROM 0x1101 -> file offset (file starts at 0x1000)
    ee[base:base + 0x6a] = rec
    ee[base + 0x6a] = c & 0xff; ee[base + 0x6b] = c >> 8
    def put(off, data):
        data = bytes(data); cc = crc16(data)
        ee[off:off + len(data)] = data; ee[off + len(data)] = cc & 0xff; ee[off + len(data) + 1] = cc >> 8
    put(0x16d, bytes.fromhex(secrets.token_hex(16)))
    put(0x17f, bytes.fromhex(mac))
    put(0x187, [state & 3])
    f = crc16([3])
    ee[0x1fd] = 3; ee[0x1fe] = f & 0xff; ee[0x1ff] = f >> 8
    open(path, 'wb').write(ee)
    print('paired: ssid=%s chan=%d state=%d key=%s...' % (ssid.decode(), chan, state, key[:8]))

main()
