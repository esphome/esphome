#ifdef USE_HOST

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "preferences.h"

#include <csignal>
#include <sched.h>
#include <time.h>
#include <cmath>
#include <cstdlib>

namespace esphome {

void HOT yield() { ::sched_yield(); }
uint32_t IRAM_ATTR HOT millis() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  time_t seconds = spec.tv_sec;
  uint32_t ms = round(spec.tv_nsec / 1e6);
  return ((uint32_t) seconds) * 1000U + ms;
}
void HOT delay(uint32_t ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  int res;
  do {
    res = nanosleep(&ts, &ts);
  } while (res != 0 && errno == EINTR);
}
uint32_t IRAM_ATTR HOT micros() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  time_t seconds = spec.tv_sec;
  uint32_t us = round(spec.tv_nsec / 1e3);
  return ((uint32_t) seconds) * 1000000U + us;
}
void IRAM_ATTR HOT delayMicroseconds(uint32_t us) {
  struct timespec ts;
  ts.tv_sec = us / 1000000U;
  ts.tv_nsec = (us % 1000000U) * 1000U;
  int res;
  do {
    res = nanosleep(&ts, &ts);
  } while (res != 0 && errno == EINTR);
}
void arch_restart() { exit(0); }
void arch_init() {
  // pass
}
void HOT arch_feed_wdt() {
  // pass
}

uint8_t progmem_read_byte(const uint8_t *addr) { return *addr; }
uint32_t arch_get_cpu_cycle_count() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  time_t seconds = spec.tv_sec;
  uint32_t us = spec.tv_nsec;
  return ((uint32_t) seconds) * 1000000000U + us;
}
uint32_t arch_get_cpu_freq_hz() { return 1000000000U; }

}  // namespace esphome

void setup();
void loop();
namespace {
// Flag toggled by SIGTERM/SIGINT to stop the main loop gracefully.
volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int) { shutdown_requested = 1; }

void register_signal_handlers() {
  // Allow systemd (or a user) to terminate the process cleanly.
  struct sigaction action {};
  action.sa_handler = signal_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;
  sigaction(SIGTERM, &action, nullptr);
  sigaction(SIGINT, &action, nullptr);

  // Ignore SIGPIPE so socket write errors don't kill the process abruptly.
  struct sigaction ignore_action {};
  ignore_action.sa_handler = SIG_IGN;
  sigemptyset(&ignore_action.sa_mask);
  ignore_action.sa_flags = 0;
  sigaction(SIGPIPE, &ignore_action, nullptr);
}
}  // namespace

int main() {
  register_signal_handlers();
  esphome::host::setup_preferences();
  setup();
  // Exit the loop when shutdown has been requested.
  while (!shutdown_requested) {
    loop();
  }
  // Give components a chance to shut down cleanly before exit.
  esphome::App.run_safe_shutdown_hooks();
  esphome::App.teardown_components(esphome::TEARDOWN_TIMEOUT_REBOOT_MS);
  esphome::App.run_powerdown_hooks();
  return 0;
}

#endif  // USE_HOST
