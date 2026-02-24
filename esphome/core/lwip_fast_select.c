// Fast socket monitoring for ESP32 (ESP-IDF LwIP)
// Replaces lwip_select() with direct rcvevent reads and FreeRTOS task notifications.
//
// This must be a .c file (not .cpp) because:
// 1. lwip/priv/sockets_priv.h conflicts with C++ compilation units that include bootloader headers
// 2. The netconn callback is a C function pointer
//
// defines.h is force-included by the build system (-include flag), providing USE_ESP32 etc.
//
// Thread safety analysis
// ======================
// Three threads interact with this code:
//   1. Main loop task   — calls init, has_data, hook
//   2. LwIP TCP/IP task — calls event_callback (reads s_original_callback; writes rcvevent
//                         via the original callback under SYS_ARCH_PROTECT/UNPROTECT mutex)
//   3. Background tasks — call wake_main_loop
//
// LwIP source references (STABLE-2_2_0_RELEASE):
//   https://github.com/lwip-tcpip/lwip/blob/STABLE-2_2_0_RELEASE/src/api/sockets.c
//     - event_callback (static, same for all sockets): line 619
//     - DEFAULT_SOCKET_EVENTCB = event_callback: line 622
//     - lwip_socket_dbg_get_socket (direct array lookup, no locking): line 654
//     - tryget_socket_unconn_nouse (the array lookup helper): line 1008
//     - All socket types use DEFAULT_SOCKET_EVENTCB: lines 3309-3325
//     - event_callback SYS_ARCH_PROTECT before rcvevent switch: line 3685
//     - sock->rcvevent++ (NETCONN_EVT_RCVPLUS case): line 3688
//     - SYS_ARCH_UNPROTECT after switch: line 3720
//   https://github.com/lwip-tcpip/lwip/blob/STABLE-2_2_0_RELEASE/src/include/lwip/sys.h
//     - SYS_ARCH_PROTECT calls sys_arch_protect(): line 557
//     - SYS_ARCH_UNPROTECT calls sys_arch_unprotect(): line 568
//     (ESP-IDF implements sys_arch_protect/unprotect as FreeRTOS mutex lock/unlock)
//
// Shared state and safety rationale:
//
//   s_main_loop_task (TaskHandle_t, 4 bytes):
//     Written once by main loop in init(), before any hook/wake calls.
//     Read by TCP/IP thread (in callback) and background tasks (in wake).
//     Safe: write-once-then-read pattern. The init() call completes during setup()
//     before any sockets are hooked, so all subsequent reads see the final value.
//
//   s_original_callback (netconn_callback, 4-byte function pointer):
//     Written by main loop in hook_socket() (only when NULL — set once).
//     Read by TCP/IP thread in esphome_socket_event_callback().
//     Safe: set-once pattern. The first hook_socket() captures the original callback.
//     All subsequent hooks see it already set and skip the write. The TCP/IP thread
//     only reads this after the callback pointer has been swapped (which happens after
//     the write), so it always sees the initialized value.
//
//   sock->conn->callback (netconn_callback, 4-byte function pointer):
//     Written by main loop in hook_socket(). Never restored — all LwIP sockets share
//     the same static event_callback (line 619, 622), so the wrapper stays permanently.
//     Read by TCP/IP thread when invoking the callback.
//     Safe: 32-bit aligned pointer writes are atomic on Xtensa and RISC-V (ESP32).
//     The TCP/IP thread will see either the old or new pointer atomically — never a
//     torn value. Both the wrapper and original callbacks are valid at all times
//     (the wrapper itself calls the original), so either value is correct.
//
//   sock->rcvevent (s16_t, 2 bytes):
//     Written by TCP/IP thread in event_callback under SYS_ARCH_PROTECT (line 3685).
//     Read by main loop in has_data() via volatile cast.
//     Safe: SYS_ARCH_UNPROTECT (line 3720) releases a FreeRTOS mutex, which internally
//     uses a critical section with memory barrier (rsync on Xtensa), ensuring the write
//     is committed before the mutex is released. The volatile cast prevents the compiler
//     from caching the read. Aligned 16-bit reads are single-instruction loads on
//     Xtensa (L16SI) and RISC-V (LH), which cannot produce torn values.
//
//   FreeRTOS task notification value:
//     Written by TCP/IP thread (xTaskNotifyGive in callback) and background tasks
//     (xTaskNotifyGive in wake_main_loop). Read by main loop (ulTaskNotifyTake).
//     Safe: FreeRTOS notification APIs are thread-safe by design (use internal
//     critical sections). Multiple concurrent xTaskNotifyGive calls are safe —
//     the notification count simply increments.

#ifdef USE_ESP32

// LwIP headers must come first — they define netconn_callback, struct lwip_sock, etc.
#include <lwip/api.h>
#include <lwip/priv/sockets_priv.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/core/lwip_fast_select.h"

#include <stddef.h>

