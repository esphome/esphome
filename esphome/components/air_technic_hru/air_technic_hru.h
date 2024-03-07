#pragma once

#include "esphome/core/component.h"
#include "esphome/components/modbus/modbus.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/select/select.h"
#include "esphome/components/fan/fan.h"

#include <vector>
#include <queue>

namespace esphome {
namespace air_technic_hru {

class AirTechnicHru;

class AirTechnicHRUSwitch : public switch_::Switch, public Component {
 public:
  void setup() override {}
  void dump_config() override;

  void set_parent(AirTechnicHru *parent) { this->parent_ = parent; }
  void set_id(const char *id) { this->id_ = id; }

 protected:
  void write_state(bool state) override;

  AirTechnicHru *parent_;
  std::string id_;
};

class AirTechnicHRUNumber : public number::Number, public Component {
 public:
  void setup() override {}
  void dump_config() override;

  void set_parent(AirTechnicHru *parent) { this->parent_ = parent; }
  void set_id(const char *id) { this->id_ = id; }

 protected:
  void control(float value) override;

  AirTechnicHru *parent_;
  std::string id_;
};

class AirTechnicHRUButton : public button::Button, public Component {
 public:
  void setup() override {}
  void dump_config() override;

  void set_parent(AirTechnicHru *parent) { this->parent_ = parent; }
  void set_id(const char *id) { this->id_ = id; }

 protected:
  void press_action() override;

  AirTechnicHru *parent_;
  std::string id_;
};

class AirTechnicHRUSelect : public select::Select, public Component {
 public:
  void setup() override {}
  void dump_config() override;

  void set_parent(AirTechnicHru *parent) { this->parent_ = parent; }
  void set_id(const char *id) { this->id_ = id; }

 protected:
  void control(const std::string &value) override;

  AirTechnicHru *parent_;
  std::string id_;
};

class AirTechnicHru : public fan::Fan, public PollingComponent, public modbus::ModbusDevice {
 public:
  AirTechnicHru(int speed_count) : fan_speed_count_(speed_count) {}

  void setup() override {}

  void update() override;
  void on_modbus_data(const std::vector<uint8_t> &data) override;

  void dump_config() override;

  void set_room_temperature_sensor(sensor::Sensor *sensor) { room_temperature_sensor_ = sensor; }
  void set_outdoor_temperature_sensor(sensor::Sensor *sensor) { outdoor_temperature_sensor_ = sensor; }
  void set_supply_air_temperature_sensor(sensor::Sensor *sensor) { supply_air_temperature_sensor_ = sensor; }
  void set_defrost_temperature_sensor(sensor::Sensor *sensor) { defrost_temperature_sensor_ = sensor; }
  void set_co2_sensor(sensor::Sensor *sensor) { co2_sensor_ = sensor; }
  void set_filter_hour_count_sensor(sensor::Sensor *sensor) { filter_hour_count_sensor_ = sensor; }
  void set_supply_fan_speed_sensor(sensor::Sensor *sensor) { supply_fan_speed_sensor_ = sensor; }
  void set_exhaust_fan_speed_sensor(sensor::Sensor *sensor) { exhaust_fan_speed_sensor_ = sensor; }

  void set_fire_alarm_sensor(binary_sensor::BinarySensor *sensor) { fire_alarm_sensor_ = sensor; }
  void set_bypass_open_sensor(binary_sensor::BinarySensor *sensor) { bypass_open_sensor_ = sensor; }
  void set_defrosting_sensor(binary_sensor::BinarySensor *sensor) { defrosting_sensor_ = sensor; }

  void set_power_restore_switch(AirTechnicHRUSwitch *sw) { power_restore_switch_ = sw; }
  void set_heater_installed_switch(AirTechnicHRUSwitch *sw) { heater_installed_switch = sw; }
  void set_co2_sensor_installed_switch(AirTechnicHRUSwitch *sw) { co2_sensor_installed_ = sw; }
  void set_external_control_switch(AirTechnicHRUSwitch *sw) { external_control_switch_ = sw; }

