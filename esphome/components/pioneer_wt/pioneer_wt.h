#pragma once

#include <array>
#include <cstdint>

#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "pioneer_wt_protocol.h"

namespace esphome::pioneer_wt {

class PioneerWT : public climate::Climate, public uart::UARTDevice, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void handle_frame_(const uint8_t *frame, size_t len);
  void handle_status_(const uint8_t *body, size_t len);
  void send_reply_(uint8_t flags, uint8_t ack_seq, uint8_t msg_class, const uint8_t *extra, size_t extra_len);
  /// Writes one frame; payload is everything after the checksum.
  void write_frame_(uint8_t flags, uint8_t type, uint8_t seq, uint8_t ack, const uint8_t *payload, size_t len);
  /// Queues a data frame that the AC must acknowledge; it is resent until it is.
  void queue_command_(uint8_t msg_class, const uint8_t *body, size_t body_len);
  void send_pending_();
  void request_status_();

  struct PendingFrame {
    uint8_t len;
    uint8_t seq;
    uint8_t attempts;
    std::array<uint8_t, MAX_BODY_SIZE + 2> payload;
  };

  FrameReader reader_;
  std::array<PendingFrame, TX_QUEUE_SIZE> queue_{};
  uint8_t queue_count_{0};
  uint8_t next_seq_{1};
  uint32_t last_send_{0};
  uint32_t last_status_{0};
  bool power_{false};
  uint8_t ac_mode_{MODE_AUTO};
};

}  // namespace esphome::pioneer_wt
