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

/// Look up a LwIP socket struct from a file descriptor.
/// Returns NULL if fd is invalid or the socket/netconn is not initialized.
/// Use this at registration time to cache the pointer for esphome_lwip_socket_has_data().
struct lwip_sock *esphome_lwip_get_sock(int fd);

/// Check if a cached LwIP socket has data ready via unlocked hint read of rcvevent.
/// On ESPHOME_THREAD_MULTI_ATOMICS builds, the caller must run on the main
/// loop task after Application::loop's per-iter std::atomic_thread_fence
/// (memory_order_acquire); that fence pairs with the TCP/IP thread's
/// SYS_ARCH_UNPROTECT release, so a plain load suffices and avoids the
/// per-call `memw` that volatile would emit on Xtensa under default
/// -mserialize-volatile. Without atomics (e.g. BK72xx), the fence is skipped
/// and the volatile load provides ordering on its own.
/// Stale reads are harmless either way: the hooked event_callback
/// xTaskNotifyGives on RCVPLUS, so the next iteration re-snapshots and
/// ulTaskNotifyTake never loses a wake.
/// The offset and size are verified at compile time in lwip_fast_select.c.
static inline bool esphome_lwip_socket_has_data(struct lwip_sock *sock) {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  return *(int16_t *) ((char *) sock + (int) ESPHOME_LWIP_SOCK_RCVEVENT_OFFSET) > 0;
#else
  return *(volatile int16_t *) ((char *) sock + (int) ESPHOME_LWIP_SOCK_RCVEVENT_OFFSET) > 0;
#endif
}

/// Hook a socket's netconn callback to notify the main loop task on receive events.
/// Wraps the original event_callback with one that also calls xTaskNotifyGive().
/// Must be called from the main loop after socket creation.
/// The sock pointer must have been obtained from esphome_lwip_get_sock().
void esphome_lwip_hook_socket(struct lwip_sock *sock);

/// Set the listener netconn that the fast-select callback filters OTA wakes against.
/// After this is called, the OTA wake hook only fires for RCVPLUS events whose `conn`
/// matches this listener. Passing NULL disables OTA wakes (no event matches a NULL
/// listener) — correct behavior before install and after teardown.
void esphome_fast_select_set_ota_listener_sock(struct lwip_sock *sock);

/// Set or clear TCP_NODELAY on a socket's tcp_pcb directly.
/// Must be called with the TCPIP core lock held (LwIPLock in C++).
/// This bypasses lwip_setsockopt() overhead (socket lookups, switch cascade,
/// hooks, refcounting) — just a direct pcb->flags bit set/clear.
/// Returns true if successful, false if sock/conn/pcb is NULL or the socket is not TCP.
bool esphome_lwip_set_nodelay(struct lwip_sock *sock, bool enable);

#ifdef __cplusplus
}
#endif
