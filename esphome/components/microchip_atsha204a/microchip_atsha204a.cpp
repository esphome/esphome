#include "microchip_atsha204a.h"

#include <algorithm>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::microchip_atsha204a {

ESPHOME_LOG_TAG(TAG, "microchip_atsha204a");

// Microchip ATSHA204A datasheet DS40002025A.
// https://ww1.microchip.com/downloads/en/devicedoc/atsha204a-data-sheet-40002025a.pdf
static constexpr uint32_t WAKE_DELAY_US = 3000;
static constexpr uint32_t WATCHDOG_MIN_MS = 700;
static constexpr uint8_t STATUS_PACKET_SIZE = 4;
static constexpr uint8_t WAKE_STATUS = 0x11;
static constexpr uint8_t READ_32_BYTES = 0x80;
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 30;
static constexpr uint32_t SESSION_TIMEOUT_MS = 500;

static uint16_t packet_crc(const uint8_t *data, size_t length) {
  // The chip shifts the CRC toward the MSB while consuming each byte LSB first.
  return reverse_bits(crc16(data, length, 0));
}

// Maximum execution times from DS40002025A, table 8-4.
static constexpr uint32_t execution_ms(Opcode opcode) {
  switch (opcode) {
    case Opcode::OPCODE_PAUSE:
    case Opcode::OPCODE_DEV_REV:
      return 2;
    case Opcode::OPCODE_READ:
      return 4;
    case Opcode::OPCODE_UPDATE_EXTRA:
      return 12;
    case Opcode::OPCODE_SHA:
      return 22;
    case Opcode::OPCODE_LOCK:
      return 24;
    case Opcode::OPCODE_MAC:
      return 35;
    case Opcode::OPCODE_CHECK_MAC:
      return 38;
    case Opcode::OPCODE_WRITE:
      return 42;
    case Opcode::OPCODE_GEN_DIG:
      return 43;
    case Opcode::OPCODE_RANDOM:
      return 50;
    case Opcode::OPCODE_NONCE:
      return 60;
    case Opcode::OPCODE_DERIVE_KEY:
      return 62;
    case Opcode::OPCODE_HMAC:
      return 69;
  }
  return 0;
}

#ifdef USE_TEXT_SENSOR
template<size_t N> static void publish_hex(text_sensor::TextSensor *sensor, const std::array<uint8_t, N> &bytes) {
  if (sensor == nullptr)
    return;
  char result[format_hex_size(N)];
  format_hex_to(result, bytes);
  for (char &c : result) {
    if (c >= 'a' && c <= 'f')
      c -= 'a' - 'A';
  }
  publish_text_sensor(sensor, StringRef(result));
}

void publish_text_sensor(text_sensor::TextSensor *sensor, StringRef value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
    sensor->publish_state(value.empty() ? "" : value.c_str(), value.size());
}
#endif

void ATSHA204A::setup() {
  this->disable_loop();
  if (!this->transport_->setup()) {
    this->mark_failed(LOG_STR("Transport setup failed"));
    this->publish_presence_(false);
  }
}

void ATSHA204A::dump_config() {
  ESP_LOGCONFIG(TAG, "Microchip ATSHA204A:");
  this->transport_->dump_config();
  LOG_UPDATE_INTERVAL(this);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_binary_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Serial number", this->serial_number_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Chip revision", this->chip_revision_text_sensor_);
#endif
}

void ATSHA204A::update() {
  if (this->state_ == State::STATE_IDLE && !this->is_failed())
    this->start_wake_();
}

bool ATSHA204A::start_wake_() {
  this->state_ = State::STATE_WAKE;
  this->enable_loop();
  this->awake_ = this->transport_->wake();
  if (!this->awake_) {
    this->handle_response_(nullptr, IO_ERROR);
    return false;
  }
  this->wait_start_ = micros();
  this->woke_at_ = millis();
  this->wait_us_ = WAKE_DELAY_US;
  this->response_requested_ = false;
  return true;
}

