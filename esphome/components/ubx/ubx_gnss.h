#pragma once

#include "esphome/components/gnss/gnss.h"
#include "esphome/components/uart/uart.h"
#include "ubx_protocol.h"

namespace esphome::ubx {

static constexpr uint8_t UBX_DYNAMIC_MODEL_UNSET = 0xFF;

/// A u-blox receiver set up for and read in the binary UBX protocol.
class UBXGNSS final : public gnss::GNSSComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_measurement_rate_ms(uint16_t ms) { this->measurement_rate_ms_ = ms; }
  void set_dynamic_model(uint8_t model) { this->dynamic_model_ = model; }

 protected:
  void start_setup_();
  void clear_data_();
  void send_step_();
  void step_done_();
  void step_timeout_();
  void handle_frame_();
  void send_(uint8_t msg_class, uint8_t msg_id, const uint8_t *payload, uint16_t len);
  const uint8_t *messages_(uint8_t &count) const;

  FrameParser parser_;
  // From MON-VER; empty until the receiver answers
  char sw_version_[31]{};
  char hw_version_[11]{};
  uint32_t last_epoch_ms_{0};
  uint16_t protocol_version_{0};
  uint16_t measurement_rate_ms_{1000};
  uint8_t dynamic_model_{UBX_DYNAMIC_MODEL_UNSET};
  uint8_t step_{0};
  uint8_t retries_{0};
  bool fix_{false};
};

}  // namespace esphome::ubx
