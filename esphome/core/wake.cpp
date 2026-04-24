#include "esphome/core/wake.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_ESP8266
#include <coredecls.h>
#endif

#ifdef USE_HOST
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#endif

namespace esphome {

// === Wake-requested flag storage ===
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<uint8_t> g_wake_requested{0};
#else
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile uint8_t g_wake_requested = 0;
#endif

// === ESP32 / LibreTiny — IRAM_ATTR entry points ===
#if defined(USE_ESP32) || defined(USE_LIBRETINY)
void IRAM_ATTR wake_loop_isrsafe(BaseType_t *px_higher_priority_task_woken) {
  // ISR-safe: set flag before notify so the wake is visible on the next gate
  // check. wake_request_set() is just an aligned 8-bit store / atomic store
  // and is safe from IRAM.
  wake_request_set();
  esphome_main_task_notify_from_isr(px_higher_priority_task_woken);
}
void IRAM_ATTR wake_loop_any_context() { wake_main_task_any_context(); }
#endif

// === ESP8266 / RP2040 ===
#if defined(USE_ESP8266) || defined(USE_RP2040)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile bool g_main_loop_woke = false;
#endif

#ifdef USE_ESP8266
void IRAM_ATTR wake_loop_any_context() { wake_loop_impl(); }
#endif

// === RP2040 — wakeable_delay (needs file-scope state for alarm callback) ===
#ifdef USE_RP2040
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static volatile bool s_delay_expired = false;

static int64_t alarm_callback_(alarm_id_t id, void *user_data) {
  (void) id;
  (void) user_data;
  s_delay_expired = true;
  __sev();
  return 0;
}

namespace internal {
void wakeable_delay(uint32_t ms) {
  if (ms == 0) [[unlikely]] {
    yield();
    return;
  }
  if (g_main_loop_woke) {
    g_main_loop_woke = false;
    return;
  }
  s_delay_expired = false;
  alarm_id_t alarm = add_alarm_in_ms(ms, alarm_callback_, nullptr, true);
  if (alarm <= 0) {
    delay(ms);
    return;
  }
  while (!g_main_loop_woke && !s_delay_expired) {
    __wfe();
  }
  if (!s_delay_expired)
    cancel_alarm(alarm);
  g_main_loop_woke = false;
}
}  // namespace internal
#endif  // USE_RP2040

// === Host (UDP loopback socket + select() based fd watcher) ===
#ifdef USE_HOST

static const char *const TAG = "wake";

namespace internal {
// File-scope state — referenced inline by wake_drain_notifications() and
// wake_fd_ready() in wake.h, and by the bodies in this file.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
int g_wake_socket_fd = -1;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
fd_set g_read_fds{};
}  // namespace internal

namespace {
// File-local state owned entirely by the select() loop.
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<int> s_socket_fds;
int s_max_fd = -1;
bool s_socket_fds_changed = false;
fd_set s_base_read_fds{};
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace

bool wake_register_fd(int fd) {
  // WARNING: not thread-safe — must be called only from the main loop.
  if (fd < 0)
    return false;

  if (fd >= FD_SETSIZE) {
    ESP_LOGE(TAG, "fd %d exceeds FD_SETSIZE %d", fd, FD_SETSIZE);
    return false;
  }

  s_socket_fds.push_back(fd);
  s_socket_fds_changed = true;
  if (fd > s_max_fd) {
    s_max_fd = fd;
  }

  return true;
}

void wake_unregister_fd(int fd) {
  // WARNING: not thread-safe — must be called only from the main loop.
  if (fd < 0)
    return;

  for (size_t i = 0; i < s_socket_fds.size(); i++) {
    if (s_socket_fds[i] != fd)
      continue;

    // Swap with last element and pop — O(1) removal since order doesn't matter.
    if (i < s_socket_fds.size() - 1)
      s_socket_fds[i] = s_socket_fds.back();
    s_socket_fds.pop_back();
    s_socket_fds_changed = true;
    // Only recalculate max_fd if we removed the current max.
    if (fd == s_max_fd) {
      s_max_fd = -1;
      for (int sock_fd : s_socket_fds) {
        if (sock_fd > s_max_fd)
          s_max_fd = sock_fd;
      }
    }
    return;
  }
}

namespace internal {
void wakeable_delay(uint32_t ms) {
  // Fallback select() path for the host platform (and any future platform
  // without fast select). select() is the host equivalent of FreeRTOS task
  // notify / esp_delay / WFE used on the embedded targets.
  if (!s_socket_fds.empty()) [[likely]] {
    // Update fd_set if socket list has changed.
    if (s_socket_fds_changed) [[unlikely]] {
      FD_ZERO(&s_base_read_fds);
      // fd bounds are validated in wake_register_fd().
      for (int fd : s_socket_fds) {
        FD_SET(fd, &s_base_read_fds);
      }
      s_socket_fds_changed = false;
    }

    // Copy base fd_set before each select.
    g_read_fds = s_base_read_fds;

    // Convert ms to timeval.
    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms - tv.tv_sec * 1000) * 1000;

    // Call select with timeout.
    int ret = ::select(s_max_fd + 1, &g_read_fds, nullptr, nullptr, &tv);

    // Process select() result:
    // ret > 0: socket(s) have data ready - normal and expected
    // ret == 0: timeout occurred - normal and expected
    if (ret >= 0) [[likely]] {
      // Yield if zero timeout since select(0) only polls without yielding.
      if (ms == 0) [[unlikely]] {
        yield();
      }
      return;
    }
    // ret < 0: error (EINTR is normal, anything else is unexpected).
    const int err = errno;
    if (err == EINTR) {
      return;
    }
    // select() error - log and fall through to delay().
    ESP_LOGW(TAG, "select() failed with errno %d", err);
  }
  // No sockets registered or select() failed - use regular delay.
  delay(ms);
}
}  // namespace internal

void wake_loop_threadsafe() {
  // Set flag before sending so the consumer's gate check on the next loop()
  // entry observes the wake regardless of select() scheduling.
  wake_request_set();
  if (internal::g_wake_socket_fd >= 0) {
    const char dummy = 1;
    ::send(internal::g_wake_socket_fd, &dummy, 1, 0);
  }
}

void wake_setup() {
  // Create UDP socket for wake notifications.
  internal::g_wake_socket_fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (internal::g_wake_socket_fd < 0) {
    ESP_LOGW(TAG, "Wake socket create failed: %d", errno);
    return;
  }

  // Bind to loopback with auto-assigned port.
  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;  // Auto-assign port

  if (::bind(internal::g_wake_socket_fd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    ESP_LOGW(TAG, "Wake socket bind failed: %d", errno);
    ::close(internal::g_wake_socket_fd);
    internal::g_wake_socket_fd = -1;
    return;
  }

  // Get the assigned address and connect to it.
  // Connecting a UDP socket allows using send() instead of sendto() for better performance.
  struct sockaddr_in wake_addr;
  socklen_t len = sizeof(wake_addr);
  if (::getsockname(internal::g_wake_socket_fd, (struct sockaddr *) &wake_addr, &len) < 0) {
    ESP_LOGW(TAG, "Wake socket address failed: %d", errno);
    ::close(internal::g_wake_socket_fd);
    internal::g_wake_socket_fd = -1;
    return;
  }

  // Connect to self (loopback) — allows using send() instead of sendto().
  // After connect(), no need to store wake_addr — the socket remembers it.
  if (::connect(internal::g_wake_socket_fd, (struct sockaddr *) &wake_addr, sizeof(wake_addr)) < 0) {
    ESP_LOGW(TAG, "Wake socket connect failed: %d", errno);
    ::close(internal::g_wake_socket_fd);
    internal::g_wake_socket_fd = -1;
    return;
  }

  // Set non-blocking mode.
  int flags = ::fcntl(internal::g_wake_socket_fd, F_GETFL, 0);
  ::fcntl(internal::g_wake_socket_fd, F_SETFL, flags | O_NONBLOCK);

  // Register with the select() loop.
  if (!wake_register_fd(internal::g_wake_socket_fd)) {
    ESP_LOGW(TAG, "Wake socket register failed");
    ::close(internal::g_wake_socket_fd);
    internal::g_wake_socket_fd = -1;
    return;
  }
}
#endif  // USE_HOST

}  // namespace esphome
