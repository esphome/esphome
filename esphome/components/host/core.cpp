#ifdef USE_HOST

#include "esphome/core/application.h"
#include "preferences.h"

#include <csignal>

namespace {
volatile sig_atomic_t s_signal_received = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
void signal_handler(int signal) { s_signal_received = signal; }
}  // namespace

// HAL functions live in hal.cpp.

void setup();
void loop();
int main() {
  // Install signal handlers for graceful shutdown (flushes preferences to disk)
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  esphome::host::setup_preferences();
  setup();
  while (s_signal_received == 0) {
    loop();
  }
  esphome::App.run_safe_shutdown_hooks();
  return 0;
}

#endif  // USE_HOST
