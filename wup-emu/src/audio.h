#pragma once
#include <stdint.h>
void audio_init(void);
void audio_tick(int cycles);
void audio_fill(int16_t *out, int frames);
int audio_backlog(void);
void audio_discard(void);