// Compile-time verification of thread safety assumptions.
// On ESP32 (Xtensa/RISC-V), naturally-aligned reads/writes up to 32 bits are atomic.
// These asserts ensure our cross-thread shared state meets those requirements.

// Pointer types must fit in a single 32-bit store (atomic write)
_Static_assert(sizeof(TaskHandle_t) <= 4, "TaskHandle_t must be <= 4 bytes for atomic access");
_Static_assert(sizeof(netconn_callback) <= 4, "netconn_callback must be <= 4 bytes for atomic access");

// rcvevent must fit in a single atomic read
_Static_assert(sizeof(((struct lwip_sock *) 0)->rcvevent) <= 4, "rcvevent must be <= 4 bytes for atomic access");

// Struct member alignment — natural alignment guarantees atomicity on Xtensa/RISC-V.
// Misaligned access would not be atomic even if the size is <= 4 bytes.
_Static_assert(offsetof(struct netconn, callback) % sizeof(netconn_callback) == 0,
               "netconn.callback must be naturally aligned for atomic access");
_Static_assert(offsetof(struct lwip_sock, rcvevent) % sizeof(((struct lwip_sock *) 0)->rcvevent) == 0,
               "lwip_sock.rcvevent must be naturally aligned for atomic access");

// Task handle for the main loop — written once in init(), read from TCP/IP and background tasks.
static TaskHandle_t s_main_loop_task = NULL;

// Saved original event_callback pointer — written once in first hook_socket(), read from TCP/IP task.
static netconn_callback s_original_callback = NULL;

// Wrapper callback: calls original event_callback + notifies main loop task.
// Called from LwIP's TCP/IP thread when socket events occur (task context, not ISR).
static void esphome_socket_event_callback(struct netconn *conn, enum netconn_evt evt, u16_t len) {
  // Call original LwIP event_callback first — updates rcvevent/sendevent/errevent,
  // signals any select() waiters. This preserves all LwIP behavior.
  // s_original_callback is always valid here: hook_socket() sets it before swapping
  // the callback pointer, so this wrapper cannot run until it's initialized.
  s_original_callback(conn, evt, len);
  // Wake the main loop task if sleeping in ulTaskNotifyTake().
  // Only notify on receive events to avoid spurious wakeups from send-ready events.
  if (evt == NETCONN_EVT_RCVPLUS) {
    TaskHandle_t task = s_main_loop_task;
    if (task != NULL) {
      xTaskNotifyGive(task);
    }
  }
}

void esphome_lwip_fast_select_init(void) { s_main_loop_task = xTaskGetCurrentTaskHandle(); }

// lwip_socket_dbg_get_socket() is a thin wrapper around the static
// tryget_socket_unconn_nouse() — a direct array lookup without the refcount
// that get_socket()/done_socket() uses. This is safe because the caller owns
// the socket lifetime: both has_data() and socket close happen on the main
// loop thread, so the sockets[] entry cannot be freed while we read it.
// If lwip_socket_dbg_get_socket() were ever removed, we could fall back to lwip_select().
// Returns the sock only if both the sock and its netconn are valid, NULL otherwise.
static inline struct lwip_sock *get_sock(int fd) {
  struct lwip_sock *sock = lwip_socket_dbg_get_socket(fd);
  if (sock == NULL || sock->conn == NULL)
    return NULL;
  return sock;
}

bool esphome_lwip_socket_has_data(int fd) {
  struct lwip_sock *sock = get_sock(fd);
  if (sock == NULL)
    return false;
  // volatile prevents the compiler from caching/reordering this cross-thread read.
  // The write side (TCP/IP thread) commits via SYS_ARCH_UNPROTECT which releases a
  // FreeRTOS mutex with a memory barrier (rsync on Xtensa), ensuring the value is
  // visible. Aligned 16-bit reads are single-instruction loads (L16SI/LH) on
  // Xtensa/RISC-V and cannot produce torn values.
  return *(volatile s16_t *) &sock->rcvevent > 0;
}

void esphome_lwip_hook_socket(int fd) {
  struct lwip_sock *sock = get_sock(fd);
  if (sock == NULL)
    return;

  // Save original callback once — all LwIP sockets share the same static event_callback
  // (DEFAULT_SOCKET_EVENTCB in sockets.c, used for SOCK_RAW, SOCK_DGRAM, and SOCK_STREAM).
  if (s_original_callback == NULL) {
    s_original_callback = sock->conn->callback;
  }

  // Replace with our wrapper. Atomic on ESP32 (32-bit aligned pointer write).
  // TCP/IP thread sees either old or new pointer — both are valid.
  sock->conn->callback = esphome_socket_event_callback;
}

// Wake the main loop from another FreeRTOS task. NOT ISR-safe.
void esphome_lwip_wake_main_loop(void) {
  TaskHandle_t task = s_main_loop_task;
  if (task != NULL) {
    xTaskNotifyGive(task);
  }
}

#endif  // USE_ESP32
