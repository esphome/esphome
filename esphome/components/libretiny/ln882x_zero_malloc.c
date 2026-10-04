/*
 * LN882x: heap_5 returns NULL for a zero-size request, and the SDK's
 * malloc-failed hook ends in LN_ASSERT(0), which spins until the watchdog
 * resets the chip. LibreTiny's scan handler asks for zero entries when a scan
 * finds no networks. Linked with -Wl,--wrap=pvPortMalloc so every allocation
 * comes through here; drop it once LibreTiny no longer allocates zero entries.
 */

#ifdef USE_LN882X

#include <stddef.h>

void *__real_pvPortMalloc(size_t size);  // NOLINT(readability-identifier-naming)

void *__wrap_pvPortMalloc(size_t size) {  // NOLINT(readability-identifier-naming)
  return __real_pvPortMalloc(size == 0 ? 1 : size);
}

#endif  // USE_LN882X
