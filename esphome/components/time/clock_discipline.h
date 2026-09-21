#pragma once

#include "esphome/core/defines.h"
#ifdef USE_TIME_DISCIPLINE
#include "discipline_clock.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "time_discipline.h"
#include <array>
#include <sys/time.h>

namespace esphome::time {

class ClockDiscipline final : public PollingComponent {
 public:
  explicit ClockDiscipline(uint8_t source_count);
  void setup() override;
  void update() override;
  void dump_config() override;
  void set_source_timeout(uint8_t source, uint32_t interval_ms);
  bool observe_epoch(uint8_t source, uint32_t epoch);
  void on_sync(uint8_t source);
  int capture_settimeofday(const struct timeval *tv, const struct timezone *tz);

  bool has_estimate() const { return this->has_estimate_; }
  bool is_holdover() const { return this->initialized_ && this->fresh_sources_ == 0; }
  uint32_t get_error() const { return this->error_; }
  uint32_t get_observation_count() const { return this->observations_; }
  uint32_t get_rejected_count() const { return this->rejected_; }
  uint32_t get_active_sources() const { return this->estimate_.active_sources; }
  double get_offset_seconds() const { return this->estimate_.offset_seconds; }
  double get_frequency_ppm() const { return this->clock_.get_frequency() * 1e6; }
  double get_uncertainty_seconds() const { return this->estimate_.dispersion_seconds; }
  uint32_t get_update_duration_us() const { return this->update_duration_us_; }
  uint32_t get_stack_free_bytes() const { return this->stack_free_bytes_; }

 protected:
  bool observe_(uint8_t source, int64_t reference_us, int64_t monotonic_us);
  bool check_(uint32_t status);
  void refresh_estimate_();
  bool publish_(bool step);
  static int32_t clock_now_(void *context, int64_t *ns);
  static int32_t clock_frequency_(void *context, double *ratio);
  static int32_t clock_set_frequency_(void *context, double ratio, int64_t *ns);
  static int32_t clock_step_(void *context, int64_t offset_ns, int64_t *ns);

  DisciplineClock clock_;
  void *storage_{nullptr};
  EsphomeTimeEstimate estimate_{};
  uint8_t source_count_;
  uint32_t error_{0};
  uint32_t observations_{0};
  uint32_t rejected_{0};
  uint32_t fresh_sources_{0};
  uint32_t update_duration_us_{0};
  uint32_t stack_free_bytes_{0};
  std::array<int64_t, 2> last_observation_us_{};
  std::array<int64_t, 2> source_timeout_us_{};
  bool initialized_{false};
  bool has_estimate_{false};

 private:
  // Capture is called by network tasks; only this mailbox crosses threads.
  Mutex capture_mutex_;
  int64_t pending_reference_us_{0};
  int64_t pending_monotonic_us_{0};
  uint32_t pending_count_{0};
  bool capture_enabled_{false};
  bool intercept_writes_{false};
};

extern ClockDiscipline *global_clock_discipline;

}  // namespace esphome::time
#endif