bool ATSHA204A::send_frame_(Opcode opcode, uint8_t param1, uint16_t param2, const uint8_t *data, size_t size) {
  uint8_t packet[MAX_COMMAND];
  const size_t count = size + 7;
  packet[0] = count;
  packet[1] = static_cast<uint8_t>(opcode);
  packet[2] = param1;
  put_le16(packet + 3, param2);
  if (size != 0)
    std::copy_n(data, size, packet + 5);
  put_le16(packet + count - 2, packet_crc(packet, count - 2));
  if (!this->transport_->command(packet, count))
    return false;
  this->wait_start_ = micros();
  this->wait_us_ = execution_ms(opcode) * 1000;
  this->response_requested_ = false;
  return true;
}

void ATSHA204A::command(Opcode opcode, uint8_t param1, uint16_t param2, std::span<const uint8_t> data, EndMode end_mode,
                        uint32_t session, bool expect_response, ResultCallback callback) {
  if (execution_ms(opcode) == 0 || data.size() > MAX_COMMAND - 7 ||
      (!expect_response && (opcode != Opcode::OPCODE_PAUSE || end_mode != EndMode::END_MODE_SLEEP))) {
    if (callback)
      callback(INVALID_ARGUMENT, {}, 0);
    return;
  }
  if (this->is_failed()) {
    if (callback)
      callback(IO_ERROR, {}, 0);
    return;
  }
  if (session != 0 && (this->state_ != State::STATE_HELD || session != this->session_)) {
    if (callback)
      callback(SESSION_LOST, {}, 0);
    return;
  }
  if (session != 0 && this->session_expired_(millis())) {
    this->end_session(this->session_);
    if (callback)
      callback(SESSION_LOST, {}, 0);
    return;
  }
  if (session == 0 && this->state_ != State::STATE_IDLE) {
    if (callback)
      callback(BUSY, {}, 0);
    return;
  }
  if (session == 0) {
    if (++this->next_session_ == 0)
      ++this->next_session_;
    this->session_ = this->next_session_;
  }
  this->user_opcode_ = opcode;
  this->user_param1_ = param1;
  this->user_param2_ = param2;
  this->user_data_.assign(data.begin(), data.end());
  this->user_end_ = end_mode;
  this->expect_response_ = expect_response;
  this->result_callback_ = std::move(callback);
  this->user_active_ = true;
  if (this->awake_) {
    this->send_user_();
  } else {
    this->start_wake_();
  }
}

void ATSHA204A::send_user_() {
  this->state_ = State::STATE_USER;
  const bool sent = this->send_frame_(this->user_opcode_, this->user_param1_, this->user_param2_,
                                      this->user_data_.data(), this->user_data_.size());
  std::fill(this->user_data_.begin(), this->user_data_.end(), 0);
  this->user_data_.clear();
  if (!sent)
    this->finish_user_(IO_ERROR);
}

void ATSHA204A::read_block(Zone zone, uint16_t address, BlockCallback callback) {
  if (this->state_ != State::STATE_POLL_CALLBACK || !callback)
    return;
  if (!this->send_frame_(Opcode::OPCODE_READ, READ_32_BYTES | static_cast<uint8_t>(zone), address, nullptr, 0)) {
    this->finish_poll_(false);
    callback(nullptr);
    return;
  }
  this->state_ = State::STATE_BLOCK;
  this->block_callback_ = std::move(callback);
}

