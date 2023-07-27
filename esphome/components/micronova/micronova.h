#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"

#include <vector>

namespace esphome {
namespace micronova {

static const char *const TAG = "micronova";
static const int STOVE_REPLY_DELAY = 60;

static const std::string STOVE_STATES[11] = {"Off",
                                             "Start",
                                             "Pellets loading",
                                             "Igniton",
                                             "Working",
                                             "Brazier Cleaning",
                                             "Final Cleaning",
                                             "Stanby",
                                             "No pellets alarm",
                                             "No ignition alarm",
                                             "Undefined alarm"};

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

//////////////////////////////////////////////////////////////////////
// Interface classes.
class MicroNovaBaseListener {
 public:
  MicroNovaBaseListener(MicroNova *m) { micronova_ = m; }
  virtual void dump_config();

  void set_micronova_object(MicroNova *m) { micronova_ = m; }

  void set_function(MicroNovaFunctions f) { function_ = f; }
  MicroNovaFunctions get_function() { return function_; }

  void set_memory_location(uint8_t f) { memory_location_ = f; }
  uint8_t get_memory_location() { return memory_location_; }

  void set_memory_address(uint8_t f) { memory_address_ = f; }
  uint8_t get_memory_address() { return memory_address_; }

 protected:
  MicroNova *micronova_{nullptr};
  MicroNovaFunctions function_ = MicroNovaFunctions::STOVE_FUNCTION_VOID;
  uint8_t memory_location_ = 0;
  uint8_t memory_address_ = 0;
};

class MicroNovaSensorListener : public MicroNovaBaseListener {
 public:
  MicroNovaSensorListener(MicroNova *m) : MicroNovaBaseListener(m) {}
  virtual void read_value_from_stove();

  void set_needs_update(bool u) { needs_update_ = u; }
  bool get_needs_update() { return needs_update_; }

 protected:
  bool needs_update_ = false;
};

class MicroNovaSwitchListener : public MicroNovaBaseListener {
 public:
  MicroNovaSwitchListener(MicroNova *m) : MicroNovaBaseListener(m) {}
  virtual void set_stove_switch_state(bool v);
  virtual bool get_stove_switch_state();

 protected:
  uint8_t memory_data_on_ = 0;
  uint8_t memory_data_off_ = 0;
};

class MicroNovaButtonListener : public MicroNovaBaseListener {
 public:
  MicroNovaButtonListener(MicroNova *m) : MicroNovaBaseListener(m) {}

 protected:
  uint8_t memory_data_ = 0;
};

/////////////////////////////////////////////////////////////////////
// Main component class
class MicroNova : public PollingComponent, public uart::UARTDevice {
 public:
  MicroNova() {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  void register_micronova_listener(MicroNovaSensorListener *l) { micronova_listeners_.push_back(l); }

  int read_address(uint8_t addr, uint8_t reg);
  void write_address(uint8_t location, uint8_t address, uint8_t data);

  void set_enable_rx_pin(GPIOPin *enable_rx_pin) { this->enable_rx_pin_ = enable_rx_pin; }

  void set_current_stove_state(uint8_t s) { current_stove_state_ = s; }
  uint8_t get_current_stove_state() { return current_stove_state_; }

  void set_thermostat_temperature(uint8_t t) { current_thermostat_temperature_ = t; }
  uint8_t get_thermostat_temperature() { return current_thermostat_temperature_; }

  void set_stove_switch(MicroNovaSwitchListener *s) { stove_switch_ = s; }
  MicroNovaSwitchListener *get_stove_switch() { return stove_switch_; }

 protected:
  uint8_t current_stove_state_ = 0;
  uint8_t current_thermostat_temperature_ = 20;

  GPIOPin *enable_rx_pin_{nullptr};

  std::vector<MicroNovaSensorListener *> micronova_listeners_{};
  MicroNovaSwitchListener *stove_switch_{nullptr};
};

}  // namespace micronova
}  // namespace esphome
