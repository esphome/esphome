#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/time.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::ds3231 {

/// When an alarm fires. Alarm 1 supports every mode, alarm 2 has no seconds register and so no EVERY_SECOND.
enum AlarmMode : uint8_t {
  ALARM_MODE_EVERY_SECOND = 0,  ///< Alarm 1 only.
  ALARM_MODE_EVERY_MINUTE,      ///< Alarm 1: when the seconds match. Alarm 2: at second 00 of every minute.
  ALARM_MODE_HOURLY,            ///< When minutes (and seconds) match.
  ALARM_MODE_DAILY,             ///< When hours, minutes (and seconds) match.
  ALARM_MODE_MONTHLY,           ///< When date, hours, minutes (and seconds) match.
  ALARM_MODE_WEEKLY,            ///< When day of week, hours, minutes (and seconds) match.
};

/// Frequency of the INT/SQW pin when it is a square-wave output (RS2:RS1 in the control register).
enum SquareWaveFrequency : uint8_t {
  SQUARE_WAVE_1HZ = 0,
  SQUARE_WAVE_1024HZ = 1,
  SQUARE_WAVE_4096HZ = 2,
  SQUARE_WAVE_8192HZ = 3,
};

/// Hub for a single DS3231 real-time clock. Owns all register access; the platforms talk to the
/// chip through this class.
class DS3231Component final : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // ---- Configuration (set from codegen) ----
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_square_wave_frequency(SquareWaveFrequency frequency) {
    this->square_wave_frequency_ = frequency;
    this->square_wave_configured_ = true;
  }
  void set_battery_backed_square_wave(bool enabled) { this->battery_backed_square_wave_ = enabled ? 1 : 0; }
  void set_output_32khz_config(bool enabled) { this->output_32khz_ = enabled ? 1 : 0; }
  void set_aging_offset_config(int8_t offset) {
    this->aging_offset_ = offset;
    this->aging_offset_configured_ = true;
  }
  void set_auto_clear_alarms(bool auto_clear) { this->auto_clear_alarms_ = auto_clear; }
  void set_alarm_check_interval(uint32_t interval) { this->alarm_check_interval_ = interval; }
  void set_monitor_alarms(bool monitor) { this->monitor_alarms_ = monitor; }

  Trigger<> *get_alarm_1_trigger() { return &this->alarm_1_trigger_; }
  Trigger<> *get_alarm_2_trigger() { return &this->alarm_2_trigger_; }

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(alarm_1)
  SUB_BINARY_SENSOR(alarm_2)
  SUB_BINARY_SENSOR(oscillator_stopped)
