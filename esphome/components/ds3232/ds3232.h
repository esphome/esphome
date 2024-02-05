#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/automation.h"
#include "ds3232_alarm.h"
namespace esphome {
namespace ds3232 {

/// @brief Power state of DS3232 chip
enum DS3232PowerState {
  /// @brief Power state is unknown, i.e. rst_pin is not connected.
  UNKNOWN = 0b00,

  /// @brief Vcc >= Vbattery, i.e. power is online.
  ONLINE = 0b01,

  /// @brief Vbat >= Vcc, i.e. no supplied power provided, battery is used.
  BATTERY = 0b10
};


/// @brief State of NVRAM functionality
enum DS3232NVRAMState {
  /// @brief State is unknown
  UNCERTAIN = 0b0000,

  /// @brief NVRAM should be initialized before usage.
  NEED_INITIALIZATION = 0b0001,

  /// @brief NVRAM initialization process in progress.
  INITIALIZATION = 0b0010,

  /// @brief NVRAM should be initialized with variables default values.
  NEED_RESET = 0b0100,

  /// @brief NVRAM is in normal state.
  OK = 0b1000,

  /// @brief NVRAM found but its state is faulty.
  FAIL = 0b1111
};

/// @brief Basic register to store service information
static const uint8_t SVC_NVRAM_ADDRESS = 0x14;

/// @brief Minimal number of NVRAM register available to user variables.
static const uint8_t MIN_NVRAM_ADDRESS = 0x18;

/// @brief Maximal number of NVRAM register available to user variables.
static const uint8_t MAX_NVRAM_ADDRESS = 0xFF;

/// @brief Magic value 1 to control validity of NVRAM.
static const uint8_t MAGIX_CONTROL_1 = 0b00000011;
/// @brief Magic value 2 to control validity of NVRAM.
static const uint8_t MAGIX_CONTROL_2 = 0b00101101;

/// @brief Major version of NVRAM storage logic
static const uint8_t NVRAM_DATA_MAJ_VERSION = 0;
/// @brief Minor version of NVRAM storage logic
static const uint8_t NVRAM_DATA_MIN_VERSION = 1;
/// @brief NVRAM storage version
static const uint8_t NVRAM_DATA_VERSION = NVRAM_DATA_MAJ_VERSION * 10 + NVRAM_DATA_MIN_VERSION;

/// @brief Component that implements ds3232mz+ crystal capabilities
class DS3232Component : public time::RealTimeClock, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;
  /// @brief Gets current state of NVRAM functionality.
  /// @return One of values from DS3232NVRAMState
  /// @see DS3232NVRAMState
  DS3232NVRAMState get_nvram_state() { return this->busy_ ? DS3232NVRAMState::UNCERTAIN : this->nvram_state_; }
  void read_time();             // Action
  void write_time();            // Action
  bool is_heartbeat_enabled();  // Condition
  void enable_heartbeat();      // Action
  void enable_alarms();         // Action
  /// @brief Perform full memory reinitialization. All available
  /// memory registers will be set to zeroes.
  /// This could help if memory structure is corrupted or faulty.
  void reset_memory();  // Action
  /// @brief Sets all 'configuration' variables to their initial values.
  void reset_to_factory();                                     // Action
  void set_alarm_one(const ds3232_alarm::DS3232Alarm &alarm);  // Action
  void set_alarm_two(const ds3232_alarm::DS3232Alarm &alarm);  // Action
  ds3232_alarm::DS3232Alarm get_alarm_one();
  ds3232_alarm::DS3232Alarm get_alarm_two();
  void clear_alarm_one();
  void clear_alarm_two();
  /// @brief Gets power source state.
  /// @return One of values from DS3232PowerState
  DS3232PowerState get_power_state();
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_interrupt_pin(GPIOPin *pin) { int_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { rst_pin_ = pin; }
  void set_fire_alarms_on_startup(bool value) { fire_alarms_on_startup_ = value; }
  void add_on_alarm_one_callback(std::function<void(ESPTime)> &&func) {
    this->alarm_one_callback_.add(std::move(func));
  }
  void add_on_alarm_two_callback(std::function<void(ESPTime)> &&func) {
    this->alarm_two_callback_.add(std::move(func));
  }
  void add_on_heartbeat_callback(std::function<void()> &&func) { this->heartbeat_callback_.add(std::move(func)); }
  void add_on_power_change_callback(std::function<void(DS3232PowerState)> &&func) {
    this->power_state_callback_.add(std::move(func));
  }
  void add_on_variable_init_required_callback(std::function<void()> &&func) {
    this->variable_init_callback_.add(std::move(func));
  }
  void add_on_variable_fail_callback(std::function<void()> &&func) {
    this->variable_fail_callback_.add(std::move(func));
  }
  bool read_memory(uint8_t reg_id, std::vector<uint8_t> &data);
  bool write_memory(uint8_t reg_id, const std::vector<uint8_t> &data);
  bool is_valid_nvram(const uint8_t reg_id, uint8_t size) { return this->validate_mem_(reg_id, size, true); }

 protected:
  void reinit_osf_();
  bool read_data_();
  void clear_nvram_();
  void plan_clear_nvram_();
  void plan_reset_nvram_();
  bool fire_alarms_on_startup_{false};
  void read_temperature_();
  void process_interrupt_();
  void process_alarms_();
  bool pin_state_{false};
  bool alarm_1_firing_{false};
  bool alarm_2_firing_{false};
  bool nvram_available_{false};
  bool late_startup_{true};
  DS3232NVRAMState nvram_state_{DS3232NVRAMState::UNCERTAIN};
  bool validate_mem_(uint8_t reg_id, uint8_t size, bool ignore_empty = false);
  ESPTime reg_to_time_();
  bool produce_stable_frequency_{false};
  bool busy_{false};
  sensor::Sensor *temperature_sensor_{nullptr};
  GPIOPin *int_pin_{nullptr};
  GPIOPin *rst_pin_{nullptr};
  CallbackManager<void(ESPTime)> alarm_one_callback_{};
  CallbackManager<void(ESPTime)> alarm_two_callback_{};
  CallbackManager<void()> heartbeat_callback_{};
  CallbackManager<void(DS3232PowerState)> power_state_callback_{};
  CallbackManager<void()> variable_init_callback_{};
  CallbackManager<void()> variable_fail_callback_{};
  Deduplicator<DS3232PowerState> power_state_;
  Deduplicator<float> temperature_state_;

  union DS3232NVRAMData {
    struct {
      uint8_t magix_1 : 2;      // Always should be 0b11;
      uint8_t magix_2 : 6;      // Always should be 'E' char.
      bool is_initialized : 1;  // If set to 1 then it means that nvram has initialized values.
      uint8_t min_version : 3;
      uint8_t maj_version : 4;
    } info;

    mutable uint8_t raw[sizeof(info)];
  } nvram_info_;

  union DS3232RegData {
    struct {
      // RTC regs (0x00)
      uint8_t second : 4;
      uint8_t second_10 : 3;
      uint8_t unused_0 : 1;

      uint8_t minute : 4;
      uint8_t minute_10 : 3;
      uint8_t unused_1 : 1;

      uint8_t hour : 4;
      uint8_t hour_10 : 2;
      uint8_t unused_2 : 2;  // Always store in 24h mode

      uint8_t weekday : 3;
      uint8_t unused_3 : 5;

      uint8_t day : 4;
      uint8_t day_10 : 2;
      uint8_t unused_4 : 2;

      uint8_t month : 4;
      uint8_t month_10 : 1;
      uint8_t unused_5 : 2;
      uint8_t century : 1;

      uint8_t year : 4;
      uint8_t year_10 : 4;

      // Alarm 1 regs (0x07)
      uint8_t alarm_1_second : 4;
      uint8_t alarm_1_second_10 : 3;
      bool alarm_1_mode_1 : 1;

      uint8_t alarm_1_minute : 4;
      uint8_t alarm_1_minute_10 : 3;
      bool alarm_1_mode_2 : 1;

      uint8_t alarm_1_hour : 4;
      uint8_t alarm_1_hour_10 : 2;
      uint8_t unused_6 : 1;  // Always store in 24h mode
      bool alarm_1_mode_3 : 1;

      uint8_t alarm_1_day : 4;
      uint8_t alarm_1_day_10 : 2;
      bool alarm_1_use_day_of_week : 1;
      bool alarm_1_mode_4 : 1;

      // Alarm 2 regs (0x0B)
      uint8_t alarm_2_minute : 4;
      uint8_t alarm_2_minute_10 : 3;
      bool alarm_2_mode_2 : 1;

      uint8_t alarm_2_hour : 4;
      uint8_t alarm_2_hour_10 : 2;
      uint8_t unused_7 : 1;  // Always store in 24h mode
      bool alarm_2_mode_3 : 1;

      uint8_t alarm_2_day : 4;
      uint8_t alarm_2_day_10 : 2;
      bool alarm_2_use_day_of_week : 1;
      bool alarm_2_mode_4 : 1;

      // Control register (0x0E)
      bool alarm_1_enable : 1;
      bool alarm_2_enable : 1;
      bool int_enable : 1;
      uint8_t unused_8 : 2;
      bool convert_temp : 1;
      bool enable_battery_square_signal : 1;
      bool EOSC : 1;

      // Status register (0x0F)
      bool alarm_1_match : 1;
      bool alarm_2_match : 1;
      bool device_busy : 1;
      bool enable_hf_output : 1;
      uint8_t unused_9 : 2;
      bool enable_hf_output_on_battery_mode : 1;
      bool osf_bit : 1;

      // Aging register (0x10)
      uint8_t unused_10 : 8;

      // Temperature data (read-only, 0x11)
      uint8_t temperature_integral : 8;

      uint8_t unused_11 : 6;
      uint8_t temperature_fractional : 2;

    } reg;
    mutable struct {
      mutable uint8_t rtc_raw[7];
      mutable uint8_t alarm_1_raw[4];
      mutable uint8_t alarm_2_raw[3];
      mutable uint8_t control_raw[1];
      mutable uint8_t status_raw[1];
      mutable uint8_t aging_raw[1];
      mutable uint8_t temperature_raw[2];
    } raw_blocks;
    mutable uint8_t raw[sizeof(reg)];
  } reg_data_;
};

class HeartbeatEvent : public Trigger<> {
 public:
  explicit HeartbeatEvent(DS3232Component *parent) {
    parent->add_on_heartbeat_callback([this]() { this->trigger(); });
  }
};

class AlarmFiredEvent : public Trigger<ESPTime> {
 public:
  explicit AlarmFiredEvent(DS3232Component *parent, bool is_first = true) {
    if (is_first) {
      parent->add_on_alarm_one_callback([this](ESPTime fire_time) { this->trigger(fire_time); });
    } else {
      parent->add_on_alarm_two_callback([this](ESPTime fire_time) { this->trigger(fire_time); });
    };
  };
};

class PowerStateEvent : public Trigger<DS3232PowerState> {
 public:
  explicit PowerStateEvent(DS3232Component *parent) {
    parent->add_on_power_change_callback([this](DS3232PowerState power_state) { this->trigger(power_state); });
  }
};

template<typename... Ts> class FactoryResetNVRAMAction : public Action<Ts...>, public Parented<DS3232Component> {
 public:
  void play(Ts... x) override { this->parent_->reset_to_factory(); }
};

template<typename... Ts> class EraseNVRAMAction : public Action<Ts...>, public Parented<DS3232Component> {
 public:
  void play(Ts... x) override { this->parent_->reset_memory(); }
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<DS3232Component> {
 public:
  void play(Ts... x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<DS3232Component> {
 public:
  void play(Ts... x) override { this->parent_->read_time(); }
};

template<typename... Ts> class EnableHeartbeatAction : public Action<Ts...>, public Parented<DS3232Component> {
 public:
  void play(Ts... x) override { this->parent_->enable_heartbeat(); }
};

template<typename... Ts> class EnableAlarmsAction : public Action<Ts...>, public Parented<DS3232Component> {
 public:
  void play(Ts... x) override { this->parent_->enable_alarms(); }
};

template<typename... Ts> class SetAlarmAction : public Action<Ts...> {
 public:
  SetAlarmAction(DS3232Component *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(ds3232_alarm::DS3232Alarm, alarm_data)
  TEMPLATABLE_VALUE(uint8_t, alarm_id)
  void play(Ts... x) {
    if (!alarm_data_.has_value())
      return;
    switch (alarm_id_.value(x...)) {
      case 1:
        this->parent_->set_alarm_one(alarm_data_.value(x...));
      case 2:
        this->parent_->set_alarm_two(alarm_data_.value(x...));
      default:
        break;
    }
  }

 protected:
  DS3232Component *parent_;
};

template<typename... Ts> class IsHeartbeatEnabledCondition : public Condition<Ts...>, public Parented<DS3232Component> {
 public:
  bool check(Ts... x) override { return this->parent_->is_heartbeat_enabled(); }
};

}  // namespace ds3232
}  // namespace esphome
