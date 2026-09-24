#include "clock_discipline.h"
#ifdef USE_TIME_DISCIPLINE
#include "esphome/core/log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>

extern "C" int __real_settimeofday(const struct timeval *tv, const struct timezone *tz);

namespace esphome::time {

static const char *const TAG = "time.discipline";
static constexpr int64_t TAI_UTC_US = 37000000;  // UTC-TAI offset since 2017; no leap announcements in legacy inputs.
static constexpr int64_t MIN_EPOCH_US = 1546300800000000;
static constexpr int64_t MAX_EPOCH_US = 4102444800000000;  // Reject corrupt legacy dates beyond 2100.
static constexpr int64_t MAX_CAPTURE_AGE_US = 5000000;
static constexpr double MAX_FREQUENCY_RATIO = 0.0002;
ClockDiscipline *global_clock_discipline = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

ClockDiscipline::ClockDiscipline(uint8_t source_count) : PollingComponent(1000), source_count_(source_count) {
  global_clock_discipline = this;
}

void ClockDiscipline::setup() {
  // ESP-IDF malloc only guarantees four-byte alignment; Rust requires eight on C6.
  this->storage_ = ::aligned_alloc(esphome_time_alignment(), esphome_time_size());
  if (this->storage_ == nullptr || reinterpret_cast<uintptr_t>(this->storage_) % esphome_time_alignment() != 0) {
    ESP_LOGE(TAG, "Unable to allocate %u bytes with %u-byte alignment for Rust",
             static_cast<unsigned>(esphome_time_size()), static_cast<unsigned>(esphome_time_alignment()));
    this->check_(ESPHOME_TIME_INVALID_ARGUMENT);
    return;
  }
  LockGuard lock(this->capture_mutex_);
  this->capture_enabled_ = true;
}

void ClockDiscipline::set_source_timeout(uint8_t source, uint32_t interval_ms) {
  if (source < 2)
    this->source_timeout_us_[source] = std::max<int64_t>(60000000, static_cast<int64_t>(interval_ms) * 3000);
}

int ClockDiscipline::capture_settimeofday(const struct timeval *tv, const struct timezone *tz) {
  LockGuard lock(this->capture_mutex_);
  if (!this->capture_enabled_)
    return __real_settimeofday(tv, tz);
  if (tv == nullptr || tv->tv_sec < MIN_EPOCH_US / 1000000 || tv->tv_sec > MAX_EPOCH_US / 1000000 ||
      tv->tv_usec < 0 || tv->tv_usec >= 1000000) {
    errno = EINVAL;
    return -1;
  }
  // Before the first observation, let legacy validity checks see the initial time.
  if (!this->intercept_writes_ && __real_settimeofday(tv, tz) != 0)
    return -1;
  this->pending_reference_us_ = static_cast<int64_t>(tv->tv_sec) * 1000000 + tv->tv_usec;
  this->pending_monotonic_us_ = esp_timer_get_time();
  this->pending_count_ = std::min<uint32_t>(2, this->pending_count_ + 1);
  return 0;
}

bool ClockDiscipline::observe_epoch(uint8_t source, uint32_t epoch) {
  if (this->error_ != 0 || this->storage_ == nullptr)
    return false;
  this->observe_(source, static_cast<int64_t>(epoch) * 1000000, esp_timer_get_time());
  return true;
}

void ClockDiscipline::on_sync(uint8_t source) {
  int64_t reference_us;
  int64_t monotonic_us;
  uint32_t count;
  {
    LockGuard lock(this->capture_mutex_);
    count = this->pending_count_;
    reference_us = this->pending_reference_us_;
    monotonic_us = this->pending_monotonic_us_;
    this->pending_count_ = 0;
  }
  if (this->error_ != 0)
    return;
  if (count != 1) {
    this->rejected_++;
    ESP_LOGW(TAG, "Ignoring sync notification without one unambiguous captured clock write");
    return;
  }
  this->observe_(source, reference_us, monotonic_us);
}

bool ClockDiscipline::observe_(uint8_t source, int64_t reference_us, int64_t monotonic_us) {
  const int64_t started = esp_timer_get_time();
  const int64_t age_us = started - monotonic_us;
  if (source >= this->source_count_ || reference_us < MIN_EPOCH_US || reference_us > MAX_EPOCH_US || age_us < 0 ||
      age_us > MAX_CAPTURE_AGE_US) {
    this->rejected_++;
    ESP_LOGW(TAG, "Ignoring invalid or stale legacy observation");
    return false;
  }
  if (!this->initialized_) {
    this->clock_.anchor(reference_us + age_us, started);
    const EsphomeTimeClock callbacks{this, clock_now_, clock_set_frequency_, clock_frequency_, clock_step_,
                                     MAX_FREQUENCY_RATIO};
    if (!this->check_(esphome_time_init(this->storage_, esphome_time_size(), callbacks, this->source_count_)))
      return false;
    this->initialized_ = true;
    {
      LockGuard lock(this->capture_mutex_);
      this->intercept_writes_ = true;
    }
    if (!this->publish_(true))
      return false;
  }
  const int64_t now = esp_timer_get_time();
  // Advance the captured reference to processing time. It never comes from our corrected output.
  const int64_t reference_ns = (reference_us + now - monotonic_us + TAI_UTC_US) * 1000;
  const int64_t local_ns = (this->clock_.at(now) + TAI_UTC_US) * 1000;
  // Legacy observations have no accuracy metadata. One second is a conservative model default,
  // not measured precision; account additionally for unknown rate during deferred delivery.
  const int64_t uncertainty_ns = 1000000000 + (now - monotonic_us) / 5;
  if (!this->check_(esphome_time_set_usable(this->storage_, source, 1)) ||
      !this->check_(esphome_time_observe(this->storage_, source, reference_ns, local_ns, uncertainty_ns)))
    return false;
  this->last_observation_us_[source] = now;
  this->fresh_sources_ |= 1U << source;
  this->observations_++;
  this->refresh_estimate_();
  this->publish_(false);
  this->update_duration_us_ = esp_timer_get_time() - started;
  this->stack_free_bytes_ = uxTaskGetStackHighWaterMark(nullptr);
  return this->error_ == 0;
}

void ClockDiscipline::update() {
  if (!this->initialized_ || this->error_ != 0)
    return;
  const int64_t now = esp_timer_get_time();
  for (uint8_t source = 0; source < this->source_count_; source++) {
    if ((this->fresh_sources_ & (1U << source)) != 0 &&
        now - this->last_observation_us_[source] > this->source_timeout_us_[source]) {
      if (!this->check_(esphome_time_set_usable(this->storage_, source, 0)))
        return;
      this->fresh_sources_ &= ~(1U << source);
    }
  }
  if (!this->check_(esphome_time_update(this->storage_)))
    return;
  this->refresh_estimate_();
  this->publish_(false);
  this->update_duration_us_ = esp_timer_get_time() - now;
  this->stack_free_bytes_ = uxTaskGetStackHighWaterMark(nullptr);
}

void ClockDiscipline::refresh_estimate_() {
  // The component's heap address may only be four-byte aligned. Use an aligned
  // stack buffer for Rust's double fields, then copy into the C++ diagnostics.
  alignas(8) EsphomeTimeEstimate estimate;
  this->has_estimate_ = this->check_(esphome_time_estimate(this->storage_, &estimate));
  if (this->has_estimate_)
    this->estimate_ = estimate;
}

bool ClockDiscipline::check_(uint32_t status) {
  if (status == ESPHOME_TIME_OK)
    return true;
  this->error_ = status;
  this->has_estimate_ = false;
  {
    LockGuard lock(this->capture_mutex_);
    this->capture_enabled_ = false;
  }
  struct timeval cancel{};
  adjtime(&cancel, nullptr);
  ESP_LOGE(TAG, "Discipline failed (%u); returning to legacy system-clock updates", static_cast<unsigned>(status));
  this->mark_failed();
  return false;
}

bool ClockDiscipline::publish_(bool step) {
  if (this->error_ != 0)
    return false;
  struct timeval value{};
  if (step) {
    const int64_t now = this->clock_.at(esp_timer_get_time());
    value.tv_sec = now / 1000000;
    value.tv_usec = now % 1000000;
    return this->check_(__real_settimeofday(&value, nullptr) == 0 ? ESPHOME_TIME_OK : ESPHOME_TIME_CLOCK_ERROR);
  }
  gettimeofday(&value, nullptr);
  const int64_t system_us = static_cast<int64_t>(value.tv_sec) * 1000000 + value.tv_usec;
  const int64_t error_us = this->clock_.at(esp_timer_get_time()) - system_us;
  // ESP-IDF slews at 500 ppm; leave headroom above the adapter's 200 ppm range.
  // Add the next second's frequency correction so it is applied between updates.
  const int64_t correction = error_us + static_cast<int64_t>(std::llround(this->clock_.get_frequency() * 1000000));
  value.tv_sec = correction / 1000000;
  value.tv_usec = correction % 1000000;
  return this->check_(adjtime(&value, nullptr) == 0 ? ESPHOME_TIME_OK : ESPHOME_TIME_CLOCK_ERROR);
}

int32_t ClockDiscipline::clock_now_(void *context, int64_t *ns) {
  auto *self = static_cast<ClockDiscipline *>(context);
  *ns = (self->clock_.at(esp_timer_get_time()) + TAI_UTC_US) * 1000;
  return 0;
}
int32_t ClockDiscipline::clock_frequency_(void *context, double *ratio) {
  *ratio = static_cast<ClockDiscipline *>(context)->clock_.get_frequency();
  return 0;
}
int32_t ClockDiscipline::clock_set_frequency_(void *context, double ratio, int64_t *ns) {
  auto *self = static_cast<ClockDiscipline *>(context);
  self->clock_.set_frequency(ratio, esp_timer_get_time());
  return clock_now_(context, ns);
}
int32_t ClockDiscipline::clock_step_(void *context, int64_t offset_ns, int64_t *ns) {
  auto *self = static_cast<ClockDiscipline *>(context);
  self->clock_.step(offset_ns / 1000, esp_timer_get_time());
  if (!self->publish_(true))
    return -1;
  return clock_now_(context, ns);
}

void ClockDiscipline::dump_config() {
  ESP_LOGCONFIG(TAG, "Statime clock discipline: %u sources, %u bytes controller storage",
                this->source_count_, static_cast<unsigned>(esphome_time_size()));
  ESP_LOGCONFIG(TAG, "  Legacy observation uncertainty: 1 s; controller update: 1 s");
}

}  // namespace esphome::time

extern "C" int __wrap_settimeofday(const struct timeval *tv, const struct timezone *tz) {
  auto *discipline = esphome::time::global_clock_discipline;
  return discipline == nullptr ? __real_settimeofday(tv, tz) : discipline->capture_settimeofday(tv, tz);
}

extern "C" void esphome_time_panic(const char *message, size_t length) {
  ESP_LOGE("time.discipline", "Rust panic: %.*s", static_cast<int>(length), message);
}
#endif
