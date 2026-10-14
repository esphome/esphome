#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace rx8111 {

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
static const uint8_t RX8111_REG_SEC = 0x10;         ///< Seconds  BCD 00-59, bit7 unused
static const uint8_t RX8111_REG_MIN = 0x11;         ///< Minutes  BCD 00-59, bit7 unused
static const uint8_t RX8111_REG_HOUR = 0x12;        ///< Hours    BCD 00-23, bits7:6 unused
static const uint8_t RX8111_REG_WEEK = 0x13;        ///< Weekday  one-hot: Sun=0x01...Sat=0x40
static const uint8_t RX8111_REG_DAY = 0x14;         ///< Day      BCD 01-31, bits7:6 unused
static const uint8_t RX8111_REG_MONTH = 0x15;       ///< Month    BCD 01-12, bits7:5 unused
static const uint8_t RX8111_REG_YEAR = 0x16;        ///< Year     BCD 00-99
static const uint8_t RX8111_REG_MIN_ALARM = 0x17;   ///< Minute alarm (AE bit7)
static const uint8_t RX8111_REG_HOUR_ALARM = 0x18;  ///< Hour alarm   (AE bit7)
static const uint8_t RX8111_REG_WD_ALARM = 0x19;    ///< Week/day alarm (AE bit7, WADA selects)
static const uint8_t RX8111_REG_TIMER0 = 0x1A;      ///< Timer counter LSB
static const uint8_t RX8111_REG_TIMER1 = 0x1B;      ///< Timer counter byte1

// AE bit: when set the alarm field is disabled (active-low enable logic)
static const uint8_t RX8111_ALARM_AE = 0x80;

// ---------------------------------------------------------------------------
// FOUT frequency selection (FSEL1:FSEL0 in extension register, same encoding
// ---------------------------------------------------------------------------
static const uint8_t RX8111_EXT_FSEL_SHIFT = 6;
static const uint8_t RX8111_EXT_FSEL_MASK = 0xC0;
enum FoutFrequency : uint8_t {
  FOUT_32768_HZ = 0,  ///< 32.768 kHz
  FOUT_1024_HZ = 1,   ///< 1024 Hz
  FOUT_1_HZ = 2,      ///< 1 Hz
  FOUT_OFF = 3,       ///< Output disabled
};

// ---------------------------------------------------------------------------
// Timer source clock selection
// ---------------------------------------------------------------------------
enum TimerClock : uint8_t {
  TIMER_4096_HZ = 0,  ///< 4096 Hz
  TIMER_64_HZ = 1,    ///< 64 Hz
  TIMER_1_HZ = 2,     ///< 1 Hz
  TIMER_1_60_HZ = 3,  ///< 1/60 Hz (once per minute)
};

// ---------------------------------------------------------------------------
// EVIN level selection (EHL bit in the EVIN Setting register, RX8111CE only)
// ---------------------------------------------------------------------------
enum EventLevel : uint8_t {
  EVENT_LEVEL_LOW = 0,   ///< Trigger when EVIN is low for at least the filter period
  EVENT_LEVEL_HIGH = 1,  ///< Trigger when EVIN is high for at least the filter period
};

// ---------------------------------------------------------------------------
// RX8111 timestamp record storage mode
// ---------------------------------------------------------------------------
enum TimestampRecordMode : uint8_t {
  TIMESTAMP_RECORD_LATEST = 0,          ///< Keep only the latest event time in 0x20-0x29
  TIMESTAMP_RECORD_STOP_WHEN_FULL = 1,  ///< Store up to 8 records in RAM then stop
  TIMESTAMP_RECORD_OVERWRITE = 2,       ///< Store up to 8 records and wrap when full
};