#endif

  // ---- Clock registers (used by the time platform) ----
  /// Read the calendar registers (UTC). Fails if the oscillator was stopped since the time was last set.
  bool read_time(ESPTime &out);
  /// Write @p time (UTC) to the calendar registers and clear the oscillator-stop flag.
  bool write_time(const ESPTime &time);

  // ---- Temperature ----
  bool read_temperature(float &out);
  void force_temperature_conversion();

  // ---- Alarms ----
  /// Program alarm 1. @p day_of_month is used in MONTHLY mode, @p day_of_week (1 = Sunday) in WEEKLY mode.
  void set_alarm_1(AlarmMode mode, uint8_t second, uint8_t minute, uint8_t hour, uint8_t day_of_month,
                   uint8_t day_of_week, bool enabled);
  /// Program alarm 2 (minute resolution, fires at second 00).
  void set_alarm_2(AlarmMode mode, uint8_t minute, uint8_t hour, uint8_t day_of_month, uint8_t day_of_week,
                   bool enabled);
  /// Enable @p alarm. A match from while it was disabled is cleared first, so only the next one fires.
  void enable_alarm(uint8_t alarm);
  void disable_alarm(uint8_t alarm) { this->set_alarm_enabled_(alarm, false); }
  /// Clear the alarm flag, which also releases the INT/SQW line.
  void clear_alarm(uint8_t alarm);
  /// True while the alarm flag of @p alarm (1 or 2) is set on the chip.
  bool is_alarm_triggered(uint8_t alarm);

  // ---- Outputs and calibration ----
  /// Switch INT/SQW to a square-wave output at @p frequency. Alarms then no longer drive the pin.
  void enable_square_wave(SquareWaveFrequency frequency);
  /// Switch INT/SQW back to the (active-low) alarm interrupt output.
  void disable_square_wave();
  void set_output_32khz(bool enabled);
  /// Aging offset in LSB (about 0.1 ppm each at 25 degC); positive values slow the clock down.
  void set_aging_offset(int8_t offset);

 protected:
  static constexpr uint8_t CONTROL_A1IE = 0x01;
  static constexpr uint8_t CONTROL_A2IE = 0x02;
  static constexpr uint8_t CONTROL_INTCN = 0x04;
  static constexpr uint8_t CONTROL_RS_MASK = 0x18;
  static constexpr uint8_t CONTROL_RS_SHIFT = 3;
  static constexpr uint8_t CONTROL_CONV = 0x20;
  static constexpr uint8_t CONTROL_BBSQW = 0x40;
  static constexpr uint8_t CONTROL_EOSC = 0x80;

  static constexpr uint8_t STATUS_A1F = 0x01;
  static constexpr uint8_t STATUS_A2F = 0x02;
  static constexpr uint8_t STATUS_BSY = 0x04;
  static constexpr uint8_t STATUS_EN32KHZ = 0x08;
  static constexpr uint8_t STATUS_OSF = 0x80;
  static constexpr uint8_t STATUS_FLAGS = STATUS_A1F | STATUS_A2F | STATUS_OSF;

  /// Scheduler ids: one pulse timer per alarm (id + alarm - 1).
  static constexpr uint32_t ALARM_PULSE_TIMEOUT_ID = 0;

  static void IRAM_ATTR interrupt_handler(DS3231Component *arg);

  /// Read the control and status registers into the cached copies. Sets the warning flag on failure.
  bool read_control_status_();
  /// Replace the bits in @p mask of the control register with @p value, on the chip and in the cache.
  bool update_control_(uint8_t mask, uint8_t value);
  /// Write the status register, clearing the flags in @p clear (writing 1 to a flag leaves it unchanged).
  bool write_status_(uint8_t clear);
  bool write_aging_offset_(int8_t offset);
  /// Clear the flag of @p alarm and report it off. Returns false (state unchanged) if that fails.
  bool clear_alarm_flag_(uint8_t alarm);
  void set_alarm_enabled_(uint8_t alarm, bool enabled);
  /// Write the alarm registers, clear the stale flag and set the interrupt enable.
  void write_alarm_(uint8_t alarm, const uint8_t *regs, size_t len, bool enabled);
  /// Read the flags, fire triggers and update the binary sensors.
  void check_alarms_();
  void publish_alarm_state_(uint8_t alarm, bool state);
  void publish_oscillator_stopped_();

  InternalGPIOPin *interrupt_pin_{nullptr};
  Trigger<> alarm_1_trigger_;
  Trigger<> alarm_2_trigger_;

  uint32_t alarm_check_interval_{1000};
  uint8_t control_reg_{0};
  uint8_t status_reg_{0};
  int8_t aging_offset_{0};
  SquareWaveFrequency square_wave_frequency_{SQUARE_WAVE_1HZ};
  /// -1: leave the chip as it is, 0: off, 1: on.
  int8_t battery_backed_square_wave_{-1};
  int8_t output_32khz_{-1};
  bool square_wave_configured_{false};
  bool aging_offset_configured_{false};
  bool auto_clear_alarms_{true};
  bool monitor_alarms_{false};
  /// Set while an alarm flag has been reported, so a latched flag fires its trigger only once.
  bool alarm_active_[2]{false, false};
};

}  // namespace esphome::ds3231
