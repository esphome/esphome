#ifdef USE_HOST

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <time.h>
#include <cerrno>
#include <cstdlib>

// Empty host namespace block to satisfy ci-custom's lint_namespace check.
// HAL functions live in namespace esphome (root) — they are not part of the
// host component's API.
namespace esphome::host {}  // namespace esphome::host

namespace esphome {

// yield(), arch_init(), arch_feed_wdt(), arch_get_cpu_freq_hz() inlined in
// core/hal/hal_host.h.

uint32_t IRAM_ATTR HOT millis() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  return static_cast<uint32_t>(spec.tv_sec * 1000ULL + spec.tv_nsec / 1000000);
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
  return static_cast<uint32_t>(spec.tv_sec * 1000000ULL + spec.tv_nsec / 1000);
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

uint32_t arch_get_cpu_cycle_count() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  time_t seconds = spec.tv_sec;
  uint32_t ns = static_cast<uint32_t>(spec.tv_nsec);
  return static_cast<uint32_t>(seconds) * 1000000000U + ns;
}

}  // namespace esphome

#endif  // USE_HOST