// ---------------------------------------------------------------------------
// EVIN pull-up/pull-down selection (PDN, PU1, PU0 in EVIN Setting register 0x2B)
// ---------------------------------------------------------------------------
enum EvinPull : uint8_t {
  EVIN_PULL_NONE = 0,       ///< Hi-Z (no connection)
  EVIN_PULL_UP_500K = 1,    ///< Pull-up 500 kOhm (to VOUT)
  EVIN_PULL_UP_1M = 2,      ///< Pull-up 1 MOhm (to VOUT)
  EVIN_PULL_UP_10M = 3,     ///< Pull-up 10 MOhm (to VOUT)
  EVIN_PULL_DOWN_500K = 4,  ///< Pull-down 500 kOhm (to GND)
};

// ---------------------------------------------------------------------------
// RX8111Component
// ---------------------------------------------------------------------------
static const uint8_t RX8111_REG_EXTENSION = 0x1D;         ///< FSEL1,FSEL0,USEL,TE,WADA,ETS,TSEL1,TSEL0
static const uint8_t RX8111_REG_FLAG = 0x1E;              ///< POR,z,UF,TF,AF,EVF,VLF,XST
static const uint8_t RX8111_REG_CONTROL = 0x1F;           ///< z,z,UIE,TIE,AIE,EIE,z,STOP
static const uint8_t RX8111_REG_SEC_ALARM = 0x2C;         ///< Second alarm (AE bit7) - unique to RX8111
static const uint8_t RX8111_REG_TIMER2 = 0x1C;            ///< Timer counter MSB (24-bit timer)
static const uint8_t RX8111_REG_TIMERCTRL = 0x2D;         ///< TBKON,TBKE,TMPIN,TSTP
static const uint8_t RX8111_REG_PWRSWITCH = 0x32;         ///< CHGEN(7),INIEN(6),SWSEL1(3),SWSEL0(2),SMPT1(1),SMPT0(0)
static const uint8_t RX8111_REG_STATUS = 0x33;            ///< EVIN(6),VCMP(2),VLOW(1)
static const uint8_t RX8111_REG_TSCTRL1 = 0x34;           ///< EISEL(2),TSCLR(1),TSRAM(0)
static const uint8_t RX8111_REG_TSCTRL2 = 0x35;           ///< ECMP(3),EVDET(2),EVLOW(1),EXST(0)
static const uint8_t RX8111_REG_TSCTRL3 = 0x36;           ///< TSFULL(4),TSEMP(3),TSAD2(2),TSAD1(1),TSAD0(0)
static const uint8_t RX8111_REG_TIMESTAMP_SINGLE = 0x20;  ///< Latest event timestamp block (0x20-0x29)
static const uint8_t RX8111_REG_TIMESTAMP_MULTI = 0x40;   ///< Timestamp RAM base (0x40-0x7F)

// ---------------------------------------------------------------------------
// RX8111CE flag register (0x1E) bit masks
// ---------------------------------------------------------------------------
static const uint8_t RX8111_FLAG_POR = 0x80;  ///< Power-On Reset flag
static const uint8_t RX8111_FLAG_UF = 0x20;   ///< Time Update Flag
static const uint8_t RX8111_FLAG_TF = 0x10;   ///< Timer Flag
static const uint8_t RX8111_FLAG_AF = 0x08;   ///< Alarm Flag
static const uint8_t RX8111_FLAG_EVF = 0x04;  ///< Event (time stamp) Flag
static const uint8_t RX8111_FLAG_VLF = 0x02;  ///< Voltage Low Flag (oscillation lost)
static const uint8_t RX8111_FLAG_XST = 0x01;  ///< Crystal Oscillation Stop flag

// ---------------------------------------------------------------------------
// RX8111CE control register (0x1F) bit masks
// ---------------------------------------------------------------------------
static const uint8_t RX8111_CTRL_UIE = 0x20;   ///< Update Interrupt Enable
static const uint8_t RX8111_CTRL_TIE = 0x10;   ///< Timer Interrupt Enable
static const uint8_t RX8111_CTRL_AIE = 0x08;   ///< Alarm Interrupt Enable
static const uint8_t RX8111_CTRL_EIE = 0x04;   ///< Event Interrupt Enable
static const uint8_t RX8111_CTRL_STOP = 0x01;  ///< Stop bit - halts timekeeping

