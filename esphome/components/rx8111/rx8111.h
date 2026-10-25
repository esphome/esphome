#pragma once

#include "esphome/core/helpers.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::rx8111 {

namespace testing {
class RX8111Test;
}  // namespace testing

class RX8111Component final : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void read_time();
  void write_time();
  void set_alarm_enabled(bool enabled) { this->alarm_enabled_ = enabled; }
  void set_alarm_explicitly_configured(bool configured) { this->alarm_explicitly_configured_ = configured; }
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
  void set_evin_settings(uint8_t settings) { this->evin_settings_ = settings; }
#ifdef USE_TEXT_SENSOR
  void set_event_timestamp_sensor(text_sensor::TextSensor *sensor, uint8_t mode) {
    this->timestamp_sensor_ = sensor;
    this->timestamp_mode_ = mode;
    this->event_configured_ = true;
  }
#endif
  void set_battery_backup(bool enabled, bool charging) {
    this->battery_configured_ = true;
    this->battery_backup_ = enabled;
    this->battery_charging_ = charging;
  }
  void set_fout_frequency(uint8_t frequency) {
    this->fout_configured_ = true;
    this->fout_frequency_ = frequency;
  }
  void set_event_config(uint8_t settings) {
    this->event_configured_ = true;
    this->event_enabled_ = true;
    this->evin_settings_ = settings;
  }
  void set_event_disabled() {
    this->event_configured_ = true;
    this->event_enabled_ = false;
  }
  template<typename F> void add_on_event_callback(F &&callback) {
    this->event_callbacks_.add(std::forward<F>(callback));
  }
  bool clear_event_flag();
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
  friend class testing::RX8111Test;

  static constexpr uint32_t BOOT_RETRY_DELAY_MS = 1000;
  // set_alarm() and the boot alarm ignore a field set to this value
  static constexpr uint8_t ALARM_FIELD_UNSET = 0xFF;

  // One bit per fault source, so a recovery in one path cannot hide another path's fault
  enum Warning : uint16_t {
    WARN_CALENDAR = 1 << 0,
    WARN_EVENT_POLL = 1 << 1,
    WARN_ALARM = 1 << 2,
    WARN_TIMER = 1 << 3,
    WARN_WRITE_TIME = 1 << 4,
    WARN_ALARM_FLAG_CLEAR = 1 << 5,
    WARN_TIMER_FLAG_CLEAR = 1 << 6,
    WARN_BATTERY = 1 << 7,
    WARN_FOUT = 1 << 8,
    WARN_EVENT = 1 << 9,
    WARN_EVENT_FLAG_CLEAR = 1 << 10,
    WARN_TIMESTAMP = 1 << 11,
  };

  bool set_stop_bit_(bool stop);
  bool finish_time_write_(bool calendar_written);
  bool read_calendar_(ESPTime &rtc_time);
  void set_warning_(uint16_t bit, const LogString *message);
  void clear_warning_(uint16_t bit);
  void configure_boot_battery_();
  void configure_boot_fout_();
  void retry_fout_after_timer_start_();
  enum BootFeature : uint8_t { BOOT_BATTERY, BOOT_FOUT, BOOT_EVENT, BOOT_ALARM, BOOT_TIMER, BOOT_FEATURE_COUNT };
  void boot_config_result_(BootFeature feature, bool success);
  bool watching_() const {
#ifdef USE_TEXT_SENSOR
    if (this->timestamp_sensor_ != nullptr && !this->event_boot_pending_)
      return true;
#endif
    return this->alarm_armed_ || this->timer_enabled_ || this->timer_retained_event_pending_ ||
           (this->event_enabled_ && !this->event_boot_pending_ && !this->event_callbacks_.empty());
  }
  void configure_boot_event_();
  bool configure_event_();
#ifdef USE_TEXT_SENSOR
  bool publish_timestamps_(uint8_t flags, bool buffered);
  void publish_timestamp_(const uint8_t *raw, bool buffered);
  text_sensor::TextSensor *timestamp_sensor_{nullptr};
  uint8_t last_timestamp_[10]{};
  uint8_t timestamp_mode_{0};
  bool timestamp_seen_ : 1 {false};
  bool retained_inspected_ : 1 {false};
  bool capture_restore_pending_ : 1 {false};
#endif
  void check_event_flags_();
  bool program_alarm_(uint8_t second, uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day, bool boot = false);
  bool set_alarm_interrupt_(bool enabled);
  bool disable_timer_(uint8_t &extension, uint8_t &control);
  bool configure_timer_(bool clear_stale_flag = false);
  void start_event_polling_();
  bool poll_event_flags_();
  void configure_boot_alarm_();
  void configure_boot_timer_();
  void cancel_boot_retry_(bool alarm);

  LazyCallbackManager<void()> alarm_callbacks_;
  LazyCallbackManager<void()> timer_callbacks_;
  LazyCallbackManager<void()> event_callbacks_;
  uint32_t timer_count_{0};
  uint32_t event_poll_interval_{1000};
  uint32_t boot_retry_delay_ms_{BOOT_RETRY_DELAY_MS};
  uint16_t warnings_{0};
  uint8_t boot_attempts_[BOOT_FEATURE_COUNT]{};
  uint8_t evin_settings_{0};
  uint8_t fout_frequency_{3};
  uint8_t timer_clock_{2};  // 1 Hz
  uint8_t alarm_second_{ALARM_FIELD_UNSET};
  uint8_t alarm_minute_{ALARM_FIELD_UNSET};
  uint8_t alarm_hour_{ALARM_FIELD_UNSET};
  uint8_t alarm_weekday_{ALARM_FIELD_UNSET};
  uint8_t alarm_day_{ALARM_FIELD_UNSET};
  bool time_write_invalid_ : 1 {false};
  bool battery_configured_ : 1 {false};
  bool battery_backup_ : 1 {false};
  bool battery_charging_ : 1 {false};
  bool fout_configured_ : 1 {false};
  bool event_configured_ : 1 {false};
  bool event_enabled_ : 1 {false};
  bool event_flag_seen_ : 1 {false};
  bool event_boot_pending_ : 1 {false};
  bool timer_enabled_ : 1 {false};
  bool timer_configured_ : 1 {false};
  bool timer_flag_seen_ : 1 {false};
  bool timer_retained_event_pending_ : 1 {false};
  bool event_polling_started_ : 1 {false};
  bool boot_alarm_event_seen_ : 1 {false};
  bool alarm_enabled_ : 1 {false};
  bool alarm_explicitly_configured_ : 1 {false};
  bool alarm_flag_seen_ : 1 {false};
  bool alarm_armed_ : 1 {false};
};

}  // namespace esphome::rx8111
