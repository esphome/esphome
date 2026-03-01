#pragma once

// Fast socket monitoring for ESP32 and LibreTiny (LwIP >= 2.1.3)
// Replaces lwip_select() with direct rcvevent reads and FreeRTOS task notifications.

#include <stdbool.h>

// Forward declare lwip_sock for C++ callers that store cached pointers.
// The full definition is only available in the .c file (lwip/priv/sockets_priv.h
// conflicts with C++ compilation units).
struct lwip_sock;

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

/// Check if a cached LwIP socket has data ready via direct rcvevent read.
/// The sock pointer must have been obtained from esphome_lwip_get_sock() and must
/// remain valid (caller owns socket lifetime — no concurrent close).
/// Hot path: no fd lookup, no null checks — just a volatile 16-bit load.
bool esphome_lwip_socket_has_data(struct lwip_sock *sock);

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

#ifdef __cplusplus
}
#endif