// ---------------------------------------------------------------------------
// RX8111CE extension register (0x1D) bit masks / shifts
// ---------------------------------------------------------------------------
static const uint8_t RX8111_EXT_TE = 0x10;         ///< Timer Enable
static const uint8_t RX8111_EXT_WADA = 0x08;       ///< Week/Day Alarm select (1=DAY, 0=WEEK)
static const uint8_t RX8111_EXT_ETS = 0x04;        ///< Event Time Stamp enable
static const uint8_t RX8111_EXT_TSEL_MASK = 0x03;  ///< TSEL1:TSEL0 in bits 1:0

// ---------------------------------------------------------------------------
// RX8111CE EVIN Setting register (0x2B) bit masks
// ---------------------------------------------------------------------------
static const uint8_t RX8111_REG_EVIN_SETTING = 0x2B;  ///< EHL(7),ET1(6),ET0(5),PDN(4),PU1(3),PU0(2),OVW(1),-(0)
static const uint8_t RX8111_EVIN_EHL = 0x80;          ///< EVIN High/Low level select (1=high, 0=low)
static const uint8_t RX8111_EVIN_ET_MASK = 0x60;      ///< ET1:ET0 chattering filter mask (bits 6:5)
static const uint8_t RX8111_EVIN_ET_SHIFT = 5;        ///< ET1:ET0 bit shift
static const uint8_t RX8111_EVIN_PDN = 0x10;          ///< Pull-down select
static const uint8_t RX8111_EVIN_PU1 = 0x08;          ///< Pull-up bit 1
static const uint8_t RX8111_EVIN_PU0 = 0x04;          ///< Pull-up bit 0
static const uint8_t RX8111_EVIN_OVW = 0x02;          ///< Timestamp overwrite mode

// ---------------------------------------------------------------------------
// RX8111CE timestamp control register 1 (0x34) bit masks
// ---------------------------------------------------------------------------
static const uint8_t RX8111_TSCTRL1_EISEL =
    0x04;  ///< Event Interrupt SELect (1=interrupt after 8 records, 0=interrupt on each event)
static const uint8_t RX8111_TSCTRL1_TSCLR = 0x02;  ///< Clear timestamp RAM pointer (auto-resets)
static const uint8_t RX8111_TSCTRL1_TSRAM = 0x01;  ///< Route 0x40-0x7F RAM to timestamp storage

// ---------------------------------------------------------------------------
// RX8111CE power switch register (0x32) bit masks
// ---------------------------------------------------------------------------
static const uint8_t RX8111_PWR_CHGEN = 0x80;  ///< Charge Enable
static const uint8_t RX8111_PWR_INIEN = 0x40;  ///< Power Switch (interface) Enable

// ---------------------------------------------------------------------------
// RX8111CE status monitor register (0x33) bit masks
// ---------------------------------------------------------------------------
static const uint8_t RX8111_STATUS_EVIN = 0x40;  ///< Current EVIN pin voltage level
static const uint8_t RX8111_STATUS_VCMP = 0x04;  ///< VDD comparison result
static const uint8_t RX8111_STATUS_VLOW = 0x02;  ///< VBAT low detection flag

// ---------------------------------------------------------------------------
// RX8111CE timestamp control register 3 (0x36) bit masks
// ---------------------------------------------------------------------------
static const uint8_t RX8111_TSCTRL3_TSFULL = 0x10;     ///< Timestamp RAM full
static const uint8_t RX8111_TSCTRL3_TSEMP = 0x08;      ///< Timestamp RAM empty
static const uint8_t RX8111_TSCTRL3_TSAD_MASK = 0x07;  ///< Latest timestamp RAM slot pointer

// ---------------------------------------------------------------------------

