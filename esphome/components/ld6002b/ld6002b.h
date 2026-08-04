#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/gpio.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <array>

namespace esphome::ld6002b {

static constexpr uint8_t MAX_TARGETS = 3;
static constexpr size_t DEFAULT_MAX_DATA_LEN = 1024;
// Largest protocol payload is TYPE_SET_AREA: int32 area id + 6 floats = 28 bytes.
static constexpr size_t CMD_MAX_DATA_LEN = 28;

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
    if (target >= MAX_TARGETS)
      return;
    this->target_presence_[target] = sensor;
  }
#endif

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

  void queue_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void process_command_queue_();
  void send_command_(uint16_t type, const uint8_t *data, uint8_t len);
  void send_command_internal_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  void write_frame_(uint16_t type, const uint8_t *data, uint8_t len, bool track);
  void send_control_command_(uint32_t command);

  static uint16_t read_u16_be(const uint8_t *data);
  static uint32_t read_u32_le(const uint8_t *data);
  static void write_u32_le(uint8_t *data, uint32_t value);

#ifdef USE_BINARY_SENSOR
  binary_sensor::BinarySensor *presence_binary_sensor_{nullptr};
  std::array<binary_sensor::BinarySensor *, MAX_TARGETS> target_presence_{};
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
  bool frame_oversize_{false};
  uint8_t *data_buf_{nullptr};
  uint16_t next_frame_id_{0};

  // Sized for the boot burst: with every platform configured, setup() enqueues
  // roughly ten GET/config commands back to back before the first ack lands.
  static constexpr uint8_t CMD_QUEUE_SIZE = 16;
  static constexpr uint32_t CMD_ACK_TIMEOUT_MS = 300;
  // A sleeping module consumes the first frame to wake and answers only the one after it.
  static constexpr uint32_t CMD_FIRST_ACK_TIMEOUT_MS = 600;
  // How long the module stays awake after any frame, and so still answers the next one.
  static constexpr uint32_t MODULE_AWAKE_MS = 10000;
  static constexpr uint8_t CMD_MAX_RETRIES = 3;
  // A reply cannot trail the frame that earned it for longer than this; the field worst case is ~726ms.
  static constexpr uint32_t STALE_ACK_MAX_AGE_MS = 1000;

  std::array<PendingCommand, CMD_QUEUE_SIZE> cmd_queue_{};
  uint8_t cmd_head_{0};
  uint8_t cmd_tail_{0};
  uint8_t cmd_count_{0};
  bool command_active_{false};
  bool command_sent_{false};
  PendingCommand active_command_{};
  // Frame a pending wake pulse will write, snapshotted because active_command_ may move on first.
  std::array<uint8_t, CMD_MAX_DATA_LEN> wake_scratch_{};
  bool wake_pulse_pending_{false};
  uint8_t retries_left_{0};
  uint32_t last_send_ms_{0};
  // Last frame seen in either direction; any traffic keeps the module awake.
  uint32_t last_traffic_ms_{0};
  // Frames transmitted for the command in flight, including retries; drives the retry budget.
  uint8_t attempts_sent_{0};
  // Subset of those the module can actually answer: a frame that woke it is consumed, not replied to.
  uint8_t acks_expected_{0};
  // ACKs still owed for superseded attempts; they carry no id, only their arrival order.
  uint16_t stale_ack_type_{0};
  uint8_t stale_ack_count_{0};
  // When that debt was booked, so a debt no reply can still settle expires instead of eating a live ACK.
  uint32_t stale_ack_ms_{0};
  // Bumped whenever the active command changes, so a deferred send can tell it was retired.
  uint8_t send_generation_{0};

  // Which person owns each target_N slot, so a slot survives the module re-sorting its array.
  std::array<int32_t, MAX_TARGETS> slot_cluster_{};
  std::array<bool, MAX_TARGETS> slot_occupied_{};

  bool target_presence_any_{false};
};

}  // namespace esphome::ld6002b