void ATSHA204A::loop() {
  if (this->state_ == State::STATE_IDLE)
    return;
  if (this->state_ == State::STATE_HELD) {
    if (this->session_expired_(App.get_loop_component_start_time()))
      this->end_session(this->session_);
    return;
  }
  if (micros() - this->wait_start_ < this->wait_us_)
    return;
  if (!this->response_requested_) {
    // On a shared single-wire bus, wake replies may collide before Pause selects a chip.
    if (this->state_ == State::STATE_WAKE && this->user_active_ && this->user_opcode_ == Opcode::OPCODE_PAUSE &&
        this->transport_->is_single_wire()) {
      this->send_user_();
      return;
    }
    if (this->state_ == State::STATE_USER && !this->expect_response_) {
      // Pause only idles chips whose selector does not match. Sleep any chip still awake.
      const bool sent = this->transport_->power_down(false, true);
      this->awake_ = false;
      this->finish_user_(sent ? UNCONFIRMED : IO_ERROR);
      return;
    }
    if (!this->transport_->request_response()) {
      this->handle_response_(nullptr, IO_ERROR);
      return;
    }
    this->response_requested_ = true;
    this->response_start_ = micros();
  }
  uint8_t packet[MAX_RESPONSE];
  switch (this->transport_->receive(packet)) {
    case Receive::RECEIVE_PENDING:
      if (micros() - this->response_start_ >= RESPONSE_TIMEOUT_MS * 1000)
        this->handle_response_(nullptr, TIMEOUT);
      return;
    case Receive::RECEIVE_IO_ERROR:
      this->handle_response_(nullptr, IO_ERROR);
      return;
    case Receive::RECEIVE_INVALID_LENGTH:
      this->handle_response_(nullptr, BAD_LENGTH);
      return;
    case Receive::RECEIVE_COMPLETE:
      break;
  }
  const size_t crc_offset = packet[0] - 2;
  if (packet_crc(packet, crc_offset) != encode_value_le<uint16_t>(packet + crc_offset)) {
    this->handle_response_(nullptr, BAD_CRC);
    return;
  }
  this->handle_response_(packet, packet[0] == STATUS_PACKET_SIZE ? packet[1] : 0);
}

void ATSHA204A::handle_response_(const uint8_t *packet, int status) {
  switch (this->state_) {
    case State::STATE_WAKE:
      if (this->closing_session_) {
        this->close_session_(status == WAKE_STATUS);
        return;
      }
      if (status != WAKE_STATUS) {
        if (this->user_active_) {
          this->finish_user_(status < 0 ? status : WAKE_FAILED);
        } else {
          this->finish_poll_(false);
        }
      } else if (this->user_active_) {
        this->send_user_();
      } else {
        this->state_ = State::STATE_REVISION;
        if (!this->send_frame_(Opcode::OPCODE_DEV_REV, 0, 0, nullptr, 0))
          this->finish_poll_(false);
      }
      return;
    case State::STATE_REVISION:
      if (packet == nullptr || packet[0] != 7) {
        this->finish_poll_(false);
        return;
      }
      std::copy_n(packet + 1, this->revision_.size(), this->revision_.begin());
      this->state_ = State::STATE_CONFIG;
      if (!this->send_frame_(Opcode::OPCODE_READ, READ_32_BYTES, 0, nullptr, 0))
        this->finish_poll_(false);
      return;
    case State::STATE_CONFIG: {
      if (packet == nullptr || packet[0] != MAX_RESPONSE) {
        this->finish_poll_(false);
        return;
      }
      std::array<uint8_t, 9> serial;
      std::copy_n(packet + 1, 4, serial.begin());
      std::copy_n(packet + 9, 5, serial.begin() + 4);
      if (this->has_serial_ && this->serial_ != serial) {
        this->finish_poll_(false);
        return;
      }
      if (!this->has_serial_) {
#ifdef USE_TEXT_SENSOR
        publish_hex(this->serial_number_text_sensor_, serial);
        publish_hex(this->chip_revision_text_sensor_, this->revision_);
#endif
        this->serial_ = serial;
        this->has_serial_ = true;
      }
      if (this->poll_callback_) {
        this->publish_presence_(true);
        this->state_ = State::STATE_POLL_CALLBACK;
        this->poll_callback_();
      }
      if (this->state_ == State::STATE_CONFIG || this->state_ == State::STATE_POLL_CALLBACK)
        this->finish_poll_(true);
      return;
    }
    case State::STATE_BLOCK: {
      auto callback = std::exchange(this->block_callback_, {});
      this->state_ = State::STATE_POLL_CALLBACK;
      callback(packet != nullptr && packet[0] == MAX_RESPONSE ? packet + 1 : nullptr);
      if (this->state_ == State::STATE_POLL_CALLBACK)
        this->finish_poll_(true);
      return;
    }
    case State::STATE_USER:
      this->finish_user_(status, packet != nullptr && packet[0] > STATUS_PACKET_SIZE ? packet + 1 : nullptr,
                         packet != nullptr && packet[0] > STATUS_PACKET_SIZE ? packet[0] - 3 : 0);
      return;
    case State::STATE_POLL_CALLBACK:
    case State::STATE_IDLE:
    case State::STATE_HELD:
      return;
  }
}

