#pragma once

// Fast socket monitoring for ESP32 and LibreTiny (LwIP >= 2.1.3)
// Replaces lwip_select() with direct rcvevent reads and FreeRTOS task notifications.

#include <stdbool.h>
#include <stdint.h>

// Forward declare lwip_sock for C++ callers that store cached pointers.
// The full definition is only available in the .c file (lwip/priv/sockets_priv.h
// conflicts with C++ compilation units).
struct lwip_sock;

// Byte offset of rcvevent (s16_t) within struct lwip_sock.
// Verified at compile time in lwip_fast_select.c via _Static_assert.
// Anonymous enum for a compile-time constant that works in both C and C++.
enum { ESPHOME_LWIP_SOCK_RCVEVENT_OFFSET = 8 };

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize fast select — must be called from the main loop task during setup().
/// Saves the current task handle for xTaskNotifyGive() wake notifications.
void esphome_lwip_fast_select_init(void);

/// Look up a LwIP socket struct from a file descriptor.
/// Returns NULL if fd is invalid or the socket/netconn is not initialized.
/// Use this at registration time to cache the pointer for esphome_lwip_socket_has_data().
struct lwip_sock *esphome_lwip_get_sock(int fd);

/// Check if a cached LwIP socket has data ready via unlocked hint read of rcvevent.
/// This avoids lwIP core lock contention between the main loop (CPU0) and
/// streaming/networking work (CPU1). Correctness is preserved because callers
/// already handle EWOULDBLOCK on nonblocking sockets — a stale hint simply causes
/// a harmless retry on the next loop iteration. In practice, stale reads have not
/// been observed across multi-day testing, but the design does not depend on that.
///
/// The sock pointer must have been obtained from esphome_lwip_get_sock() and must
/// remain valid (caller owns socket lifetime — no concurrent close).
/// Hot path: inlined volatile 16-bit load — no function call overhead.
/// Uses offset-based access because lwip/priv/sockets_priv.h conflicts with C++.
/// The offset and size are verified at compile time in lwip_fast_select.c.
static inline bool esphome_lwip_socket_has_data(struct lwip_sock *sock) {
  // Unlocked hint read — no lwIP core lock needed.
  // volatile prevents the compiler from caching/reordering this cross-thread read.
  // The write side (TCP/IP thread) commits via SYS_ARCH_UNPROTECT which releases a
  // FreeRTOS mutex (ESP32) or resumes the scheduler (LibreTiny), ensuring the value
  // is visible. Aligned 16-bit reads are single-instruction loads (L16SI/LH/LDRH) on
  // Xtensa/RISC-V/ARM and cannot produce torn values.
  return *(volatile int16_t *) ((char *) sock + (int) ESPHOME_LWIP_SOCK_RCVEVENT_OFFSET) > 0;
}

/// Hook a socket's netconn callback to notify the main loop task on receive events.
/// Wraps the original event_callback with one that also calls xTaskNotifyGive().
/// Must be called from the main loop after socket creation.
/// The sock pointer must have been obtained from esphome_lwip_get_sock().
void esphome_lwip_hook_socket(struct lwip_sock *sock);

/// Wake the main loop task from another FreeRTOS task — costs <1 us.
/// NOT ISR-safe — must only be called from task context.
void esphome_lwip_wake_main_loop(void);

/// Wake the main loop task from an ISR — costs <1 us.
/// ISR-safe variant using vTaskNotifyGiveFromISR().
/// @param px_higher_priority_task_woken Set to pdTRUE if a context switch is needed.
void esphome_lwip_wake_main_loop_from_isr(int *px_higher_priority_task_woken);

/// Set or clear TCP_NODELAY on a socket's tcp_pcb directly.
/// Must be called with the TCPIP core lock held (LwIPLock in C++).
/// This bypasses lwip_setsockopt() overhead (socket lookups, switch cascade,
/// hooks, refcounting) — just a direct pcb->flags bit set/clear.
/// Returns true if successful, false if sock/conn/pcb is NULL or the socket is not TCP.
bool esphome_lwip_set_nodelay(struct lwip_sock *sock, bool enable);

/// Wake the main loop task from any context (ISR, thread, or main loop).
/// ESP32-only: uses xPortInIsrContext() to detect ISR context.
/// LibreTiny lacks IRAM_ATTR support needed for ISR-safe paths.
#ifdef USE_ESP32
void esphome_lwip_wake_main_loop_any_context(void);
#endif

#ifdef __cplusplus
}
#endif
