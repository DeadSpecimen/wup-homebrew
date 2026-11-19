/* tone: the Homebrew Menu's sounds, made with integer maths (no samples stored, no maths library): each note a sine (a
 * two-term recurrence) with a 5 ms rise and an exponential fall. Shared by the menu (menu/sound.c) and its apps
 * (libwup's kit). The click: 8 ms at 1.6 kHz and its octave. Two notes, 0.16 s, for what appears over the screen: a
 * confirmation (E6 then G#6, rising) and an alert (B5 then E5, falling, quieter). */
#pragma once
#include <stdint.h>

typedef struct { int64_t c, y1, y2; int64_t env, d; int start, rise; } voice_t;
/* 2 cos(w) in Q30 and sin(w) in Q15 for w = 2 pi f / 48 kHz (computed once on a computer) */
static inline void tone(voice_t *v, int64_t c, int32_t s, int amp, int start, int64_t d) {
    v->c = c; v->y2 = 0; v->y1 = (int64_t)amp * s;  /* y in Q15 x amplitude: sin(0), sin(w) */
    v->env = (int64_t)1 << 30; v->d = d; v->start = start; v->rise = 240;
}
static inline int16_t render(voice_t *v, int n, int t) {   /* t: the sample number of the whole sound */
    int64_t mix = 0;
    for (int i = 0; i < n; i++) {
        voice_t *p = &v[i];
        if (t < p->start) continue;
        int64_t y = ((p->c * p->y1) >> 30) - p->y2;
        p->y2 = p->y1; p->y1 = y;
        int64_t a = (p->y2 * p->env) >> 30;
        int k = t - p->start; if (k < p->rise) a = a * k / p->rise;
        mix += a >> 15;
        p->env = (p->env * p->d) >> 30;
    }
    return (int16_t)(mix > 32000 ? 32000 : mix < -32000 ? -32000 : mix);
}

#define CLICK 400                                   /* 8 ms */
static inline void make_click(int16_t out[2 * CLICK]) {
    voice_t v[2];
    tone(&v[0], 2100555978, 6813, 9000, 0, 1062615066);       /* 1.6 kHz, gone in 8 ms */
    tone(&v[1], 1961823932, 13328, 3000, 0, 1062615066);      /* and its octave */
    v[0].rise = v[1].rise = 24;
    for (int i = 0; i < CLICK; i++) out[2 * i] = out[2 * i + 1] = render(v, 2, i);
}

/* the two notes, made as they are queued, 240 frames at a time (no buffer kept: 60 KB would be the App Store's whole
   margin of memory). queue: wup_audio_queue */
#define TWO_NOTES 7680                              /* 0.16 s */
static inline void play_two_notes(int alert, int (*queue)(const int16_t *lr, int frames)) {
    voice_t v[2]; int16_t buf[2 * 240];
    if (alert) { tone(&v[0], 2129557746, 4225, 3200, 0, 1073369229); tone(&v[1], 2139492403, 2824, 3200, 80 * 48, 1073369229); }     /* B5, E5: each gone in 60 ms */
    else { tone(&v[0], 2115578141, 5627, 3600, 0, 1073369229); tone(&v[1], 2096910565, 7069, 3600, 70 * 48, 1073369229); }           /* E6, G#6 */
    for (int t = 0; t < TWO_NOTES; t += 240) {
        for (int i = 0; i < 240; i++) {
            int16_t y = render(v, 2, t + i);
            if (t + i > TWO_NOTES - 480) y = (int16_t)(y * (TWO_NOTES - t - i) / 480);
            buf[2 * i] = buf[2 * i + 1] = y;
        }
        if (queue(buf, 240) < 240) break;           /* (no room: the rest is dropped) */
    }
}

/* an app opens: three notes rising, quick (E5, B5, E6: the chime's first three), 0.24 s; made as they are queued */
#define LAUNCH 11520
static inline void play_launch(int (*queue)(const int16_t *lr, int frames)) {
    voice_t v[3]; int16_t buf[2 * 240];
    tone(&v[0], 2139492403, 2824, 3000, 0, 1073369229); tone(&v[1], 2129557746, 4225, 3000, 60 * 48, 1073369229); tone(&v[2], 2115578141, 5627, 3200, 120 * 48, 1073492520);
    for (int t = 0; t < LAUNCH; t += 240) {
        for (int i = 0; i < 240; i++) {
            int16_t y = render(v, 3, t + i);
            if (t + i > LAUNCH - 960) y = (int16_t)(y * (LAUNCH - t - i) / 960);
            buf[2 * i] = buf[2 * i + 1] = y;
        }
        if (queue(buf, 240) < 240) break;
    }
}