void ATSHA204A::finish_poll_(bool present) {
  const bool slept = !this->awake_ || this->transport_->power_down(false);
  this->awake_ = false;
  this->block_callback_ = {};
  if (!present || !slept) {
    if (this->has_serial_) {
      this->has_serial_ = false;
#ifdef USE_TEXT_SENSOR
      publish_text_sensor(this->serial_number_text_sensor_, {});
      publish_text_sensor(this->chip_revision_text_sensor_, {});
#endif
      if (this->disconnect_callback_)
        this->disconnect_callback_();
    }
  }
  this->set_idle_();
  this->publish_presence_(present && slept);
}

void ATSHA204A::publish_presence_(bool present) {
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->presence_binary_sensor_, present);
#else
  (void) present;
#endif
}

void ATSHA204A::finish_user_(int status, const uint8_t *data, size_t size) {
  if (status < 0 || this->user_end_ == EndMode::END_MODE_SLEEP) {
    if (this->awake_ && !this->transport_->power_down(false) && status == 0)
      status = IO_ERROR;
    this->awake_ = false;
    this->session_ = 0;
  } else if (this->awake_ && this->user_end_ == EndMode::END_MODE_IDLE) {
    if (!this->transport_->power_down(true)) {
      status = IO_ERROR;
      this->session_ = 0;
    }
    this->awake_ = false;
  }
  if (this->session_ == 0) {
    this->set_idle_();
  } else {
    this->state_ = State::STATE_HELD;
    this->held_at_ = millis();
  }
  this->user_active_ = false;
  std::fill(this->user_data_.begin(), this->user_data_.end(), 0);
  this->user_data_.clear();
  auto callback = std::exchange(this->result_callback_, {});
  if (callback) {
    CommandResponse response;
    if (size != 0)
      response.assign(data, data + size);
    callback(status, response, this->session_);
  }
}

bool ATSHA204A::close_session_(bool ready_to_sleep) {
  const bool sent = ready_to_sleep && this->transport_->power_down(false);
  if (!sent) {
    ESP_LOGW(TAG, "Could not put the chip to sleep while ending the session");
  }
  this->awake_ = false;
  this->session_ = 0;
  this->closing_session_ = false;
  this->set_idle_();
  return sent;
}

void ATSHA204A::set_idle_() {
  this->state_ = State::STATE_IDLE;
  this->disable_loop();
}

bool ATSHA204A::session_expired_(uint32_t now) const {
  // The loop timestamp can predate a command completed during the same tick.
  return static_cast<int32_t>(now - (this->held_at_ + SESSION_TIMEOUT_MS)) >= 0 ||
         (this->awake_ && static_cast<int32_t>(now - (this->woke_at_ + WATCHDOG_MIN_MS)) >= 0);
}

bool ATSHA204A::end_session(uint32_t session) {
  if (this->state_ != State::STATE_HELD || session == 0 || session != this->session_)
    return false;
  if (!this->awake_) {
    // An idle chip must be woken before Sleep can erase its retained state.
    this->closing_session_ = true;
    return this->start_wake_();
  }
  return this->close_session_(true);
}

}  // namespace esphome::microchip_atsha204a
