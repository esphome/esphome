#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::micronova {

static constexpr uint8_t WRITE_QUEUE_SIZE = 10;

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
  MicroNova(GPIOPin *enable_rx_pin) : enable_rx_pin_(enable_rx_pin) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

#ifdef MICRONOVA_LISTENER_COUNT
  void register_micronova_listener(MicroNovaListener *listener);

  /// Queue a read request to the stove (low priority - added at back)
  /// All listeners registered for this address will be notified with the result
  /// @param location Memory location on the stove
  /// @param address Memory address on the stove
  void queue_read_request(uint8_t location, uint8_t address);
#endif

#ifdef USE_MICRONOVA_WRITER
  /// Queue a write command to the stove (processed before reads)
  /// @param location Memory location on the stove
  /// @param address Memory address on the stove
  /// @param data Data to write
  /// @return true if command was queued, false if queue was full
  bool queue_write_command(uint8_t location, uint8_t address, uint8_t data);
#endif

 protected:
  void send_current_command_();
  int read_stove_reply_();
#ifdef MICRONOVA_LISTENER_COUNT
  void request_update_listeners_();
#endif

  GPIOPin *enable_rx_pin_;

#ifdef USE_MICRONOVA_WRITER
  StaticRingBuffer<MicroNovaCommand, WRITE_QUEUE_SIZE> write_queue_;
#endif
#ifdef MICRONOVA_LISTENER_COUNT
  StaticRingBuffer<MicroNovaCommand, MICRONOVA_LISTENER_COUNT> read_queue_;
#endif
  MicroNovaCommand current_command_{};
  uint32_t transmission_time_{0};  ///< Time when current command was sent
  bool reply_pending_{false};      ///< True if we are waiting for a reply from the stove

#ifdef MICRONOVA_LISTENER_COUNT
  StaticVector<MicroNovaListener *, MICRONOVA_LISTENER_COUNT> listeners_;
#endif
};

}  // namespace esphome::micronova
