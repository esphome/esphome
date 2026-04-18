#ifdef USE_HOST

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "preferences.h"

#include <csignal>
#include <sched.h>
#include <time.h>
#include <cmath>
#include <cstdlib>

namespace {
volatile sig_atomic_t s_signal_received = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
void signal_handler(int signal) { s_signal_received = signal; }
}  // namespace

namespace esphome {

void HOT yield() { ::sched_yield(); }
uint32_t IRAM_ATTR HOT millis() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  time_t seconds = spec.tv_sec;
  uint32_t ms = round(spec.tv_nsec / 1e6);
  return ((uint32_t) seconds) * 1000U + ms;
}
uint64_t millis_64() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  return static_cast<uint64_t>(spec.tv_sec) * 1000ULL + static_cast<uint64_t>(spec.tv_nsec) / 1000000ULL;
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
