#pragma once

#include <cmath>
#include <cstdint>

namespace esphome::time {

// Private steering model anchored to a monotonic clock. Public time remains libc time.
class DisciplineClock {
 public:
  int64_t at(int64_t monotonic_us) const {
    const int64_t elapsed = monotonic_us - this->monotonic_us_;
    return this->epoch_us_ + elapsed + static_cast<int64_t>(std::llround(elapsed * this->frequency_));
  }
  void anchor(int64_t epoch_us, int64_t monotonic_us) {
    this->epoch_us_ = epoch_us;
    this->monotonic_us_ = monotonic_us;
  }
  void set_frequency(double frequency, int64_t monotonic_us) {
    this->anchor(this->at(monotonic_us), monotonic_us);
    this->frequency_ = frequency;
  }
  void step(int64_t offset_us, int64_t monotonic_us) { this->anchor(this->at(monotonic_us) + offset_us, monotonic_us); }
  double get_frequency() const { return this->frequency_; }

 protected:
  int64_t epoch_us_{0};
  int64_t monotonic_us_{0};
  double frequency_{0};
};

}  // namespace esphome::time
