/* <dirent.h> on the pad: folders of the filesystem (libwup's sys.c, on littlefs) */
#pragma once
#include <sys/types.h>
#define DT_UNKNOWN 0
#define DT_DIR     4
#define DT_REG     8
struct dirent { ino_t d_ino; unsigned char d_type; char d_name[256]; };
typedef struct wup__dir DIR;
