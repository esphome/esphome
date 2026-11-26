#pragma once
// Re-enumerate USB from the main loop: a bench stimulus for the host's handling of the NCM data interface.
#include <USB.h>
#include <tusb.h>
inline void hammer_reenumerate() {
  mutex_enter_blocking(&USB.mutex);
  tud_disconnect();
  mutex_exit(&USB.mutex);
  delay(500);
  mutex_enter_blocking(&USB.mutex);
  tud_connect();
  mutex_exit(&USB.mutex);
}
