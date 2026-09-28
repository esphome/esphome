#pragma once

#include "esphome/core/defines.h"

#ifdef USE_HOST

#include "esphome/core/hal.h"

#include <sys/select.h>
#include <sys/socket.h>

namespace esphome {

/// Host: wakes select() via UDP loopback socket. Defined in wake_host.cpp.
void wake_loop_threadsafe();

/// Register a socket file descriptor with the host select() loop. Not
/// thread-safe — main loop only. Returns false if fd is invalid or
/// >= FD_SETSIZE.
bool wake_register_fd(int fd);

/// Unregister a socket file descriptor. Not thread-safe — main loop only.
void wake_unregister_fd(int fd);

/// One-time setup of the loopback wake socket. Called from Application::setup().
void wake_setup();

inline void wake_loop_any_context() { wake_loop_threadsafe(); }

namespace internal {
/// Host wakeable_delay uses select() over the registered fds — defined in wake_host.cpp.
void wakeable_delay(uint32_t ms);

// File-scope state owned by wake_host.cpp. Accessed inline by
// wake_drain_notifications() and wake_fd_ready() so the hot path stays in the header.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern int g_wake_socket_fd;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern fd_set g_read_fds;
}  // namespace internal

inline bool ESPHOME_ALWAYS_INLINE wake_fd_ready(int fd) { return FD_ISSET(fd, &internal::g_read_fds); }

// Small buffer for draining wake notification bytes (1 byte sent per wake).
// Sized to drain multiple notifications per recvfrom() without wasting stack.
inline constexpr size_t WAKE_NOTIFY_DRAIN_BUFFER_SIZE = 16;

inline void ESPHOME_ALWAYS_INLINE wake_drain_notifications() {
  // Called from main loop to drain any pending wake notifications.
  // Must check wake_fd_ready() to avoid blocking on empty socket.
  if (internal::g_wake_socket_fd >= 0 && wake_fd_ready(internal::g_wake_socket_fd)) {
    char buffer[WAKE_NOTIFY_DRAIN_BUFFER_SIZE];
    // Drain all pending notifications with non-blocking reads. Multiple wake events
    // may have triggered multiple writes, so drain until EWOULDBLOCK. We control
    // both ends of this loopback socket (always 1 byte per wake), so no error
    // checking — any error indicates catastrophic system failure.
    while (::recvfrom(internal::g_wake_socket_fd, buffer, sizeof(buffer), 0, nullptr, nullptr) > 0) {
    }
  }
}

}  // namespace esphome

#endif  // USE_HOST
