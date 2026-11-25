#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "transport.h"

#include <array>
#include <functional>
#include <utility>
#include <span>

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/string_ref.h"
#endif

namespace esphome::microchip_atsha204a {

#ifdef USE_TEXT_SENSOR
void publish_text_sensor(text_sensor::TextSensor *sensor, StringRef value);

template<typename T>
  requires requires(const T &value) {
    value.data();
    value.size();
  }
void publish_text_sensor(text_sensor::TextSensor *sensor, const T &value) {
  publish_text_sensor(sensor, StringRef(value.data(), value.size()));
}
#endif

enum class Opcode : uint8_t {
  OPCODE_PAUSE = 0x01,
  OPCODE_READ = 0x02,
  OPCODE_MAC = 0x08,
  OPCODE_HMAC = 0x11,
  OPCODE_WRITE = 0x12,
  OPCODE_GEN_DIG = 0x15,
  OPCODE_NONCE = 0x16,
  OPCODE_LOCK = 0x17,
  OPCODE_RANDOM = 0x1B,
  OPCODE_DERIVE_KEY = 0x1C,
  OPCODE_UPDATE_EXTRA = 0x20,
  OPCODE_CHECK_MAC = 0x28,
  OPCODE_DEV_REV = 0x30,
  OPCODE_SHA = 0x47,
};
using CommandResponse = StaticVector<uint8_t, MAX_RESPONSE - 3>;

enum class EndMode : uint8_t { END_MODE_SLEEP, END_MODE_IDLE, END_MODE_KEEP_AWAKE };
enum HostStatus : int {
  BUSY = -1,
  TIMEOUT = -2,
  BAD_CRC = -3,
  BAD_LENGTH = -4,
  IO_ERROR = -5,
  SESSION_LOST = -6,
  INVALID_ARGUMENT = -7,
  WAKE_FAILED = -8,
  UNCONFIRMED = -9,
};

class ATSHA204A : public PollingComponent {
 public:
  enum class Zone : uint8_t { CONFIG = 0, OTP = 1, DATA = 2 };
  using BlockCallback = std::function<void(const uint8_t *)>;
  using ResultCallback = std::function<void(int, const CommandResponse &, uint32_t)>;

  explicit ATSHA204A(Transport *transport) : transport_(transport) {}
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(presence)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(serial_number)
  SUB_TEXT_SENSOR(chip_revision)
#endif
  template<typename F> void set_poll_callback(F &&callback) { this->poll_callback_ = std::forward<F>(callback); }
  template<typename F> void set_disconnect_callback(F &&callback) {
    this->disconnect_callback_ = std::forward<F>(callback);
  }

  // Only callable inside the poll or block callback. Payload is valid for that callback only.
  void read_block(Zone zone, uint16_t address, BlockCallback callback);
  void command(Opcode opcode, uint8_t param1, uint16_t param2, std::span<const uint8_t> data, EndMode end_mode,
               uint32_t session, bool expect_response, ResultCallback callback);
  // Returns whether Sleep was sent or a wake before Sleep was started.
  bool end_session(uint32_t session);

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

 protected:
  enum class State : uint8_t {
    STATE_IDLE,
    STATE_WAKE,
    STATE_REVISION,
    STATE_CONFIG,
    STATE_POLL_CALLBACK,
    STATE_BLOCK,
    STATE_USER,
    STATE_HELD
  };

  bool start_wake_();
  bool send_frame_(Opcode opcode, uint8_t param1, uint16_t param2, const uint8_t *data, size_t size);
  void send_user_();
  void handle_response_(const uint8_t *packet, int status);
  void finish_poll_(bool present);
  void set_idle_();
  void publish_presence_(bool present);
  void finish_user_(int status, const uint8_t *data = nullptr, size_t size = 0);
  bool close_session_(bool ready_to_sleep);
  bool session_expired_(uint32_t now) const;

  Transport *const transport_;
  State state_{State::STATE_IDLE};
  uint32_t wait_start_{};
  uint32_t wait_us_{};
  uint32_t response_start_{};
  bool response_requested_{};
  bool user_active_{};
  bool closing_session_{};
  bool awake_{};
  bool has_serial_{};
  bool expect_response_{};
  std::array<uint8_t, 9> serial_{};
  std::array<uint8_t, 4> revision_{};
  std::function<void()> poll_callback_;
  std::function<void()> disconnect_callback_;
  BlockCallback block_callback_;
  ResultCallback result_callback_;
  Opcode user_opcode_{};
  uint8_t user_param1_{};
  uint16_t user_param2_{};
  StaticVector<uint8_t, MAX_COMMAND - 7> user_data_;
  EndMode user_end_{};
  uint32_t next_session_{};
  uint32_t session_{};
  uint32_t held_at_{};
  uint32_t woke_at_{};
};

}  // namespace esphome::microchip_atsha204a
