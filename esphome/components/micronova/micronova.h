#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/log.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace micronova {

static const char *const TAG = "micronova";
static const int STOVE_REPLY_DELAY = 60;

static const std::string STOVE_STATES[11] = {"Off",
                                             "Start",
                                             "Pellets loading",
                                             "Ignition",
                                             "Working",
                                             "Brazier Cleaning",
                                             "Final Cleaning",
                                             "Standby",
                                             "No pellets alarm",
                                             "No ignition alarm",
                                             "Undefined alarm"};

enum class MicroNovaFunctions {
  STOVE_FUNCTION_VOID = 0,
  STOVE_FUNCTION_SWITCH = 1,
  STOVE_FUNCTION_ROOM_TEMPERATURE = 2,
  STOVE_FUNCTION_THERMOSTAT_TEMPERATURE = 3,
  STOVE_FUNCTION_FUMES_TEMPERATURE = 4,
  STOVE_FUNCTION_STOVE_POWER = 5,
  STOVE_FUNCTION_FAN_SPEED = 6,
  STOVE_FUNCTION_STOVE_STATE = 7,
  STOVE_FUNCTION_MEMORY_ADDRESS_SENSOR = 8,
  STOVE_FUNCTION_WATER_TEMPERATURE = 9,
  STOVE_FUNCTION_WATER_PRESSURE = 10,
  STOVE_FUNCTION_POWER_LEVEL = 11,
  STOVE_FUNCTION_CUSTOM = 12
};

class MicroNova;

//////////////////////////////////////////////////////////////////////
// Interface classes.
class MicroNovaBaseListener {
 public:
  MicroNovaBaseListener() {}
  MicroNovaBaseListener(MicroNova *m) { this->micronova_ = m; }
  virtual void dump_config();

  void set_micronova_object(MicroNova *m) { this->micronova_ = m; }

  void set_function(MicroNovaFunctions f) { this->function_ = f; }
  MicroNovaFunctions get_function() { return this->function_; }

  void set_memory_location(uint8_t l) { this->memory_location_ = l; }
  uint8_t get_memory_location() { return this->memory_location_; }

  void set_memory_address(uint8_t a) { this->memory_address_ = a; }
  uint8_t get_memory_address() { return this->memory_address_; }

 protected:
  MicroNova *micronova_{nullptr};
  MicroNovaFunctions function_ = MicroNovaFunctions::STOVE_FUNCTION_VOID;
  uint8_t memory_location_ = 0;
  uint8_t memory_address_ = 0;
};

class MicroNovaSensorListener : public MicroNovaBaseListener {
 public:
  MicroNovaSensorListener() {}
  MicroNovaSensorListener(MicroNova *m) : MicroNovaBaseListener(m) {}
  virtual void request_value_from_stove() = 0;
  virtual void process_value_from_stove(int value_from_stove) = 0;

  void set_needs_update(bool u) { this->needs_update_ = u; }
  bool get_needs_update() { return this->needs_update_; }

 protected:
  bool needs_update_ = false;
};

class MicroNovaNumberListener : public MicroNovaBaseListener {
 public:
  MicroNovaNumberListener(MicroNova *m) : MicroNovaBaseListener(m) {}
  virtual void request_value_from_stove() = 0;
  virtual void process_value_from_stove(int value_from_stove) = 0;

  void set_needs_update(bool u) { this->needs_update_ = u; }
  bool get_needs_update() { return this->needs_update_; }

 protected:
  bool needs_update_ = false;
};

class MicroNovaSwitchListener : public MicroNovaBaseListener {
 public:
  MicroNovaSwitchListener(MicroNova *m) : MicroNovaBaseListener(m) {}
  virtual void set_stove_state(bool v) = 0;
  virtual bool get_stove_state() = 0;

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
  void register_micronova_listener(MicroNovaSensorListener *l) { this->micronova_listeners_.push_back(l); }

  void request_address(uint8_t location, uint8_t address, MicroNovaSensorListener *listener);
  void write_address(uint8_t location, uint8_t address, uint8_t data);
  int read_stove_reply();

  void set_enable_rx_pin(GPIOPin *enable_rx_pin) { this->enable_rx_pin_ = enable_rx_pin; }

  void set_current_stove_state(uint8_t s) { this->current_stove_state_ = s; }
  uint8_t get_current_stove_state() { return this->current_stove_state_; }

  void set_stove(MicroNovaSwitchListener *s) { this->stove_switch_ = s; }
  MicroNovaSwitchListener *get_stove_switch() { return this->stove_switch_; }

 protected:
  uint8_t current_stove_state_ = 0;

  GPIOPin *enable_rx_pin_{nullptr};

  struct MicroNovaSerialTransmission {
    uint32_t request_transmission_time;
    uint8_t memory_location;
    uint8_t memory_address;
    bool reply_pending;
    MicroNovaSensorListener *initiating_listener;
  };

  Mutex reply_pending_mutex_;
  MicroNovaSerialTransmission current_transmission_;

  std::vector<MicroNovaSensorListener *> micronova_listeners_{};
  MicroNovaSwitchListener *stove_switch_{nullptr};
};

}  // namespace micronova
}  // namespace esphome
