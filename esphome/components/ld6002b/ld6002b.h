#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/gpio.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <array>
#include <memory>
#include <cmath>

namespace esphome::ld6002b {

static constexpr uint8_t MAX_TARGETS = 3;
static constexpr uint8_t AREA_COUNT = 4;
static constexpr size_t DEFAULT_MAX_DATA_LEN = 1024;
static constexpr size_t DEFAULT_MAX_DATA_LEN_POINT_CLOUD = 4096;
static constexpr size_t CMD_MAX_DATA_LEN = 32;

class LD6002BComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_wakeup_pin(GPIOPin *pin) { this->wakeup_pin_ = pin; }
  void set_wakeup_pulse_ms(uint32_t ms) { this->wakeup_pulse_ms_ = ms; }
  void set_auto_wake(bool enable) { this->auto_wake_ = enable; }

#ifdef USE_BINARY_SENSOR
  void set_presence_binary_sensor(binary_sensor::BinarySensor *sensor) { this->presence_binary_sensor_ = sensor; }
  void set_target_presence_binary_sensor(uint8_t target, binary_sensor::BinarySensor *sensor) {
    this->target_presence_[target] = sensor;
  }
#endif
  void set_max_data_len(size_t max_data_len);

 protected:
  enum class ParseState : uint8_t { SOF, HEADER, HCK, DATA, DCK, DISCARD };

  struct PendingCommand {
    uint16_t type{0};
    uint8_t len{0};
    std::array<uint8_t, CMD_MAX_DATA_LEN> data{};
  };

  void parse_byte_(uint8_t byte);
  void reset_parser_();
  void handle_frame_(uint16_t type, const uint8_t *data, uint16_t len);
  void handle_target_report_(const uint8_t *data, uint16_t len);
  void handle_area_presence_(const uint8_t *data, uint16_t len);

  void queue_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void process_command_queue_();
  void send_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void send_command_untracked_(uint16_t type, const uint8_t *data, uint8_t len);
  void send_command_internal_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  void write_frame_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  void send_control_command_(uint32_t command);

  static uint16_t read_u16_be(const uint8_t *data);
  static uint32_t read_u32_le(const uint8_t *data);
  static int32_t read_int32_le(const uint8_t *data);
  static float read_f32_le(const uint8_t *data);
  static void write_u32_le(uint8_t *data, uint32_t value);

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *presence_binary_sensor_{nullptr};
  std::array<binary_sensor::BinarySensor *, MAX_TARGETS> target_presence_{};
  std::array<binary_sensor::BinarySensor *, AREA_COUNT> area_presence_{};
#endif

  GPIOPin *wakeup_pin_{nullptr};
  uint32_t wakeup_pulse_ms_{50};
  bool auto_wake_{true};

  ParseState parse_state_{ParseState::SOF};
  uint8_t header_pos_{0};
  uint8_t header_xor_{0};
  uint16_t data_len_{0};
  uint16_t frame_type_{0};
  uint16_t frame_id_{0};
  uint16_t data_pos_{0};
  uint8_t data_xor_{0};
  uint32_t discard_remaining_{0};
  size_t max_data_len_{0};
  bool max_data_len_overridden_{false};
  std::unique_ptr<uint8_t[]> data_buf_;
  uint16_t next_frame_id_{0};

  static constexpr uint8_t CMD_QUEUE_SIZE = 16;
  static constexpr uint32_t CMD_ACK_TIMEOUT_MS = 300;
  static constexpr uint8_t CMD_MAX_RETRIES = 3;

  std::array<PendingCommand, CMD_QUEUE_SIZE> cmd_queue_{};
  uint8_t cmd_head_{0};
  uint8_t cmd_tail_{0};
  uint8_t cmd_count_{0};
  bool command_active_{false};
  bool command_sent_{false};
  PendingCommand active_command_{};
  uint16_t active_frame_id_{0};
  uint8_t retries_left_{0};
  uint32_t last_send_ms_{0};

  bool target_presence_any_{false};
  bool area_presence_any_{false};

  std::array<bool, MAX_TARGETS> last_target_presence_{};
  std::array<bool, MAX_TARGETS> target_presence_initialized_{};
};

}  // namespace esphome::ld6002b