class RX8111Component : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  // ---- ESPHome component lifecycle ----------------------------------------
  void setup() override;
  void update() override;
  void dump_config() override;

  // ---- Public time API ----------------------------------------------------
  void read_time();
  void write_time();

  // ---- Configuration setters (called from Python-generated code) ----------
  void set_battery_backup(bool enable) { this->battery_backup_ = enable; }
  void set_battery_charging(bool enable) { this->battery_charging_ = enable; }
  void set_fout_frequency(FoutFrequency freq) { this->fout_frequency_ = freq; }

  // Alarm configuration
  void set_alarm_second(uint8_t sec) { this->alarm_second_ = sec; }      // 0xFF = disabled
  void set_alarm_minute(uint8_t min) { this->alarm_minute_ = min; }      // 0xFF = disabled
  void set_alarm_hour(uint8_t hour) { this->alarm_hour_ = hour; }        // 0xFF = disabled
  void set_alarm_weekday(uint8_t wday) { this->alarm_weekday_ = wday; }  // 0xFF = use day
  void set_alarm_day(uint8_t day) { this->alarm_day_ = day; }            // 0xFF = disabled
  void set_alarm_enabled(bool enable) { this->alarm_enabled_ = enable; }

  // Wake-up timer configuration
  void set_timer_count(uint32_t count) { this->timer_count_ = count; }
  void set_timer_clock(TimerClock clk) { this->timer_clock_ = clk; }
  void set_timer_enabled(bool enable) { this->timer_enabled_ = enable; }

  // EVIN event configuration
  void set_event_level(EventLevel level) { this->event_level_ = level; }
  void set_event_enabled(bool enable) { this->event_enabled_ = enable; }
  void set_evin_filter(uint8_t level) { this->evin_filter_ = level; }
  void set_evin_pull(EvinPull pull) { this->evin_pull_ = pull; }

  // Enable hardware event timestamp capture independently of EVF interrupting.
  void set_timestamp_enabled(bool enable) { this->timestamp_enabled_ = enable; }
  void set_timestamp_record_mode(TimestampRecordMode mode) { this->timestamp_record_mode_ = mode; }

  // ---- Callbacks (fire on rising-edge flag transition 0->1) ---------------
  // ESPHome's Python codegen passes a Trigger<>* for each on_alarm/on_timer
  // block; calling trigger->trigger() fires the connected automation actions.
  void add_on_alarm_callback(Trigger<> *trigger) { this->alarm_triggers_.push_back(trigger); }
  void add_on_timer_callback(Trigger<> *trigger) { this->timer_triggers_.push_back(trigger); }
  void add_on_event_callback(Trigger<> *trigger) { this->event_triggers_.push_back(trigger); }

  // ---- Explicit flag-clear actions (called from user automations) ----------
  void clear_alarm_flag() { this->clear_alarm_flag_(); }
  void clear_timer_flag() { this->clear_timer_flag_(); }
  void clear_event_flag() { this->clear_event_flag_(); }

  // ---- Runtime alarm programming -------------------------------------------
  // Sentinel values (0xFF) mean "field disabled / not specified".
  bool set_alarm_runtime(uint8_t second, uint8_t minute, uint8_t hour, uint8_t weekday, uint8_t day);
  bool unset_alarm();
  bool is_alarm_enabled() const { return this->alarm_enabled_; }

 protected:
  bool check_time_validity_();
  bool configure_battery_backup_();
  bool configure_fout_();
  bool configure_alarm_(bool enable_interrupt = true);
  bool configure_timer_();
  bool disable_alarm_();
  bool disable_timer_();
  bool configure_event_();
  bool disable_event_();
  bool apply_runtime_options_();
  bool clear_alarm_flag_();
  bool clear_timer_flag_();
  bool clear_event_flag_();

  // ---- Shared helpers (non-virtual) ----------------------------------------
  static uint8_t bcd_to_uint8_(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
  static uint8_t uint8_to_bcd_(uint8_t val) { return ((val / 10) << 4) | (val % 10); }
  static uint8_t weekday_to_onehot_(uint8_t dow) { return static_cast<uint8_t>(1u << (dow - 1)); }
  static uint8_t onehot_to_weekday_(uint8_t onehot) {
    if (onehot == 0 || onehot > 0x40 || (onehot & (onehot - 1)) != 0)
      return 0;
    return static_cast<uint8_t>(__builtin_ctz(onehot & 0x7F) + 1);
  }

  /// Read-modify-write the STOP bit in the control register.
  bool set_stop_bit_(bool stop);

  bool timestamp_enabled_{false};
  TimestampRecordMode timestamp_record_mode_{TIMESTAMP_RECORD_LATEST};
  bool timestamp_capture_active_() const;
  bool buffered_timestamp_mode_() const;
  bool write_timestamp_mode_bits_();
  bool clear_timestamp_ram_();
  // ---- Configuration state -------------------------------------------------
  bool battery_backup_{false};
  bool battery_charging_{false};
  FoutFrequency fout_frequency_{FoutFrequency::FOUT_OFF};

  bool alarm_enabled_{false};
  uint8_t alarm_second_{0xFF};
  uint8_t alarm_minute_{0xFF};
  uint8_t alarm_hour_{0xFF};
  uint8_t alarm_weekday_{0xFF};
  uint8_t alarm_day_{0xFF};

  bool timer_enabled_{false};
  uint32_t timer_count_{0};
  TimerClock timer_clock_{TimerClock::TIMER_1_HZ};

  bool event_enabled_{false};
  EventLevel event_level_{EVENT_LEVEL_LOW};
  uint8_t evin_filter_{0};
  EvinPull evin_pull_{EVIN_PULL_NONE};

  // ---- Edge-detection state (to fire callbacks only on 0->1 transition) ----
  bool prev_alarm_flag_{false};
  bool prev_timer_flag_{false};
  bool prev_event_flag_{false};

  // ---- Automation triggers -------------------------------------------------
  std::vector<Trigger<> *> alarm_triggers_;
  std::vector<Trigger<> *> timer_triggers_;
  std::vector<Trigger<> *> event_triggers_;
};

