// Fast socket monitoring for ESP32 and LibreTiny (LwIP >= 2.1.3)
// Replaces lwip_select() with direct rcvevent reads and FreeRTOS task notifications.
//
// This must be a .c file (not .cpp) because:
// 1. lwip/priv/sockets_priv.h conflicts with C++ compilation units
// 2. The netconn callback is a C function pointer
//
// USE_ESP32 and USE_LIBRETINY platform flags (-D) control compilation of this file.
// See the guard at the bottom of the header comment for details.
//
// Thread safety analysis
// ======================
// Three threads interact with this code:
//   1. Main loop task   — calls init, has_data, hook
//   2. LwIP TCP/IP task — calls event_callback (reads s_original_callback; writes rcvevent
//                         via the original callback under SYS_ARCH_PROTECT/UNPROTECT mutex)
//   3. Background tasks — call wake_main_loop
//
// LwIP source references (ESP-IDF v5.5.2, commit 30aaf64524):
//   sockets.c: https://github.com/espressif/esp-idf/blob/30aaf64524/components/lwip/lwip/src/api/sockets.c
//     - event_callback (static, same for all sockets): L327
//     - DEFAULT_SOCKET_EVENTCB = event_callback: L328
//     - tryget_socket_unconn_nouse (direct array lookup): L450
//     - lwip_socket_dbg_get_socket (thin wrapper): L461
//     - All socket types use DEFAULT_SOCKET_EVENTCB: L1741, L1748, L1759
//     - event_callback definition: L2538
//     - SYS_ARCH_PROTECT before rcvevent switch: L2578
//     - sock->rcvevent++ (NETCONN_EVT_RCVPLUS case): L2582
//     - SYS_ARCH_UNPROTECT after switch: L2615
//   sys.h: https://github.com/espressif/esp-idf/blob/30aaf64524/components/lwip/lwip/src/include/lwip/sys.h
//     - SYS_ARCH_PROTECT calls sys_arch_protect(): L495
//     - SYS_ARCH_UNPROTECT calls sys_arch_unprotect(): L506
//     (ESP-IDF implements sys_arch_protect/unprotect as FreeRTOS mutex lock/unlock)
//
// Socket slot lifetime
// ====================
// This code reads struct lwip_sock fields without SYS_ARCH_PROTECT. The safety
// argument requires that the slot cannot be freed while we read it.
//
// In LwIP, the socket table is a static array and slots are only freed via:
//   lwip_close() -> lwip_close_internal() -> free_socket_free_elements() -> free_socket()
// The TCP/IP thread does NOT call free_socket(). On link loss, RST, or timeout
// it frees the TCP PCB and signals the netconn (rcvevent++ to indicate EOF), but
// the netconn and lwip_sock slot remain allocated until the application calls
// lwip_close(). ESPHome removes the fd from the monitored set before calling
// lwip_close().
//
// Therefore lwip_socket_dbg_get_socket(fd) plus a volatile read of rcvevent
// (to prevent compiler reordering or caching) is safe as long as the application
// is single-writer for close. ESPHome guarantees this by design: all socket
// create/read/close happens on the main loop. fd numbers are not reused while
// the slot remains allocated, and the slot remains allocated until lwip_close().
// Any change in LwIP that allows free_socket() to be called outside lwip_close()
// would invalidate this assumption.
//
// LwIP source references for slot lifetime:
//   sockets.c (same commit as above):
//     - alloc_socket (slot allocation): L419
//     - free_socket (slot deallocation): L384
//     - free_socket_free_elements (called from lwip_close_internal): L393
//     - lwip_close_internal (only caller of free_socket_free_elements): L2355
//     - lwip_close (only caller of lwip_close_internal): L2450
//
// Shared state and safety rationale:
//
//   esphome_main_task_handle (TaskHandle_t, 4 bytes, defined in main_task.c):
//     Written once by main loop in Application::setup(). Read by TCP/IP thread
//     (in callback) and background tasks (in wake).
//     Safe: write-once-then-read pattern. Socket hooks may run before setup(),
//     but the NULL check on esphome_main_task_handle in the callback provides correct
//     degraded behavior — notifications are simply skipped until setup() completes.
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
//     the same static event_callback (DEFAULT_SOCKET_EVENTCB), so the wrapper stays permanently.
//     Read by TCP/IP thread when invoking the callback.
//     Safe: 32-bit aligned pointer writes are atomic on Xtensa, RISC-V (ESP32),
//     and ARM Cortex-M (LibreTiny). The TCP/IP thread will see either the old or
//     new pointer atomically — never a torn value. Both the wrapper and original
//     callbacks are valid at all times (the wrapper itself calls the original),
//     so either value is correct.
//
//   sock->rcvevent (s16_t, 2 bytes):
//     Written by TCP/IP thread in event_callback under SYS_ARCH_PROTECT.
//     Read by main loop in has_data() via volatile cast.
//     Safe: SYS_ARCH_UNPROTECT releases a FreeRTOS mutex (ESP32) or resumes the
//     scheduler (LibreTiny), both providing a memory barrier. The volatile cast
//     prevents the compiler from caching the read. Aligned 16-bit reads are
//     single-instruction loads on Xtensa (L16SI), RISC-V (LH), and ARM Cortex-M
//     (LDRH), which cannot produce torn values. On single-core chips (LibreTiny,
//     ESP32-C3/C6/H2) cross-core visibility is not an issue.
//
//   FreeRTOS task notification value:
//     Written by TCP/IP thread (xTaskNotifyGive in callback) and background tasks
//     (xTaskNotifyGive in wake_main_loop). Read by main loop (ulTaskNotifyTake).
//     Safe: FreeRTOS notification APIs are thread-safe by design (use internal
//     critical sections). Multiple concurrent xTaskNotifyGive calls are safe —
//     the notification count simply increments.

