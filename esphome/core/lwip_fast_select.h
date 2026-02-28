#pragma once

// Fast socket monitoring for ESP32 and LibreTiny (LwIP >= 2.1.3)
// Replaces lwip_select() with direct rcvevent reads and FreeRTOS task notifications.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize fast select — must be called from the main loop task during setup().
/// Saves the current task handle for xTaskNotifyGive() wake notifications.
void esphome_lwip_fast_select_init(void);

/// Check if a LwIP socket has data ready via direct rcvevent read (~215 ns per socket).
/// Uses lwip_socket_dbg_get_socket() — a direct array lookup without the refcount that
/// get_socket()/done_socket() uses. Safe because the caller owns the socket lifetime:
/// both has_data reads and socket close/unregister happen on the main loop thread.
bool esphome_lwip_socket_has_data(int fd);

/// Hook a socket's netconn callback to notify the main loop task on receive events.
/// Wraps the original event_callback with one that also calls xTaskNotifyGive().
/// Must be called from the main loop after socket creation.
void esphome_lwip_hook_socket(int fd);

/// Wake the main loop task from another FreeRTOS task — costs <1 us.
/// NOT ISR-safe — must only be called from task context.
void esphome_lwip_wake_main_loop(void);

#ifdef __cplusplus
}
#endif
