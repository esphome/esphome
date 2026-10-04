/*
 * LN882x: never ask the FreeRTOS heap for zero bytes.
 *
 * The LN882H SDK builds FreeRTOS with configUSE_MALLOC_FAILED_HOOK, and its
 * hook (components/kernel/FreeRTOS/hooks.c) ends in LN_ASSERT(0), which spins
 * until the watchdog resets the chip. heap_5 returns NULL for a zero-size
 * request, so a harmless malloc(0) runs that hook. LibreTiny's scan handler
 * makes exactly that request when a scan finds no networks, which is what
 * happens right after an access point drops the device: the chip hangs and
 * resets about ten seconds later.
 *
 * Linked with -Wl,--wrap=pvPortMalloc for LN882x only, so every caller,
 * LibreTiny's malloc wrappers included, goes through here. Drop this once
 * LibreTiny stops allocating zero entries in its LN882H scan handler.
 */

#ifdef USE_LN882X

#include <stddef.h>

void *__real_pvPortMalloc(size_t size);  // NOLINT(readability-identifier-naming)

void *__wrap_pvPortMalloc(size_t size) {  // NOLINT(readability-identifier-naming)
  return __real_pvPortMalloc(size == 0 ? 1 : size);
}

#endif  // USE_LN882X
