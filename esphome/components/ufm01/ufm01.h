#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#include "esphome/components/uart/uart.h"

#include <array>
#include <cstdint>

// component API definition at https://www.sciosense.com/wp-content/uploads/2025/06/UFM-01-Datasheet-1.pdf

namespace esphome::ufm01 {

namespace testing {
class TestableUFM01;
}  // namespace testing

static constexpr size_t FRAME_SIZE = 32;
static constexpr size_t PASSIVE_FRAME_SIZE = 23;

enum class OperatingMode : uint8_t {
  STARTUP = 0,
  ACTIVE_STREAM = 1,
  PASSIVE_POLL = 2,
};

enum class StartupPhase : uint8_t {
  WAIT = 0,
  RESET_WAIT_ACK = 1,
  RESET_RETRY_WAIT = 2,
  POST_RESET_WAIT = 3,
  ACTIVE_WAIT_FRAME = 4,
  PASSIVE_WAIT_REPLY = 5,
};

enum class PassiveReadResult : uint8_t {
  PASSIVE_READ_RESULT_PENDING = 0,
  PASSIVE_READ_RESULT_SUCCESS = 1,
  PASSIVE_READ_RESULT_FAILURE = 2,
};

class UFM01Component : public uart::UARTDevice, public Component {
#ifdef USE_SENSOR
  SUB_SENSOR(accumulated_flow)
  SUB_SENSOR(flow)
  SUB_SENSOR(temperature)
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(ufc_chip_error)
  SUB_BINARY_SENSOR(flow_direction_wrong)
  SUB_BINARY_SENSOR(empty_tube)
  SUB_BINARY_SENSOR(flow_rate_out_of_range)
#endif

 public:
  void setup() override;

  void dump_config() override;

  void loop() override;

  float get_setup_priority() const override;

 protected:
  bool clear_accumulated_flow_();
  bool set_active_mode_();
  bool reset_device_();

 private:
  bool send_command_(const std::array<uint8_t, 7> &command);
  void send_command_no_wait_(const std::array<uint8_t, 7> &command);
  bool consume_ack_();
  void flush_rx_();
  bool process_active_stream_();
  void on_active_frame_(uint8_t data[FRAME_SIZE]);

  void loop_startup_();
  void loop_active_stream_();
  void loop_passive_poll_();
  void set_startup_phase_(StartupPhase phase);
  void enter_active_stream_(const char *reason);
  void start_passive_read_();
  PassiveReadResult continue_passive_read_();

  OperatingMode operating_mode_{OperatingMode::STARTUP};
  StartupPhase startup_phase_{StartupPhase::WAIT};
  uint32_t phase_start_ms_{0};
  uint32_t startup_wait_ms_{0};
  bool reset_retried_{false};
  uint32_t last_valid_frame_ms_{0};
  uint32_t last_poll_ms_{0};

  bool passive_read_pending_{false};
  uint32_t passive_start_ms_{0};
  size_t passive_index_{0};
  uint8_t passive_frame_[PASSIVE_FRAME_SIZE];

  int32_t read_index_ = 0;
  uint8_t data_[FRAME_SIZE];

  friend class testing::TestableUFM01;
};

}  // namespace esphome::ufm01
