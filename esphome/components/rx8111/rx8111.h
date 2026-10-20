#pragma once

#include "esphome/core/automation.h"
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
  void set_alarm_second(uint8_t value) { this->alarm_second_ = value; }
  void set_alarm_minute(uint8_t value) { this->alarm_minute_ = value; }
  void set_alarm_hour(uint8_t value) { this->alarm_hour_ = value; }
  void set_alarm_weekday(uint8_t value) { this->alarm_weekday_ = value; }
  void set_alarm_day(uint8_t value) { this->alarm_day_ = value; }
  void set_timer_count(uint32_t count) { this->timer_count_ = count; }
  void set_timer_clock(uint8_t clock) { this->timer_clock_ = clock; }
  void set_timer_enabled(bool enabled) { this->timer_enabled_ = enabled; }
  template<typename F> void add_on_alarm_callback(F &&callback) {
    this->alarm_callbacks_.add(std::forward<F>(callback));
  }
  bool set_alarm(uint16_t second, uint16_t minute, uint16_t hour, uint16_t weekday, uint16_t day);
  bool schedule_alarm_in(uint32_t seconds);
  bool unset_alarm();
  bool clear_alarm_flag();
  bool clear_timer_flag();
  template<typename F> void add_on_timer_callback(F &&callback) {
    this->timer_callbacks_.add(std::forward<F>(callback));
  }

 protected:
  bool set_stop_bit_(bool stop);
  bool program_alarm_(uint8_t second, uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day);
  bool set_alarm_interrupt_(bool enabled);
  bool poll_alarm_flag_();
  bool configure_timer_();
  bool poll_timer_flag_();

  LazyCallbackManager<void()> alarm_callbacks_;
  LazyCallbackManager<void()> timer_callbacks_;
  uint32_t timer_count_{0};
  uint8_t timer_clock_{2};  // 1 Hz
  bool timer_enabled_{false};
  bool timer_flag_seen_{false};
  bool timer_flag_initialized_{false};
  uint8_t alarm_second_{0xFF};
  uint8_t alarm_minute_{0xFF};
  uint8_t alarm_hour_{0xFF};
  uint8_t alarm_weekday_{0xFF};
  uint8_t alarm_day_{0xFF};
  bool alarm_enabled_{false};
  bool alarm_flag_seen_{false};
  bool alarm_flag_initialized_{false};
  bool alarm_configured_{false};
};

template<typename... Ts> class SetAlarmAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  template<typename V> void set_second(V value) { this->second_ = value; }
  template<typename V> void set_minute(V value) { this->minute_ = value; }
  template<typename V> void set_hour(V value) { this->hour_ = value; }
  template<typename V> void set_weekday(V value) { this->weekday_ = value; }
  template<typename V> void set_day(V value) { this->day_ = value; }

  void play(const Ts &...x) override {
    this->parent_->set_alarm(this->second_.value_or(x..., 0xFF), this->minute_.value_or(x..., 0xFF),
                             this->hour_.value_or(x..., 0xFF), this->weekday_.value_or(x..., 0xFF),
                             this->day_.value_or(x..., 0xFF));
  }

 protected:
  TemplatableStorage<uint16_t, Ts...> second_{};
  TemplatableStorage<uint16_t, Ts...> minute_{};
  TemplatableStorage<uint16_t, Ts...> hour_{};
  TemplatableStorage<uint16_t, Ts...> weekday_{};
  TemplatableStorage<uint16_t, Ts...> day_{};
};

template<typename... Ts> class ScheduleAlarmInAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void set_seconds(uint32_t seconds) { this->seconds_ = seconds; }
  void play(const Ts &...x) override { this->parent_->schedule_alarm_in(this->seconds_); }

 protected:
  uint32_t seconds_{0};
};

}  // namespace esphome::rx8111
