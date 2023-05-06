#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/button/button.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"

#include <vector>

namespace esphome {
namespace micronova {

static const char *const TAG = "micronova";
static const int STOVE_REPLY_DELAY = 50;

enum class MicroNovaFunctions {
  STOVE_FUNCTION_VOID = 0,
  STOVE_FUNCTION_SWITCH = 1,
  STOVE_FUNCTION_TEMP_UP = 2,
  STOVE_FUNCTION_TEMP_DOWN = 3,
  STOVE_FUNCTION_ROOM_TEMPERATURE = 4,
  STOVE_FUNCTION_THERMOSTAT_TEMPERATURE = 5,
  STOVE_FUNCTION_FUMES_TEMPERATURE = 6,
  STOVE_FUNCTION_STOVE_POWER = 7,
  STOVE_FUNCTION_FAN_SPEED = 8,
  STOVE_FUNCTION_STOVE_STATE = 9,
  STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR = 10,
};

class MicroNova;

///////////////////////////////////////////////////////////////////////////////
// Base MicroNova function class. Stove functions (buttons, switches, sensors,.. )
// Are derived from this
class MicroNovaFunction {
 public:
  MicroNovaFunction(MicroNova *m) { micronova_ = m; }
  virtual void dump_config();
  virtual void read_value_from_stove();
  void set_micronova_object(MicroNova *m) { micronova_ = m; }

  void set_function(MicroNovaFunctions f) { function_ = f; }
  MicroNovaFunctions get_function() { return function_; }

  void set_memory_location(uint8_t f) { memory_location_ = f; }
  uint8_t get_memory_location() { return memory_location_; }

  void set_memory_address(uint8_t f) { memory_address_ = f; }
  uint8_t get_memory_address() { return memory_address_; }

  uint8_t get_current_data() { return current_data_; }

 protected:
  MicroNova *micronova_{nullptr};
  MicroNovaFunctions function_ = MicroNovaFunctions::STOVE_FUNCTION_VOID;
  uint8_t memory_location_ = 0;
  uint8_t memory_address_ = 0;
  float current_data_ = 0;
};

///////////////////////////////////////////////////////////////////////////////
// MicroNovaSensor
class MicroNovaSensor : public sensor::Sensor, public MicroNovaFunction {
 public:
  MicroNovaSensor(MicroNova *m) : MicroNovaFunction(m) {}
  void dump_config() override { LOG_SENSOR("", "Micronova sensor", this); }
  void read_value_from_stove() override;

  void set_fan_speed_offset(uint8_t f) { fan_speed_offset_ = f; }
  uint8_t get_set_fan_speed_offset() { return fan_speed_offset_; }

 protected:
  int fan_speed_offset_ = 0;
};

///////////////////////////////////////////////////////////////////////////////
// MicroNovaTextSensor
class MicroNovaTextSensor : public text_sensor::TextSensor, public MicroNovaFunction {
 public:
  MicroNovaTextSensor(MicroNova *m) : MicroNovaFunction(m) {}
  void dump_config() override { LOG_TEXT_SENSOR("", "Micronova text sensor", this); }
  void read_value_from_stove() override;
};

///////////////////////////////////////////////////////////////////////////////
// MicroNovaButton
class MicroNovaButton : public Component, public button::Button, public MicroNovaFunction {
 public:
  MicroNovaButton(MicroNova *m) : MicroNovaFunction(m) {}
  void dump_config() override { LOG_BUTTON("", "Micronova button", this); }
  void read_value_from_stove() override{};

  void set_memory_data(uint8_t f) { memory_data_ = f; }
  uint8_t get_memory_data() { return memory_data_; }

 protected:
  void press_action() override;
  uint8_t memory_data_ = 0;
};

///////////////////////////////////////////////////////////////////////////////
// MicroNovaSwitch
class MicroNovaSwitch : public Component, public switch_::Switch, public MicroNovaFunction {
 public:
  MicroNovaSwitch(MicroNova *m) : MicroNovaFunction(m) {}
  void dump_config() override { LOG_SWITCH("", "Micronova switch", this); }
  void read_value_from_stove() override{};

  void set_memory_data_on(uint8_t f) { memory_data_on_ = f; }
  uint8_t get_memory_data_on() { return memory_data_on_; }

  void set_memory_data_off(uint8_t f) { memory_data_off_ = f; }
  uint8_t get_memory_data_off() { return memory_data_off_; }

 protected:
  void write_state(bool state) override;
  uint8_t memory_data_on_ = 0;
  uint8_t memory_data_off_ = 0;
};

///////////////////////////////////////////////////////////////////////////////
// Main MicroNova class.
class MicroNova : public PollingComponent, public uart::UARTDevice {
 public:
  MicroNova(uart::UARTComponent *uart) : uart::UARTDevice(uart) {}

  void setup() override;
  void update() override;
  void dump_config() override;

  int read_address(uint8_t addr, uint8_t reg);
  void write_address(uint8_t location, uint8_t address, uint8_t data);

  void set_enable_rx_pin(GPIOPin *enable_rx_pin) { this->enable_rx_pin_ = enable_rx_pin; }
  void set_scan_memory_location(uint8_t m) { this->scan_memory_location_ = m; }

  void set_current_stove_state(uint8_t s) { current_stove_state_ = s; }
  uint8_t get_current_stove_state() { return current_stove_state_; }

  void set_thermostat_temperature(uint8_t t) { current_thermostat_temperature_ = t; }
  uint8_t get_thermostat_temperature() { return current_thermostat_temperature_; }

  void set_temp_up_button(MicroNovaButton *b) { temp_up_button_ = b; };
  void set_temp_down_button(MicroNovaButton *b) { temp_down_button_ = b; };
  void set_stove_switch(MicroNovaSwitch *s) { stove_switch_ = s; };
  MicroNovaSwitch *get_stove_switch() { return stove_switch_; }

  void add_sensor(MicroNovaFunction *s) { micronova_sensors_.push_back(s); }

 protected:
  uint8_t current_stove_state_ = 0;
  uint8_t current_thermostat_temperature_ = 20;

  GPIOPin *enable_rx_pin_{nullptr};
  int scan_memory_location_ = -1;

  std::vector<MicroNovaFunction *> micronova_sensors_;

  MicroNovaButton *temp_up_button_{nullptr};
  MicroNovaButton *temp_down_button_{nullptr};
  MicroNovaSwitch *stove_switch_{nullptr};
};
}  // namespace micronova
}  // namespace esphome
