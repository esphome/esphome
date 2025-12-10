#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <vector>

namespace esphome::micronova {

static const char *const TAG = "micronova";

class MicroNova;

//////////////////////////////////////////////////////////////////////
// Interface classes.
class MicroNovaBaseListener {
 public:
  MicroNovaBaseListener() {}
  MicroNovaBaseListener(MicroNova *m) { this->micronova_ = m; }

  void set_micronova_object(MicroNova *m) { this->micronova_ = m; }

  void set_memory_location(uint8_t l) { this->memory_location_ = l; }
  uint8_t get_memory_location() { return this->memory_location_; }

  void set_memory_address(uint8_t a) { this->memory_address_ = a; }
  uint8_t get_memory_address() { return this->memory_address_; }

  void dump_base_config();

 protected:
  MicroNova *micronova_{nullptr};
  uint8_t memory_location_ = 0;
  uint8_t memory_address_ = 0;
};

class MicroNovaListener : public MicroNovaBaseListener, public PollingComponent {
 public:
  MicroNovaListener() {}
  MicroNovaListener(MicroNova *m) : MicroNovaBaseListener(m) {}
  virtual void request_value_from_stove() = 0;
  virtual void process_value_from_stove(int value_from_stove) = 0;

  void set_needs_update(bool u) { this->needs_update_ = u; }
  bool get_needs_update() { return this->needs_update_; }

  void update() override { this->set_needs_update(true); }

  void dump_base_config();

 protected:
  bool needs_update_ = false;
};

class MicroNovaButtonListener : public MicroNovaBaseListener {
 public:
  MicroNovaButtonListener(MicroNova *m) : MicroNovaBaseListener(m) {}

 protected:
  uint8_t memory_data_ = 0;
};

/////////////////////////////////////////////////////////////////////
// Main component class
class MicroNova : public Component, public uart::UARTDevice {
 public:
  MicroNova() {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_micronova_listener(MicroNovaListener *l) { this->micronova_listeners_.push_back(l); }
  void request_update_listeners();

  void request_address(uint8_t location, uint8_t address, MicroNovaListener *listener);
  void write_address(uint8_t location, uint8_t address, uint8_t data);
  int read_stove_reply();

  void set_enable_rx_pin(GPIOPin *enable_rx_pin) { this->enable_rx_pin_ = enable_rx_pin; }

 protected:
  GPIOPin *enable_rx_pin_{nullptr};

  struct MicroNovaSerialTransmission {
    uint32_t request_transmission_time;
    uint8_t memory_location;
    uint8_t memory_address;
    bool reply_pending;
    MicroNovaListener *initiating_listener;
  };

  Mutex reply_pending_mutex_;
  MicroNovaSerialTransmission current_transmission_;

  std::vector<MicroNovaListener *> micronova_listeners_{};
};

}  // namespace esphome::micronova
