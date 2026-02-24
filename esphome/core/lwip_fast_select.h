#pragma once

// Fast socket monitoring for ESP32 (ESP-IDF LwIP)
// Replaces lwip_select() with direct rcvevent reads and FreeRTOS task notifications.
// See fast_select.md for design rationale and benchmarks.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize fast select — must be called from the main loop task during setup().
/// Saves the current task handle for xTaskNotifyGive() wake notifications.
void esphome_lwip_fast_select_init(void);

/// Check if a LwIP socket has data ready via direct rcvevent read (~215 ns per socket).
/// Uses lwip_socket_dbg_get_socket() which is a direct array lookup — no locking, no refcount.
/// Safe for single-threaded polling from the main loop.
bool esphome_lwip_socket_has_data(int fd);

/// Hook a socket's netconn callback to notify the main loop task on receive events.
/// Wraps the original event_callback with one that also calls xTaskNotifyGive().
/// Must be called from the main loop after socket creation.
void esphome_lwip_hook_socket(int fd);

/// Unhook a socket's netconn callback, restoring the original event_callback.
/// Must be called from the main loop before closing the socket.
void esphome_lwip_unhook_socket(int fd);

/// Wake the main loop task from any thread or ISR — costs <1 us.
/// Replaces the UDP loopback socket wake mechanism.
void esphome_lwip_wake_main_loop(void);

#ifdef __cplusplus
}
#endif
