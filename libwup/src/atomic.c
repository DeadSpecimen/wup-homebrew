/* The atomic operations the compiler calls for on this CPU (ARMv5 has no instructions for them): __atomic_* on 1, 2
 * and 4 byte values and the generic ones, done with interrupts off. There is one core, so that is all it takes. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "wup.h"

#define SIZED(n, T) \
    T __atomic_load_##n(const volatile T *p, int o) { (void)o; return *p; } \
    void __atomic_store_##n(volatile T *p, T v, int o) { (void)o; *p = v; } \
    T __atomic_exchange_##n(volatile T *p, T v, int o) { (void)o; unsigned st = wup_irq_off(); T old = *p; *p = v; wup_irq_restore(st); return old; } \
    bool __atomic_compare_exchange_##n(volatile T *p, T *exp, T des, bool weak, int ok, int fail) { \
        (void)weak; (void)ok; (void)fail; unsigned st = wup_irq_off(); T old = *p; bool same = old == *exp; \
        if (same) *p = des; else *exp = old; wup_irq_restore(st); return same; } \
    T __atomic_fetch_add_##n(volatile T *p, T v, int o) { (void)o; unsigned st = wup_irq_off(); T old = *p; *p = (T)(old + v); wup_irq_restore(st); return old; } \
    T __atomic_fetch_sub_##n(volatile T *p, T v, int o) { (void)o; unsigned st = wup_irq_off(); T old = *p; *p = (T)(old - v); wup_irq_restore(st); return old; } \
    T __atomic_fetch_and_##n(volatile T *p, T v, int o) { (void)o; unsigned st = wup_irq_off(); T old = *p; *p = (T)(old & v); wup_irq_restore(st); return old; } \
    T __atomic_fetch_or_##n(volatile T *p, T v, int o) { (void)o; unsigned st = wup_irq_off(); T old = *p; *p = (T)(old | v); wup_irq_restore(st); return old; } \
    T __atomic_fetch_xor_##n(volatile T *p, T v, int o) { (void)o; unsigned st = wup_irq_off(); T old = *p; *p = (T)(old ^ v); wup_irq_restore(st); return old; }
SIZED(1, uint8_t)
SIZED(2, uint16_t)
SIZED(4, uint32_t)

/* any size (structures); (the names are the compiler's own builtins in C, so they are given as assembler names) */
void any_load(unsigned n, const volatile void *p, void *out, int o) __asm__("__atomic_load");
void any_store(unsigned n, volatile void *p, const void *v, int o) __asm__("__atomic_store");
void any_exchange(unsigned n, volatile void *p, const void *v, void *old, int o) __asm__("__atomic_exchange");
bool any_compare_exchange(unsigned n, volatile void *p, void *exp, const void *des, int ok, int fail) __asm__("__atomic_compare_exchange");
void any_load(unsigned n, const volatile void *p, void *out, int o) { (void)o; unsigned st = wup_irq_off(); memcpy(out, (const void *)p, n); wup_irq_restore(st); }
void any_store(unsigned n, volatile void *p, const void *v, int o) { (void)o; unsigned st = wup_irq_off(); memcpy((void *)p, v, n); wup_irq_restore(st); }
void any_exchange(unsigned n, volatile void *p, const void *v, void *old, int o) {
    (void)o; unsigned st = wup_irq_off(); memcpy(old, (const void *)p, n); memcpy((void *)p, v, n); wup_irq_restore(st);
}
bool any_compare_exchange(unsigned n, volatile void *p, void *exp, const void *des, int ok, int fail) {
    (void)ok; (void)fail; unsigned st = wup_irq_off();
    bool same = !memcmp((const void *)p, exp, n);
    if (same) memcpy((void *)p, des, n); else memcpy(exp, (const void *)p, n);
    wup_irq_restore(st);
    return same;
}
