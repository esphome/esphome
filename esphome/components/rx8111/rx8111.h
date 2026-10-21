#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome::rx8111 {

class RX8111Component final : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void read_time();
  void write_time();
  void set_alarm_enabled(bool enabled) { this->alarm_enabled_ = enabled; }
  void set_alarm_configured(bool configured) { this->alarm_explicitly_configured_ = configured; }
  void set_alarm_second(uint8_t value) { this->alarm_second_ = value; }
  void set_alarm_minute(uint8_t value) { this->alarm_minute_ = value; }
  void set_alarm_hour(uint8_t value) { this->alarm_hour_ = value; }
  void set_alarm_weekday(uint8_t value) { this->alarm_weekday_ = value; }
  void set_alarm_day(uint8_t value) { this->alarm_day_ = value; }
  void set_timer_count(uint32_t count) { this->timer_count_ = count; }
  void set_timer_clock(uint8_t clock) { this->timer_clock_ = clock; }
  void set_timer_enabled(bool enabled) { this->timer_enabled_ = enabled; }
  void set_timer_configured(bool configured) { this->timer_configured_ = configured; }
  void set_event_poll_interval(uint32_t interval) { this->event_poll_interval_ = interval; }
  template<typename F> void add_on_alarm_callback(F &&callback) {
    this->alarm_callbacks_.add(std::forward<F>(callback));
  }
  bool set_alarm(uint16_t second, uint16_t minute, uint16_t hour, uint16_t weekday, uint16_t day);
  bool schedule_alarm_in(uint32_t seconds);
  bool unset_alarm();
  bool clear_alarm_flag();
  bool clear_timer_flag();
  bool start_timer();
  bool stop_timer();
  template<typename F> void add_on_timer_callback(F &&callback) {
    this->timer_callbacks_.add(std::forward<F>(callback));
  }

 protected:
  bool set_stop_bit_(bool stop);
  bool read_calendar_(ESPTime &rtc_time);
  void set_calendar_warning_(const LogString *message);
  void check_event_flags_();
  bool program_alarm_(uint8_t second, uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day);
  bool set_alarm_interrupt_(bool enabled);
  bool configure_timer_(bool clear_stale_flag = false);
  void start_event_polling_();
  bool poll_event_flags_();

  LazyCallbackManager<void()> alarm_callbacks_;
  LazyCallbackManager<void()> timer_callbacks_;
  uint32_t timer_count_{0};
  uint32_t event_poll_interval_{1000};
  uint8_t timer_clock_{2};  // 1 Hz
  bool timer_enabled_{false};
  bool timer_configured_{false};
  bool timer_flag_seen_{false};
  bool timer_retained_event_pending_{false};
  bool event_polling_started_{false};
  bool event_poll_warning_{false};
  bool calendar_warning_{false};
  uint8_t alarm_second_{0xFF};
  uint8_t alarm_minute_{0xFF};
  uint8_t alarm_hour_{0xFF};
  uint8_t alarm_weekday_{0xFF};
  uint8_t alarm_day_{0xFF};
  bool alarm_enabled_{false};
  bool alarm_explicitly_configured_{false};
  bool alarm_flag_seen_{false};
  bool alarm_configured_{false};
};

}  // namespace esphome::rx8111
