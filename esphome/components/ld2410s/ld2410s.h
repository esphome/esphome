#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::ld2410s {

// Largest frame read here is the standard data frame: 4 header + 2 length + payload + 4 footer
static constexpr size_t RX_BUFFER_SIZE = 96;

class LD2410S : public Component, public uart::UARTDevice {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(presence)
  SUB_BINARY_SENSOR(calibration_running)
#endif

 public:
  void loop() override;
  void dump_config() override;

 protected:
  enum class FrameType : uint8_t { NONE, SHORT_DATA, STD_DATA, COMMAND };

  void receive_byte_(uint8_t byte);
  void handle_frame_();
  void handle_data_frame_(const uint8_t *payload, uint16_t len);
  void handle_command_ack_(const uint8_t *payload, uint16_t len);
  void reset_frame_() {
    this->rx_len_ = 0;
    this->expected_len_ = 0;
    this->frame_type_ = FrameType::NONE;
  }
  void run_init_sequence_(uint32_t now);
  void send_command_(uint16_t command);
  void set_init_done_(bool done);
  void publish_presence_(bool presence);
  void publish_calibration_running_(bool running);

  uint32_t next_send_at_{0};
  uint16_t rx_len_{0};
  uint16_t expected_len_{0};
  uint8_t rx_buffer_[RX_BUFFER_SIZE];
  uint8_t init_step_{0};
  uint8_t init_timeouts_{0};
  FrameType frame_type_{FrameType::NONE};
  bool awaiting_ack_{false};
  bool init_done_{false};
};

}  // namespace esphome::ld2410s
