#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/frame_reader.h"
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

namespace testing {
class TestableUFM01;
}  // namespace testing

static constexpr size_t FRAME_SIZE = 32;
static constexpr uint8_t FRAME_START_BYTE_1 = 0x3C;
static constexpr uint8_t FRAME_START_BYTE_2 = 0x32;
static constexpr size_t PASSIVE_FRAME_SIZE = 23;
static constexpr size_t PASSIVE_FRAME_WITH_ID_SIZE = 39;
#ifdef USE_UFM01_METER_ID
static constexpr size_t PASSIVE_FRAME_MAX_SIZE = PASSIVE_FRAME_WITH_ID_SIZE;
#else
static constexpr size_t PASSIVE_FRAME_MAX_SIZE = PASSIVE_FRAME_SIZE;
#endif
static constexpr size_t SOFTWARE_VERSION_RESPONSE_SIZE = 7;
static constexpr size_t DEVICE_ID_LENGTH = 5;
static constexpr size_t DEVICE_ID_STRING_LENGTH = 10;
static constexpr size_t SOFTWARE_VERSION_LENGTH = 4;
static constexpr size_t SOFTWARE_VERSION_STRING_LENGTH = 8;

enum class OperatingMode : uint8_t {
  STARTUP = 0,
  ACTIVE_STREAM = 1,
  PASSIVE_POLL = 2,
};

enum class StartupPhase : uint8_t {
  WAIT,
  RESET_WAIT_ACK,
  RESET_RETRY_WAIT,
  POST_RESET_WAIT,
#ifdef USE_UFM01_SOFTWARE_VERSION
  SOFTWARE_VERSION_WAIT_REPLY,
#endif
  ACTIVE_WAIT_FRAME,
  SET_PASSIVE_WAIT_ACK,
  PASSIVE_WAIT_REPLY,
};

enum class ReadResult : uint8_t {
  READ_RESULT_PENDING = 0,
  READ_RESULT_SUCCESS = 1,
  READ_RESULT_FAILURE = 2,
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

#ifdef USE_UFM01_METER_ID
  SUB_TEXT_SENSOR(meter_id)
#endif
#ifdef USE_UFM01_SOFTWARE_VERSION
  SUB_TEXT_SENSOR(software_version)
#endif

 public:
  void setup() override;

  void dump_config() override;

  void loop() override;

#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
  /// Queue a clear of the accumulated flow; sent once the UART is idle, a request while one is pending joins it
  void request_clear_accumulated_flow();
#endif

  float get_setup_priority() const override;

 private:
#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
  bool can_start_clear_action_() const;
  void loop_pending_clear_action_(uint32_t now);
  void finish_pending_clear_();
#endif
  void send_command_no_wait_(const std::array<uint8_t, 7> &command);
  bool consume_ack_();
  void flush_rx_();
  bool process_active_stream_(uint32_t now);
  void on_active_frame_(const uint8_t data[FRAME_SIZE], uint32_t now);
#ifdef USE_UFM01_METER_ID
  void publish_meter_id_from_frame_(const uint8_t data[FRAME_SIZE]);
#endif
  void publish_stale_flow_and_temperature_();
#ifdef USE_UFM01_SOFTWARE_VERSION
  void start_software_version_read_(uint32_t now);
  ReadResult continue_software_version_read_(uint32_t now);
  void finish_software_version_read_(ReadResult result, uint32_t now);
  bool service_software_version_(uint32_t now);
#endif

  void loop_startup_(uint32_t now);
  void loop_active_stream_(uint32_t now);
  void loop_passive_poll_(uint32_t now);
  void set_mode_(OperatingMode mode, uint32_t now);
  void set_startup_phase_(StartupPhase phase, uint32_t now);
  void send_reset_(const LogString *status, uint32_t now);
  void enter_active_stream_(const LogString *reason, uint32_t now);
  void enter_passive_from_stale_(uint32_t now);
  void restart_startup_(const LogString *reason, uint32_t now);
  void start_passive_read_(uint32_t now);
  ReadResult continue_passive_read_(uint32_t now);
  void note_passive_poll_result_(bool success, uint32_t now);

  uint32_t phase_start_ms_{0};
  uint32_t startup_wait_ms_{0};
  uint32_t last_valid_frame_ms_{0};
  uint32_t last_poll_ms_{0};
  // Shared by the passive read and the software version read, which never overlap
  uint32_t passive_start_ms_{0};
#ifdef USE_UFM01_SOFTWARE_VERSION
  uint32_t last_software_version_attempt_ms_{0};
#endif
#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
  uint32_t pending_clear_start_ms_{0};
#endif

  OperatingMode operating_mode_{OperatingMode::STARTUP};
  StartupPhase startup_phase_{StartupPhase::WAIT};
  bool reset_retried_{false};
  bool passive_read_pending_{false};
  uint8_t consecutive_passive_failures_{0};
  uint8_t passive_index_{0};
  uint8_t passive_expected_size_{PASSIVE_FRAME_SIZE};
  uint8_t passive_start_byte_2_{0};
#ifdef USE_UFM01_SOFTWARE_VERSION
  bool software_version_published_{false};
  bool software_version_read_pending_{false};
  uint8_t software_version_failures_{0};
#endif
#ifdef USE_UFM01_METER_ID
  bool meter_id_published_{false};
  bool meter_id_invalid_logged_{false};
#endif
#ifdef USE_UFM01_CLEAR_ACCUMULATED_FLOW_ACTION
  bool clear_pending_{false};
  bool pending_clear_sent_{false};
#endif

  uint8_t passive_frame_[PASSIVE_FRAME_MAX_SIZE];
  FrameReader<FRAME_SIZE, FRAME_START_BYTE_1, FRAME_START_BYTE_2> active_reader_;

  friend class testing::TestableUFM01;
};

}  // namespace esphome::ufm01
