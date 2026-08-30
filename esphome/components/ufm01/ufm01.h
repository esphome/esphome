#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

#include <array>
#include <cstdint>

// component API definition at https://www.sciosense.com/wp-content/uploads/2025/06/UFM-01-Datasheet-1.pdf

namespace esphome::ufm01 {

class ClearAccumulatedFlowActionInterface;

namespace testing {
class TestableUFM01;
}  // namespace testing

static constexpr size_t FRAME_SIZE = 32;
static constexpr size_t PASSIVE_FRAME_SIZE = 23;
static constexpr size_t PASSIVE_FRAME_WITH_ID_SIZE = 39;
static constexpr size_t PASSIVE_FRAME_MAX_SIZE = PASSIVE_FRAME_WITH_ID_SIZE;
static constexpr size_t SOFTWARE_VERSION_RESPONSE_SIZE = 7;
static constexpr size_t DEVICE_ID_LENGTH = 5;
static constexpr size_t DEVICE_ID_STRING_LENGTH = 10;
static constexpr size_t SOFTWARE_VERSION_LENGTH = 4;
static constexpr size_t SOFTWARE_VERSION_STRING_LENGTH = 8;

enum class OperatingMode : uint8_t {
  STARTUP = 0,
  ACTIVE_STREAM = 1,
  ENTERING_PASSIVE = 2,  // SET_PASSIVE_MODE sent, waiting for ACK
  PASSIVE_POLL = 3,
};

enum class StartupPhase : uint8_t {
  WAIT = 0,
  RESET_WAIT_ACK = 1,
  RESET_RETRY_WAIT = 2,
  POST_RESET_WAIT = 3,
  SOFTWARE_VERSION_WAIT_REPLY = 4,
  ACTIVE_WAIT_FRAME = 5,
  SET_PASSIVE_WAIT_ACK = 6,
  PASSIVE_WAIT_REPLY = 7,
};

enum class PassiveReadResult : uint8_t {
  PASSIVE_READ_RESULT_PENDING = 0,
  PASSIVE_READ_RESULT_SUCCESS = 1,
  PASSIVE_READ_RESULT_FAILURE = 2,
};

enum class SoftwareVersionReadResult : uint8_t {
  SOFTWARE_VERSION_READ_RESULT_PENDING = 0,
  SOFTWARE_VERSION_READ_RESULT_SUCCESS = 1,
  SOFTWARE_VERSION_READ_RESULT_FAILURE = 2,
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

#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(device_id)
  SUB_TEXT_SENSOR(software_version)
#endif

 public:
  void setup() override;

  void dump_config() override;

  void loop() override;

  float get_setup_priority() const override;

  void request_clear_accumulated_flow_(ClearAccumulatedFlowActionInterface *action);

 private:
  void send_command_no_wait_(const std::array<uint8_t, 7> &command);
  bool consume_ack_();
  void flush_rx_();
  bool process_active_stream_();
  void on_active_frame_(uint8_t data[FRAME_SIZE]);
  void publish_device_id_from_frame_(const uint8_t data[FRAME_SIZE]);
  void publish_stale_flow_and_temperature_();
  void start_software_version_read_();
  SoftwareVersionReadResult continue_software_version_read_();

  void loop_startup_();
  void loop_active_stream_();
  void loop_entering_passive_();
  void loop_passive_poll_();
  void loop_pending_clear_action_();
  void finish_pending_clear_action_();
  void set_startup_phase_(StartupPhase phase);
  void enter_active_stream_(const char *reason);
  void enter_passive_from_stale_();
  void restart_startup_(const char *reason);
  void start_passive_read_();
  PassiveReadResult continue_passive_read_();
  void note_passive_poll_result_(PassiveReadResult result);
  void try_pending_software_version_read_();
  size_t passive_expected_frame_size_() const;

  OperatingMode operating_mode_{OperatingMode::STARTUP};
  StartupPhase startup_phase_{StartupPhase::WAIT};
  uint32_t phase_start_ms_{0};
  uint32_t startup_wait_ms_{0};
  bool reset_retried_{false};
  uint32_t last_valid_frame_ms_{0};
  uint32_t last_poll_ms_{0};
  uint8_t consecutive_passive_failures_{0};
  bool device_id_published_{false};
  bool software_version_published_{false};
  bool passive_expects_id_{false};
  bool software_version_read_pending_{false};
  uint32_t last_software_version_attempt_ms_{0};

  ClearAccumulatedFlowActionInterface *pending_clear_action_{nullptr};
  uint32_t pending_clear_start_ms_{0};

  bool passive_read_pending_{false};
  uint32_t passive_start_ms_{0};
  size_t passive_index_{0};
  uint8_t passive_frame_[PASSIVE_FRAME_MAX_SIZE];
  uint32_t software_version_start_ms_{0};
  size_t software_version_index_{0};
  uint8_t software_version_frame_[SOFTWARE_VERSION_RESPONSE_SIZE];

  int32_t read_index_ = 0;
  uint8_t data_[FRAME_SIZE];

  friend class testing::TestableUFM01;
};

}  // namespace esphome::ufm01
