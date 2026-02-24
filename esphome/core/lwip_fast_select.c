// Fast socket monitoring for ESP32 (ESP-IDF LwIP)
// Replaces lwip_select() with direct rcvevent reads and FreeRTOS task notifications.
//
// This must be a .c file (not .cpp) because:
// 1. lwip/priv/sockets_priv.h conflicts with C++ compilation units that include bootloader headers
// 2. The netconn callback is a C function pointer
//
// defines.h is force-included by the build system (-include flag), providing USE_ESP32 etc.

#ifdef USE_ESP32

// LwIP headers must come first — they define netconn_callback, struct lwip_sock, etc.
#include <lwip/api.h>
#include <lwip/priv/sockets_priv.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/core/lwip_fast_select.h"

// Task handle for the main loop — set during setup, read from any thread/ISR
static TaskHandle_t s_main_loop_task = NULL;

// Saved original event_callback pointer (same for all LwIP sockets)
static netconn_callback s_original_callback = NULL;

// Wrapper callback: calls original event_callback + notifies main loop task.
// Called from LwIP's TCP/IP thread when socket events occur.
static void esphome_socket_event_callback(struct netconn *conn, enum netconn_evt evt, u16_t len) {
  // Call original LwIP event_callback first — updates rcvevent/sendevent/errevent,
  // signals any select() waiters. This preserves all LwIP behavior.
  if (s_original_callback) {
    s_original_callback(conn, evt, len);
  }
  // Wake the main loop task if sleeping in ulTaskNotifyTake().
  // Only notify on receive events to avoid spurious wakeups from send-ready events.
  // xTaskNotifyGive is thread-safe and ISR-safe, costs <1 us.
  if (evt == NETCONN_EVT_RCVPLUS) {
    TaskHandle_t task = s_main_loop_task;
    if (task != NULL) {
      xTaskNotifyGive(task);
    }
  }
}

void esphome_lwip_fast_select_init(void) { s_main_loop_task = xTaskGetCurrentTaskHandle(); }

bool esphome_lwip_socket_has_data(int fd) {
  struct lwip_sock *sock = lwip_socket_dbg_get_socket(fd);
  return sock != NULL && sock->conn != NULL && sock->rcvevent > 0;
}

void esphome_lwip_hook_socket(int fd) {
  struct lwip_sock *sock = lwip_socket_dbg_get_socket(fd);
  if (sock == NULL || sock->conn == NULL)
    return;

  // Save original callback (only once — same event_callback for all LwIP sockets)
  if (s_original_callback == NULL) {
    s_original_callback = sock->conn->callback;
  }

  // Replace with our wrapper
  sock->conn->callback = esphome_socket_event_callback;
}

void esphome_lwip_unhook_socket(int fd) {
  struct lwip_sock *sock = lwip_socket_dbg_get_socket(fd);
  if (sock == NULL || sock->conn == NULL)
    return;

  // Restore original callback
  if (s_original_callback != NULL) {
    sock->conn->callback = s_original_callback;
  }
}

void esphome_lwip_wake_main_loop(void) {
  TaskHandle_t task = s_main_loop_task;
  if (task != NULL) {
    xTaskNotifyGive(task);
  }
}

#endif  // USE_ESP32
