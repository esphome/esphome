#include "esphome/components/time/clock_discipline.h"
#include "esphome/components/time/real_time_clock.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace {
int64_t monotonic_us;
int64_t system_us;
int64_t pending_adjustment_us;
int64_t reference_us = 1700000000000000;
double drift;

void advance() {
  reference_us += 1000000;
  const int64_t ticks = std::llround(1000000 * (1.0 + drift));
  monotonic_us += ticks;
  const int64_t correction = std::clamp<int64_t>(pending_adjustment_us, -ticks / 2000, ticks / 2000);
  pending_adjustment_us -= correction;
  system_us += ticks + correction;
}
}

int64_t esp_timer_get_time() { return monotonic_us; }
extern "C" int gettimeofday(struct timeval *tv, void *) noexcept(noexcept(::gettimeofday(nullptr, nullptr))) {
  tv->tv_sec = system_us / 1000000;
  tv->tv_usec = system_us % 1000000;
  return 0;
}
extern "C" time_t time(time_t *output) noexcept(noexcept(::time(nullptr))) {
  const time_t result = system_us / 1000000;
  if (output != nullptr)
    *output = result;
  return result;
}
extern "C" int __real_settimeofday(const struct timeval *tv, const struct timezone *) {
  system_us = tv->tv_sec * 1000000LL + tv->tv_usec;
  pending_adjustment_us = 0;
  return 0;
}
extern "C" int __wrap_settimeofday(const struct timeval *, const struct timezone *);
extern "C" int settimeofday(const struct timeval *tv, const struct timezone *tz) noexcept(noexcept(::settimeofday(nullptr, nullptr))) {
  return __wrap_settimeofday(tv, tz);
}
extern "C" int adjtime(const struct timeval *delta, struct timeval *old) noexcept(noexcept(::adjtime(nullptr, nullptr))) {
  if (old != nullptr) {
    old->tv_sec = pending_adjustment_us / 1000000;
    old->tv_usec = pending_adjustment_us % 1000000;
  }
  if (delta != nullptr)
    pending_adjustment_us = delta->tv_sec * 1000000LL + delta->tv_usec;
  return 0;
}

class LegacySource : public esphome::time::RealTimeClock {
 public:
  void epoch(uint32_t epoch) { this->synchronize_epoch_(epoch); }
  void notify() { this->time_sync_callback_.call(); }
};

int main(int argc, char **argv) {
  assert(argc == 2);
  drift = std::strtod(argv[1], nullptr) * 1e-6;
  esphome::time::ClockDiscipline discipline(2);
  LegacySource direct;
  LegacySource external;
  direct.set_discipline(&discipline, 0);
  external.set_discipline(&discipline, 1);
  discipline.set_source_timeout(0, 900000);
  discipline.set_source_timeout(1, 900000);
  unsigned notifications = 0;
  external.add_on_time_sync_callback([&]() { notifications++; });
  discipline.setup();
  direct.epoch(reference_us / 1000000);
  assert(discipline.get_observation_count() == 1);  // Common setter is not counted twice.
  assert(direct.timestamp_now() == reference_us / 1000000);
  for (unsigned second = 1; second <= 172800; second++) {
    advance();
    if (second % 900 == 0) {
      timeval tv{static_cast<time_t>(reference_us / 1000000), static_cast<suseconds_t>(reference_us % 1000000)};
      const int64_t before = system_us;
      assert(settimeofday(&tv, nullptr) == 0);
      assert(system_us == before);  // External updates become observations, not raw clock steps.
      external.notify();
    }
    discipline.update();
    assert(discipline.get_error() == 0);
  }
  assert(notifications == 192);
  assert(discipline.get_observation_count() == 193);
  assert(discipline.has_estimate());
  assert(std::abs(discipline.get_frequency_ppm() + drift * 1e6) < 5);
  const int64_t start_error = system_us - reference_us;
  for (unsigned second = 0; second < 21600; second++) {
    advance();
    discipline.update();
    assert(discipline.get_error() == 0);
  }
  assert(discipline.is_holdover());
  const double added_error = std::abs(system_us - reference_us - start_error) / 1e6;
  std::printf("drift=%+.0f ppm correction=%+.3f ppm holdover error=%.6f s free=%.3f s\n",
              drift * 1e6, discipline.get_frequency_ppm(), added_error, std::abs(drift) * 21600);
  assert(added_error < std::abs(drift) * 21600 / 2);
  assert(external.timestamp_now() == system_us / 1000000);  // Existing public consumers use the corrected clock.
  timeval tv{static_cast<time_t>(reference_us / 1000000), static_cast<suseconds_t>(reference_us % 1000000)};
  assert(settimeofday(&tv, nullptr) == 0);
  external.notify();
  assert(!discipline.is_holdover());
  assert(discipline.get_error() == 0);
  assert(discipline.get_observation_count() == 194);
  external.notify();  // A callback alone must not feed corrected output back into the estimator.
  assert(discipline.get_observation_count() == 194);
  assert(discipline.get_rejected_count() == 1);
  assert(settimeofday(&tv, nullptr) == 0);
  assert(settimeofday(&tv, nullptr) == 0);
  external.notify();  // Ambiguous source attribution is rejected.
  assert(discipline.get_rejected_count() == 2);
  assert(discipline.get_observation_count() == 194);
  tv.tv_sec += 100;
  assert(settimeofday(&tv, nullptr) == 0);
  assert(std::abs(system_us - reference_us) < 1000000);
  advance();
  external.notify();
  assert(discipline.get_error() == 0);
  assert(std::abs(system_us - reference_us) < 1000000);  // Outlier is not blindly applied.
}