// ---------------------------------------------------------------------------
// Automation action templates — time read/write
// ---------------------------------------------------------------------------
class AlarmTrigger final : public Trigger<> {};
class TimerTrigger final : public Trigger<> {};
class EventTrigger final : public Trigger<> {};

template<typename... Ts> class SetAlarmAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  template<typename V> void set_second(V second) {
    this->has_second_ = true;
    this->second_ = second;
  }
  template<typename V> void set_minute(V minute) {
    this->has_minute_ = true;
    this->minute_ = minute;
  }
  template<typename V> void set_hour(V hour) {
    this->has_hour_ = true;
    this->hour_ = hour;
  }
  template<typename V> void set_weekday(V weekday) {
    this->has_weekday_ = true;
    this->weekday_ = weekday;
  }
  template<typename V> void set_day(V day) {
    this->has_day_ = true;
    this->day_ = day;
  }

  void play(const Ts &...x) override {
    uint8_t second = 0xFF;
    uint8_t minute = 0xFF;
    uint8_t hour = 0xFF;
    uint8_t weekday = 0xFF;
    uint8_t day = 0xFF;

    if (this->has_second_ && !this->validate_alarm_field_("second", this->second_.value(x...), 0, 59, &second))
      return;
    if (this->has_minute_ && !this->validate_alarm_field_("minute", this->minute_.value(x...), 0, 59, &minute))
      return;
    if (this->has_hour_ && !this->validate_alarm_field_("hour", this->hour_.value(x...), 0, 23, &hour))
      return;
    if (this->has_weekday_ && !this->validate_alarm_field_("weekday", this->weekday_.value(x...), 1, 7, &weekday))
      return;
    if (this->has_day_ && !this->validate_alarm_field_("day", this->day_.value(x...), 1, 31, &day))
      return;

    this->parent_->set_alarm_runtime(second, minute, hour, weekday, day);
  }

 protected:
  bool validate_alarm_field_(const char *field, uint16_t value, uint8_t min_value, uint8_t max_value, uint8_t *out) {
    if (value < min_value || value > max_value) {
      ESP_LOGE("rx8111", "set_alarm: %s must be %u-%u, got %u", field, static_cast<unsigned>(min_value),
               static_cast<unsigned>(max_value), static_cast<unsigned>(value));
      return false;
    }
    *out = static_cast<uint8_t>(value);
    return true;
  }

  bool has_second_{false};
  TemplatableValue<uint16_t, Ts...> second_{};
  bool has_minute_{false};
  TemplatableValue<uint16_t, Ts...> minute_{};
  bool has_hour_{false};
  TemplatableValue<uint16_t, Ts...> hour_{};
  bool has_weekday_{false};
  TemplatableValue<uint16_t, Ts...> weekday_{};
  bool has_day_{false};
  TemplatableValue<uint16_t, Ts...> day_{};
};