// USE_LWIP_FAST_SELECT is set via -D build flag (not cg.add_define) so it is
// visible in both .c and .cpp translation units.
#ifdef USE_LWIP_FAST_SELECT

// LwIP headers must come first — they define netconn_callback, struct lwip_sock, etc.
#include <lwip/api.h>
#include <lwip/priv/sockets_priv.h>
#include <lwip/tcp.h>
// FreeRTOS include paths differ: ESP-IDF uses freertos/ prefix, LibreTiny does not
#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <FreeRTOS.h>
#include <task.h>
#endif

#include "esphome/core/lwip_fast_select.h"
#include "esphome/core/main_task.h"

#include <stddef.h>

// Compile-time verification of thread safety assumptions.
// On ESP32 (Xtensa/RISC-V) and LibreTiny (ARM Cortex-M), naturally-aligned
// reads/writes up to 32 bits are atomic.
// These asserts ensure our cross-thread shared state meets those requirements.

// Pointer types must fit in a single 32-bit store (atomic write)
_Static_assert(sizeof(TaskHandle_t) <= 4, "TaskHandle_t must be <= 4 bytes for atomic access");
_Static_assert(sizeof(netconn_callback) <= 4, "netconn_callback must be <= 4 bytes for atomic access");

// rcvevent must be exactly 2 bytes (s16_t) — the inline in lwip_fast_select.h reads it as int16_t.
// If lwIP changes this to int or similar, the offset assert would still pass but the load width would be wrong.
_Static_assert(sizeof(((struct lwip_sock *) 0)->rcvevent) == 2,
               "rcvevent size changed — update int16_t cast in esphome_lwip_socket_has_data() in lwip_fast_select.h");

// Struct member alignment — natural alignment guarantees atomicity on Xtensa/RISC-V/ARM.
// Misaligned access would not be atomic even if the size is <= 4 bytes.
_Static_assert(offsetof(struct netconn, callback) % sizeof(netconn_callback) == 0,
               "netconn.callback must be naturally aligned for atomic access");
_Static_assert(offsetof(struct lwip_sock, rcvevent) % sizeof(((struct lwip_sock *) 0)->rcvevent) == 0,
               "lwip_sock.rcvevent must be naturally aligned for atomic access");

