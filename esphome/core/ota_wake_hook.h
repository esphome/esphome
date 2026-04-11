#pragma once

// Inline OTA wake hook, called from lwip_fast_select.c on every NETCONN_EVT_RCVPLUS so a
// disabled OTA loop can be re-enabled when a monitored socket signals activity.
//
// Defined as a static inline here (rather than an out-of-line extern "C" shim into
// application.cpp) so the fast-select callback pays zero function-call overhead per
// socket event: the two volatile stores below are cheaper inlined than dispatched.
//
// The two pointers are set once in Application::set_ota_wake_component() to the addresses
// of Component::pending_enable_loop_ and Application::has_pending_enable_loop_requests_.
// Accessing those C++ members by raw address is safe: Application is a friend of Component
// (granting access at registration time), and volatile writes through a bool* see the same
// storage the C++ side reads.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Address of the registered OTA component's pending_enable_loop_ flag. NULL until
// Application::set_ota_wake_component() is called. When non-NULL, the has-pending
// pointer below is also non-NULL, so a single null check covers both.
extern volatile bool *esphome_ota_pending_enable_loop_ptr;
// Address of Application::has_pending_enable_loop_requests_. Set in tandem with the
// pending_enable pointer above.
extern volatile bool *esphome_ota_has_pending_requests_ptr;

// Mark the registered OTA component pending loop-enable. Safe to call from LwIP TCP/IP
// task context and raw-TCP IRQ context — only writes to volatile bools, no locks.
// Callers must invoke this BEFORE waking the main task, so the flags are visible to the
// main loop's next iteration.
static inline void esphome_wake_ota_component_any_context(void) {
  volatile bool *pending_enable = esphome_ota_pending_enable_loop_ptr;
  if (pending_enable != NULL) {
    *pending_enable = true;
    *esphome_ota_has_pending_requests_ptr = true;
  }
}

#ifdef __cplusplus
}
#endif
