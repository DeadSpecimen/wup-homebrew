#pragma once
/* ftpd: file sharing for the pad's filesystem over FTP (port 21, passive mode). Any FTP client: FileZilla,
 * Cyberduck, Windows File Explorer (ftp://<pad IP>), macOS Finder (Go > Connect to Server, read-only). */
#ifndef FTP_PORT
#define FTP_PORT 21
#endif
#ifndef FTP_PASV_PORT
#define FTP_PASV_PORT 50100                /* data connections: one port per session, 50100.. */
#endif
#define FTP_USER "pad"                     /* the user name when a password is set */
int ftpd_open(void);                       /* listen; 0 = ok */
void ftpd_close(void);
void ftpd_step(void);                      /* from the menu loop (outside lwIP callbacks) */
int ftpd_apps_changed(void);               /* something under /apps changed since the last call */
int ftpd_busy(void);                       /* a transfer is running */
/* password: off (anyone, any user name), or on: user "pad" and the password, of which only
   MD5("pad:GamePad:<password>") is stored */
void ftpd_set_auth(int on, const unsigned char hash[16]);
void ftpd_hash(const char *password, unsigned char hash[16]);
