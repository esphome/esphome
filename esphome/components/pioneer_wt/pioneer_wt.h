#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#include "pioneer_wt_protocol.h"

namespace esphome::pioneer_wt {

// The largest body control() builds is 30 bytes.
static constexpr size_t MAX_BODY_SIZE = 32;
static constexpr size_t TX_QUEUE_SIZE = 4;

class PioneerWT final : public climate::Climate, public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  /// Turns the indoor unit's front display on or off.
  void set_display_on(bool on) { this->send_setting_(ATTR_DISPLAY, on); }
  /// Turns the beep that confirms each command on or off.
  void set_beeper_on(bool on) { this->send_setting_(ATTR_BEEPER, on); }

#ifdef USE_SWITCH
  SUB_SWITCH(display)
  SUB_SWITCH(beeper)
#endif

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void handle_frame_(const uint8_t *frame, size_t len);
  void handle_status_(const uint8_t *body, size_t len);
  void write_frame_(uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack, uint8_t class_1, uint8_t class_2,
                    const uint8_t *body, size_t len);
  /// Queues a data frame that the AC must acknowledge; it is resent until it is.
  void queue_command_(uint8_t msg_class, const uint8_t *body, size_t len);
  void send_pending_();
  /// Removes the command at the head of the queue.
  void pop_command_();
  /// Removes the head command without an acknowledgement.
  void drop_command_();
  /// Queues a command that sets one on/off attribute.
  void send_setting_(uint16_t attr, bool on);
  void request_status_();

  struct PendingCommand {
    uint8_t msg_class;
    uint8_t seq;
    uint8_t len;
    std::array<uint8_t, MAX_BODY_SIZE> body;
  };

  uint32_t last_send_{0};
  // Time of the last full status, or of the last request for one
  uint32_t last_status_{0};
  uint32_t last_rx_{0};
  PacketReader reader_;
  StaticRingBuffer<PendingCommand, TX_QUEUE_SIZE> queue_;
  // Send attempts of the command at the head of the queue
  uint8_t attempts_{0};
  uint8_t next_seq_{1};
  bool power_{false};
  // Power after the commands still waiting to be sent; power_ is what the AC last reported.
  bool power_wanted_{false};
  // Value of ATTR_MODE: 0 auto, 1 cool, 2 dry, 3 fan, 4 heat
  uint8_t ac_mode_{0};
};

}  // namespace esphome::pioneer_wt