// Verify the hardcoded offset used in the header's inline esphome_lwip_socket_has_data().
_Static_assert(offsetof(struct lwip_sock, rcvevent) == ESPHOME_LWIP_SOCK_RCVEVENT_OFFSET,
               "lwip_sock.rcvevent offset changed — update ESPHOME_LWIP_SOCK_RCVEVENT_OFFSET in lwip_fast_select.h");

// Task handle is in main_task.c (esphome_main_task_handle) — shared with wake.h.

// Saved original event_callback pointer — written once in first hook_socket(), read from TCP/IP task.
static netconn_callback s_original_callback = NULL;

#ifdef USE_OTA_PLATFORM_ESPHOME
static struct netconn *s_ota_listener_conn = NULL;
extern void esphome_wake_ota_component_any_context(void);

void esphome_fast_select_set_ota_listener_sock(struct lwip_sock *sock) {
  s_ota_listener_conn = (sock != NULL) ? sock->conn : NULL;
}
#else
void esphome_fast_select_set_ota_listener_sock(struct lwip_sock *sock) { (void) sock; }
#endif

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
  // NETCONN_EVT_ERROR is deliberately omitted: LwIP signals errors via RCVPLUS
  // (rcvevent++ with a NULL pbuf or error in recvmbox), so error conditions
  // already wake the main loop through the RCVPLUS path.
  if (evt == NETCONN_EVT_RCVPLUS) {
#ifdef USE_OTA_PLATFORM_ESPHOME
    // Mark OTA pending-enable only for events on its listen socket. MUST happen
    // before xTaskNotifyGive so the flags are visible when the main task wakes.
    if (conn == s_ota_listener_conn) {
      esphome_wake_ota_component_any_context();
    }
#endif
    TaskHandle_t task = esphome_main_task_handle;
    if (task != NULL) {
      xTaskNotifyGive(task);
    }
  }
}

// lwip_socket_dbg_get_socket() is a thin wrapper around the static
// tryget_socket_unconn_nouse() — a direct array lookup without the refcount
// that get_socket()/done_socket() uses. This is safe because:
// 1. The only path to free_socket() is lwip_close(), called exclusively from the main loop
// 2. The TCP/IP thread never frees socket slots (see "Socket slot lifetime" above)
// 3. Both has_data() reads and lwip_close() run on the main loop — no concurrent free
// If lwip_socket_dbg_get_socket() were ever removed, we could fall back to lwip_select().
// Returns the sock only if both the sock and its netconn are valid, NULL otherwise.
static inline struct lwip_sock *get_sock(int fd) {
  struct lwip_sock *sock = lwip_socket_dbg_get_socket(fd);
  if (sock == NULL || sock->conn == NULL)
    return NULL;
  return sock;
}

struct lwip_sock *esphome_lwip_get_sock(int fd) {
  return get_sock(fd);
}

void esphome_lwip_hook_socket(struct lwip_sock *sock) {
  // Save original callback once — all LwIP sockets share the same static event_callback
  // (DEFAULT_SOCKET_EVENTCB in sockets.c, used for SOCK_RAW, SOCK_DGRAM, and SOCK_STREAM).
  if (s_original_callback == NULL) {
    s_original_callback = sock->conn->callback;
  }

  // Replace with our wrapper. Atomic on all supported platforms (32-bit aligned pointer write).
  // TCP/IP thread sees either old or new pointer — both are valid.
  sock->conn->callback = esphome_socket_event_callback;
}

bool esphome_lwip_set_nodelay(struct lwip_sock *sock, bool enable) {
  if (sock == NULL || sock->conn == NULL)
    return false;
  if (NETCONNTYPE_GROUP(sock->conn->type) != NETCONN_TCP)
    return false;
  if (sock->conn->pcb.tcp == NULL)
    return false;
  if (enable) {
    tcp_nagle_disable(sock->conn->pcb.tcp);
  } else {
    tcp_nagle_enable(sock->conn->pcb.tcp);
  }
  return true;
}

#endif  // USE_LWIP_FAST_SELECT
