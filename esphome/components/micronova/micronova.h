#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <deque>
#include <vector>

namespace esphome::micronova {

static const char *const TAG = "micronova";

/// Represents a command to be sent to the stove
/// Write commands have the high bit (0x80) set in memory_location
struct MicroNovaCommand {
  uint8_t memory_location;
  uint8_t memory_address;
  uint8_t data;  ///< Only used for write commands

  bool is_write() const;
};

class MicroNova;

//////////////////////////////////////////////////////////////////////
// Interface classes.
class MicroNovaBaseListener {
 public:
  MicroNovaBaseListener(MicroNova *m) { this->micronova_ = m; }

  void set_memory_location(uint8_t l) { this->memory_location_ = l; }
  uint8_t get_memory_location() { return this->memory_location_; }

  void set_memory_address(uint8_t a) { this->memory_address_ = a; }
  uint8_t get_memory_address() { return this->memory_address_; }

  void dump_base_config();

 protected:
  MicroNova *micronova_;
  uint8_t memory_location_ = 0;
  uint8_t memory_address_ = 0;
};

class MicroNovaListener : public MicroNovaBaseListener, public PollingComponent {
 public:
  MicroNovaListener(MicroNova *m) : MicroNovaBaseListener(m) {}

  void update() override { this->request_value_from_stove_(); }

  virtual void process_value_from_stove(int value_from_stove) = 0;

  void dump_base_config();

 protected:
  void request_value_from_stove_();
};

/////////////////////////////////////////////////////////////////////
// Main component class
class MicroNova : public Component, public uart::UARTDevice {
 public:
  MicroNova() {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_micronova_listener(MicroNovaListener *listener);

  /// Queue a read request to the stove (low priority - added at back)
  /// All listeners registered for this address will be notified with the result
  /// @param location Memory location on the stove
  /// @param address Memory address on the stove
  void queue_read_request(uint8_t location, uint8_t address);

  /// Queue a write command to the stove (high priority - inserted at front)
  /// @param location Memory location on the stove
  /// @param address Memory address on the stove
  /// @param data Data to write
  void queue_write_command(uint8_t location, uint8_t address, uint8_t data);

  void set_enable_rx_pin(GPIOPin *enable_rx_pin) { this->enable_rx_pin_ = enable_rx_pin; }

 protected:
  void send_current_command_();

  int read_stove_reply_();

  void request_update_listeners_();

  uint8_t current_stove_state_ = 0;

  GPIOPin *enable_rx_pin_{nullptr};

  std::deque<MicroNovaCommand> command_queue_;

  MicroNovaCommand current_command_;
  uint32_t transmission_time_{0};  ///< Time when current command was sent (0 = no command pending)

  std::vector<MicroNovaListener *> listeners_;
};

}  // namespace esphome::micronova