template<typename... Ts> class UnsetAlarmAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void play(const Ts &...x) override { this->parent_->unset_alarm(); }
};

template<typename... Ts> class ScheduleAlarmInAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void set_seconds(uint32_t seconds) { this->seconds_ = seconds; }

  void play(const Ts &...x) override {
    if (this->seconds_ == 0) {
      ESP_LOGE("rx8111", "schedule_alarm_in: seconds must be > 0");
      return;
    }
    auto *parent = this->parent_;
    if (!parent->is_alarm_enabled()) {
      ESP_LOGE("rx8111", "schedule_alarm_in: alarm is disabled "
                         "(set alarm_enabled: true in the time component configuration)");
      return;
    }
    auto now = parent->utcnow();
    if (!now.is_valid()) {
      ESP_LOGE("rx8111", "schedule_alarm_in: current time is invalid");
      return;
    }
    time_t target_epoch = now.timestamp + this->seconds_;
    auto target = ESPTime::from_epoch_utc(target_epoch);
    if (!target.is_valid()) {
      ESP_LOGE("rx8111", "schedule_alarm_in: computed target time is invalid");
      return;
    }
    // Program all available fields from the target time — identical to set_alarm.
    uint8_t second = static_cast<uint8_t>(target.second);
    ESP_LOGI("rx8111",
             "schedule_alarm_in: now=%04d-%02d-%02d %02d:%02d:%02d UTC, +%lu s "
             "-> target=%04d-%02d-%02d %02d:%02d:%02d UTC",
             now.year, now.month, now.day_of_month, now.hour, now.minute, now.second,
             static_cast<unsigned long>(this->seconds_), target.year, target.month, target.day_of_month, target.hour,
             target.minute, target.second);
    parent->set_alarm_runtime(second, static_cast<uint8_t>(target.minute), static_cast<uint8_t>(target.hour),
                              0xFF,  // weekday not used for relative alarms
                              static_cast<uint8_t>(target.day_of_month));
  }

 protected:
  uint32_t seconds_{0};
};

template<typename... Ts> class WriteAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void play(const Ts &...x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void play(const Ts &...x) override { this->parent_->read_time(); }
};

// ---------------------------------------------------------------------------
// Automation action templates — explicit flag clear
// ---------------------------------------------------------------------------
template<typename... Ts> class ClearAlarmFlagAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_alarm_flag(); }
};

template<typename... Ts> class ClearTimerFlagAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_timer_flag(); }
};

template<typename... Ts> class ClearEventFlagAction final : public Action<Ts...>, public Parented<RX8111Component> {
 public:
  void play(const Ts &...x) override { this->parent_->clear_event_flag(); }
};

}  // namespace rx8111
}  // namespace esphome