  void set_bypass_open_temperature_number(AirTechnicHRUNumber *num) { bypass_open_temperature_number_ = num; }
  void set_bypass_temperature_range_number(AirTechnicHRUNumber *num) { bypass_temperature_range_number_ = num; }
  void set_defrost_interval_time_number(AirTechnicHRUNumber *num) { defrost_interval_time_number_ = num; }
  void set_defrost_duration_number(AirTechnicHRUNumber *num) { defrost_duration_number_ = num; }
  void set_defrost_start_temperature_number(AirTechnicHRUNumber *num) { defrost_start_temperature_number_ = num; }
  void set_supply_exhaust_ratio_number(AirTechnicHRUNumber *num) { supply_exhaust_ratio_number_ = num; }

  void set_filter_alarm_interval_select(AirTechnicHRUSelect *sel) { filter_alarm_interval_select_ = sel; }

  void toggle_switch(const std::string &id, bool state);
  void set_number(const std::string &id, float value);
  void press_button(const std::string &id);
  void set_select(const std::string &id, const std::string &option);

  fan::FanTraits get_traits() override { return fan::FanTraits(false, true, false, fan_speed_count_); }

 protected:
  sensor::Sensor *room_temperature_sensor_{nullptr};
  sensor::Sensor *outdoor_temperature_sensor_{nullptr};
  sensor::Sensor *supply_air_temperature_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *defrost_temperature_sensor_{nullptr};
  sensor::Sensor *filter_hour_count_sensor_{nullptr};
  sensor::Sensor *supply_fan_speed_sensor_{nullptr};
  sensor::Sensor *exhaust_fan_speed_sensor_{nullptr};

  binary_sensor::BinarySensor *fire_alarm_sensor_{nullptr};
  binary_sensor::BinarySensor *bypass_open_sensor_{nullptr};
  binary_sensor::BinarySensor *defrosting_sensor_{nullptr};

  AirTechnicHRUSwitch *power_restore_switch_{nullptr};
  AirTechnicHRUSwitch *heater_installed_switch{nullptr};
  AirTechnicHRUSwitch *co2_sensor_installed_{nullptr};
  AirTechnicHRUSwitch *external_control_switch_{nullptr};

  AirTechnicHRUNumber *bypass_open_temperature_number_{nullptr};
  AirTechnicHRUNumber *bypass_temperature_range_number_{nullptr};
  AirTechnicHRUNumber *defrost_interval_time_number_{nullptr};
  AirTechnicHRUNumber *defrost_duration_number_{nullptr};
  AirTechnicHRUNumber *defrost_start_temperature_number_{nullptr};
  AirTechnicHRUNumber *supply_exhaust_ratio_number_{nullptr};

  AirTechnicHRUSelect *filter_alarm_interval_select_{nullptr};

  const int fan_speed_count_{14};

  uint16_t supply_fan_speed_{};
  uint16_t exhaust_fan_speed_{};
  float supply_exhaust_ratio_{};

  enum class Register : uint16_t {
    PowerRestore = 0,
    HeaterValidOrInvalid = 1,
    BypassOpenTemperature = 2,
    BypassOpenTemperatureRange = 3,
    DefrostingIntervalTime = 4,
    DefrostingEnteringTemperature = 5,
    DefrostingDurationTime = 6,
    OnOff = 9,
    SupplyFanSpeed = 10,
    ExhaustFanSpeed = 11,
    RoomTemperature = 12,
    OutdoorTemperature = 13,
    SupplyAirTemperature = 14,
    DefrostingTemperature = 15,
    ExternalOnOffSignal = 16,
    CO2SensorOnOffSignal = 17,
    Flags = 18,
    MultifunctionSettings = 24,
    FilterAlarmTimer = 25,
    HeaterTemperatureThreshold = 27,
    CO2Value = 768,
    FanHourCount = 769,
  };

  void control(const fan::FanCall &call) override;

  std::queue<Register> modbus_send_queue{};

  void read_next_register();
  void modbus_write_bool(Register reg, bool val);
  void modbus_write_uint16(Register reg, uint16_t val);

  void clear_modbus_send_queue();

  float data_to_temperature(const std::vector<uint8_t> &data);
  uint16_t data_to_uint16(const std::vector<uint8_t> &data);
  bool data_to_bool(const std::vector<uint8_t> &data);

  void process_flag_data(const std::vector<uint8_t> &data);

  uint16_t temperature_to_uint16(float temperature);

  void calculate_supply_exhaust_ratio();
  void calculate_target_supply_exhaust_speeds();

  void process_register_queue(const std::vector<uint8_t> &data);
};

}  // namespace air_technic_hru
}  // namespace esphome
