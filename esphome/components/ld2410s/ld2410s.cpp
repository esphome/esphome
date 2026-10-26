#include "ld2410s.h"
#include "ld2410s_number.h"
#include <algorithm>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ld2410s {

ESPHOME_LOG_TAG(TAG, "ld2410s");

static constexpr uint8_t SHORT_DATA_FRAME_HEADER = 0x6E;
static constexpr uint8_t SHORT_DATA_FRAME_FOOTER[] PROGMEM = {0x62};
static constexpr uint8_t SHORT_DATA_FRAME_SIZE = 5;  // header, state, distance low, distance high, footer
static constexpr uint8_t STD_DATA_FRAME_HEADER[] PROGMEM = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t STD_DATA_FRAME_FOOTER[] PROGMEM = {0xF8, 0xF7, 0xF6, 0xF5};
static constexpr uint8_t CMD_FRAME_HEADER[] PROGMEM = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[] PROGMEM = {0x04, 0x03, 0x02, 0x01};
static constexpr uint8_t LONG_HEADER_SIZE = 4;
static constexpr uint8_t LENGTH_FIELD_SIZE = 2;
static constexpr uint8_t LONG_PAYLOAD_POS = LONG_HEADER_SIZE + LENGTH_FIELD_SIZE;

static constexpr uint16_t CMD_CONFIRMATION = 0x0100;  // set in the command word of an acknowledgement
static constexpr uint16_t CONFIG_MODE_START_CMD = 0x00FF;
static constexpr uint16_t CONFIG_MODE_START_VALUE = 0x0001;
static constexpr uint16_t CONFIG_MODE_END_CMD = 0x00FE;
static constexpr uint16_t VERSION_READ_CMD = 0x0000;
static constexpr uint16_t PARAMETERS_WRITE_CMD = 0x0070;
static constexpr uint16_t PARAMETERS_READ_CMD = 0x0071;
static constexpr uint16_t OUTPUT_MODE_SWITCH_CMD = 0x007A;
// Minimal output mode (short frames carrying presence and distance only) is selected by six zero bytes
static constexpr uint8_t OUTPUT_MODE_MINIMAL_SIZE = 6;
// The parameter write carries a 16-bit key and a 32-bit value per parameter, the read only the keys
static constexpr uint8_t PARAMETER_KEY_SIZE = 2;
static constexpr uint8_t PARAMETER_VALUE_SIZE = 4;
static constexpr uint8_t PARAMETERS_WRITE_SIZE = PARAMETER_COUNT * (PARAMETER_KEY_SIZE + PARAMETER_VALUE_SIZE);
static constexpr uint8_t CMD_FRAME_MAX_SIZE = LONG_PAYLOAD_POS + 2 + PARAMETERS_WRITE_SIZE + sizeof(CMD_FRAME_FOOTER);
// Keys of the parameters, in Parameter order
#ifdef USE_LD2410S_PARAMETERS
static constexpr uint16_t PARAMETER_KEYS[] PROGMEM = {0x0005, 0x000A, 0x0006, 0x0002, 0x000C, 0x000B};
static_assert(sizeof(PARAMETER_KEYS) / sizeof(PARAMETER_KEYS[0]) == PARAMETER_COUNT, "one key per parameter");
#endif
// The version answer: 4 bytes device type, then major, minor and patch as 16-bit words
static constexpr uint8_t VERSION_TYPE_SIZE = 4;
static constexpr uint8_t VERSION_SIZE = VERSION_TYPE_SIZE + 3 * 2;

// Everything the module needs at start: configuration mode on, read the version and parameters when
// an entity shows them, pick the output mode, configuration mode off
static constexpr uint16_t INIT_SEQUENCE[] PROGMEM = {
    CONFIG_MODE_START_CMD,
#ifdef USE_LD2410S_VERSION
    VERSION_READ_CMD,
#endif
#ifdef USE_LD2410S_PARAMETERS
    PARAMETERS_READ_CMD,
#endif
    OUTPUT_MODE_SWITCH_CMD, CONFIG_MODE_END_CMD,
};
#ifdef USE_LD2410S_PARAMETERS
static constexpr uint16_t WRITE_SEQUENCE[] PROGMEM = {CONFIG_MODE_START_CMD, PARAMETERS_WRITE_CMD, CONFIG_MODE_END_CMD};
#endif
static constexpr uint8_t INIT_SEQUENCE_LENGTH = sizeof(INIT_SEQUENCE) / sizeof(INIT_SEQUENCE[0]);
#ifdef USE_LD2410S_PARAMETERS
static constexpr uint8_t WRITE_SEQUENCE_LENGTH = sizeof(WRITE_SEQUENCE) / sizeof(WRITE_SEQUENCE[0]);
#endif
static constexpr uint8_t STEP_MAX_TIMEOUTS = 2;
static constexpr uint32_t ACK_TIMEOUT_MS = 300;
static constexpr uint32_t COMMAND_GAP_MS = 300;     // pause after an acknowledgement before the next command
static constexpr uint32_t REINIT_PAUSE_MS = 15000;  // pause before starting over after the module stopped answering
static constexpr uint32_t LINK_TIMEOUT_MS = 5000;   // the module streams frames continuously; silence means it is gone

static constexpr uint8_t DATA_TYPE_STANDARD = 0x01;

void LD2410SComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2410S:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Has Target", this->target_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR_WITH_DEDUP("  ", "Distance", this->distance_sensor_);
#endif
#ifdef USE_LD2410S_VERSION
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
#endif
#if defined(USE_NUMBER) && defined(USE_LD2410S_PARAMETERS)
  for (auto *number : this->numbers_) {
    LOG_NUMBER("  ", "Setting", number);
  }
#endif
#if defined(USE_SELECT) && defined(USE_LD2410S_PARAMETERS)
  LOG_SELECT("  ", "Response Speed", this->response_speed_select_);
#endif
}

void LD2410SComponent::loop() {
  uint8_t chunk[16];
  // Take the count once so one pass handles a bounded number of bytes
  size_t avail = this->available();
  while (avail > 0) {
    const size_t len = std::min(avail, sizeof(chunk));
    if (!this->read_array(chunk, len)) {
      ESP_LOGV(TAG, "UART read of %u bytes failed", static_cast<unsigned>(len));
      break;
    }
    for (size_t i = 0; i < len; i++) {
      this->receive_byte_(chunk[i]);
    }
    avail -= len;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (this->sequence_ == nullptr) {
    if (!this->init_done_) {
      this->start_sequence_(INIT_SEQUENCE, INIT_SEQUENCE_LENGTH, now);
#ifdef USE_LD2410S_PARAMETERS
    } else if (this->parameters_dirty_) {
      this->parameters_dirty_ = false;
      // Keep the pause after the last acknowledgement; a start time further ahead than that is long past,
      // and after 2^31 ms the run_sequence_() check would read it as the future
      const bool in_gap = this->next_send_at_ - now <= COMMAND_GAP_MS;
      this->start_sequence_(WRITE_SEQUENCE, WRITE_SEQUENCE_LENGTH, in_gap ? this->next_send_at_ : now);
#endif
    }
  }
  this->run_sequence_(now);
  if (this->init_done_ && now - this->last_frame_at_ > LINK_TIMEOUT_MS) {
    // The count tells real silence apart from frames that never parse (wrong baud rate or firmware)
    ESP_LOGW(TAG, "No data for %" PRIu32 " s (%u frames dropped), starting over", LINK_TIMEOUT_MS / 1000,
             this->dropped_frames_);
    this->dropped_frames_ = 0;
    this->restart_init_(now);
  }
}

#ifdef USE_LD2410S_PARAMETERS
bool LD2410SComponent::set_parameter(Parameter parameter, uint8_t value) {
  // All six are written together, so the other five must be the module's own values
  if (!this->init_done_) {
    ESP_LOGW(TAG, "Parameters not read yet, change ignored");
    return false;
  }
  if (!this->parameters_known_) {
    ESP_LOGW(TAG, "The module reported a parameter out of range, changes are disabled");
    return false;
  }
  this->parameters_[static_cast<uint8_t>(parameter)] = value;
  this->parameters_dirty_ = true;  // written by loop() once no sequence is running
  return true;
}
#endif

void LD2410SComponent::restart_init_(uint32_t at) {
  this->init_done_ = false;
  this->status_set_warning();  // cleared again when the new init sequence is acknowledged
#ifdef USE_LD2410S_PARAMETERS
  // The module's own values count again once it is set up anew
  if (this->parameters_dirty_) {
    ESP_LOGW(TAG, "Dropping the pending parameter change, the module's values are read back");
  }
  this->parameters_dirty_ = false;
  this->parameters_known_ = false;  // a refused value may sit in parameters_ until the read replaces it
#endif
  this->start_sequence_(INIT_SEQUENCE, INIT_SEQUENCE_LENGTH, at);
}

void LD2410SComponent::start_sequence_(const uint16_t *sequence, uint8_t length, uint32_t now) {
  this->sequence_ = sequence;
  this->sequence_len_ = length;
  this->step_ = 0;
  this->step_timeouts_ = 0;
  this->awaiting_ack_ = false;
  this->next_send_at_ = now;
}

uint16_t LD2410SComponent::current_command_() const { return progmem_read_uint16(&this->sequence_[this->step_]); }

void LD2410SComponent::run_sequence_(uint32_t now) {
  if (this->sequence_ == nullptr || static_cast<int32_t>(now - this->next_send_at_) < 0) {
    return;
  }
  const uint16_t command = this->current_command_();
  if (this->awaiting_ack_) {
    if (++this->step_timeouts_ > STEP_MAX_TIMEOUTS) {
      ESP_LOGW(TAG, "No acknowledgement for command %04X, starting over in %" PRIu32 " s", command,
               REINIT_PAUSE_MS / 1000);
      // Leave configuration mode whatever state the module is in, then begin again
      this->send_command_(CONFIG_MODE_END_CMD);
      this->restart_init_(now + REINIT_PAUSE_MS);
      return;
    }
    ESP_LOGD(TAG, "No acknowledgement for command %04X, resending", command);
  }
  this->send_command_(command);
  this->awaiting_ack_ = true;
  this->next_send_at_ = now + ACK_TIMEOUT_MS;
}

void LD2410SComponent::send_command_(uint16_t command) {
  uint8_t frame[CMD_FRAME_MAX_SIZE];
  progmem_memcpy(frame, CMD_FRAME_HEADER, LONG_HEADER_SIZE);
  uint8_t pos = LONG_PAYLOAD_POS;
  frame[pos++] = command & 0xFF;
  frame[pos++] = command >> 8;
  switch (command) {
    case OUTPUT_MODE_SWITCH_CMD:
      memset(&frame[pos], 0, OUTPUT_MODE_MINIMAL_SIZE);
      pos += OUTPUT_MODE_MINIMAL_SIZE;
      break;
    case CONFIG_MODE_START_CMD:
      frame[pos++] = CONFIG_MODE_START_VALUE & 0xFF;
      frame[pos++] = CONFIG_MODE_START_VALUE >> 8;
      break;
#ifdef USE_LD2410S_PARAMETERS
    case PARAMETERS_READ_CMD:
    case PARAMETERS_WRITE_CMD:
      for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
        const uint16_t key = progmem_read_uint16(&PARAMETER_KEYS[i]);
        frame[pos++] = key & 0xFF;
        frame[pos++] = key >> 8;
        if (command == PARAMETERS_WRITE_CMD) {
          const uint32_t value = this->parameters_[i];  // a 32-bit field on the wire
          for (uint8_t shift = 0; shift < 32; shift += 8) {
            frame[pos++] = (value >> shift) & 0xFF;
          }
        }
      }
      break;
#endif
    default:
      break;
  }
  frame[LONG_HEADER_SIZE] = pos - LONG_PAYLOAD_POS;  // payload length, little-endian
  frame[LONG_HEADER_SIZE + 1] = 0;
  progmem_memcpy(&frame[pos], CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER));
  pos += sizeof(CMD_FRAME_FOOTER);
  this->write_array(frame, pos);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char hex_buf[format_hex_pretty_size(CMD_FRAME_MAX_SIZE)];
  ESP_LOGVV(TAG, "> %s", format_hex_pretty_to(hex_buf, frame, pos, ' '));
#endif
}

void LD2410SComponent::receive_byte_(uint8_t byte) {
  if (this->rx_len_ >= RX_BUFFER_SIZE) {
    this->reset_frame_();
  }
  this->rx_buffer_[this->rx_len_++] = byte;

  if (this->frame_type_ == FrameType::NONE) {
    if (this->rx_len_ == 1 && byte == SHORT_DATA_FRAME_HEADER) {
      this->frame_type_ = FrameType::SHORT_DATA;
      this->expected_len_ = SHORT_DATA_FRAME_SIZE;
      return;
    }
    // The two long headers are told apart once all four bytes are in
    uint8_t std_header[LONG_HEADER_SIZE];
    uint8_t cmd_header[LONG_HEADER_SIZE];
    progmem_memcpy(std_header, STD_DATA_FRAME_HEADER, LONG_HEADER_SIZE);
    progmem_memcpy(cmd_header, CMD_FRAME_HEADER, LONG_HEADER_SIZE);
    const bool std_prefix = memcmp(this->rx_buffer_, std_header, this->rx_len_) == 0;
    const bool cmd_prefix = memcmp(this->rx_buffer_, cmd_header, this->rx_len_) == 0;
    if (std_prefix || cmd_prefix) {
      if (this->rx_len_ == LONG_HEADER_SIZE) {
        this->frame_type_ = std_prefix ? FrameType::STD_DATA : FrameType::COMMAND;
      }
      return;
    }
    // Not a header: drop what was collected, and try this byte as the start of the next frame
    const bool retry = this->rx_len_ > 1;
    this->reset_frame_();
    if (retry) {
      this->receive_byte_(byte);
    }
    return;
  }

  if (this->expected_len_ == 0) {
    if (this->rx_len_ < LONG_PAYLOAD_POS) {
      return;
    }
    // Standard data and command frames carry a little-endian payload length after the header
    const uint16_t payload_len =
        encode_uint16(this->rx_buffer_[LONG_HEADER_SIZE + 1], this->rx_buffer_[LONG_HEADER_SIZE]);
    if (payload_len > RX_BUFFER_SIZE - LONG_PAYLOAD_POS - sizeof(CMD_FRAME_FOOTER)) {
      ESP_LOGV(TAG, "Dropping frame with a %u byte payload, larger than the receive buffer", payload_len);
      this->count_dropped_frame_();
      this->reset_frame_();
      return;
    }
    this->expected_len_ = LONG_PAYLOAD_POS + payload_len + sizeof(CMD_FRAME_FOOTER);
    return;
  }

  if (this->rx_len_ < this->expected_len_) {
    return;
  }
  if (this->handle_frame_() || this->rescanning_) {
    this->reset_frame_();
    return;
  }
  // A truncated long frame swallowed the bytes that followed it; try them again as new frames, once
  uint8_t rest[RX_BUFFER_SIZE];
  const uint8_t rest_len = this->rx_len_ - 1;
  memcpy(rest, &this->rx_buffer_[1], rest_len);
  this->reset_frame_();
  this->rescanning_ = true;
  for (uint8_t i = 0; i < rest_len; i++) {
    this->receive_byte_(rest[i]);
  }
  this->rescanning_ = false;
}

bool LD2410SComponent::handle_frame_() {
  const uint8_t *footer_src;
  uint8_t footer_size = sizeof(CMD_FRAME_FOOTER);
  uint8_t payload_pos = LONG_PAYLOAD_POS;
  switch (this->frame_type_) {
    case FrameType::SHORT_DATA:
      footer_src = SHORT_DATA_FRAME_FOOTER;
      footer_size = sizeof(SHORT_DATA_FRAME_FOOTER);
      payload_pos = 1;
      break;
    case FrameType::STD_DATA:
      footer_src = STD_DATA_FRAME_FOOTER;
      break;
    default:
      footer_src = CMD_FRAME_FOOTER;
      break;
  }
  uint8_t footer[sizeof(CMD_FRAME_FOOTER)];
  progmem_memcpy(footer, footer_src, footer_size);
  if (memcmp(&this->rx_buffer_[this->rx_len_ - footer_size], footer, footer_size) != 0) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(RX_BUFFER_SIZE)];
    ESP_LOGV(TAG, "Footer does not match header: %s",
             format_hex_pretty_to(hex_buf, this->rx_buffer_, this->rx_len_, ' '));
#endif
    this->count_dropped_frame_();
    return false;
  }
  const uint8_t *payload = &this->rx_buffer_[payload_pos];
  const uint16_t payload_len = this->rx_len_ - payload_pos - footer_size;
  switch (this->frame_type_) {
    case FrameType::SHORT_DATA:
    case FrameType::STD_DATA:
      if (!this->handle_data_frame_(payload, payload_len)) {
        this->count_dropped_frame_();
        break;
      }
      this->last_frame_at_ = App.get_loop_component_start_time();
      this->dropped_frames_ = 0;
      break;
    default:
      this->handle_command_ack_(payload, payload_len);
      break;
  }
  return true;
}

bool LD2410SComponent::handle_data_frame_(const uint8_t *payload, uint16_t len) {
  // Short frames carry [state][distance low][distance high]; standard frames put a type byte first, and the
  // module only sends them before the init sequence switches it to minimal output
  const uint8_t *data = payload;
  if (this->frame_type_ == FrameType::STD_DATA) {
    if (len < 4 || payload[0] != DATA_TYPE_STANDARD) {
      if (len >= 1) {
        ESP_LOGV(TAG, "Ignoring data frame type %02X", payload[0]);
      }
      return true;
    }
    data = payload + 1;
  }
  const uint8_t state = data[0];
  const uint16_t distance = encode_uint16(data[2], data[1]);
  // States 0 and 1 mean no target, 2 and 3 a target; anything else is line noise that happened to
  // carry the header and footer bytes
  if (state > 3) {
    ESP_LOGV(TAG, "Dropping frame with impossible state %u", state);
    return false;
  }
  this->publish_target_(state > 1, distance);
  return true;
}

void LD2410SComponent::handle_command_ack_(const uint8_t *payload, uint16_t len) {
  if (len < 4) {
    ESP_LOGV(TAG, "Acknowledgement too short (%u bytes)", len);
    return;
  }
  const uint16_t command_word = encode_uint16(payload[1], payload[0]);
  const uint16_t ack = encode_uint16(payload[3], payload[2]);
  if (!this->awaiting_ack_) {
    ESP_LOGV(TAG, "Unexpected acknowledgement %04X", command_word);
    return;
  }
  const uint16_t command = this->current_command_();
  if (command_word != (command | CMD_CONFIRMATION)) {
    ESP_LOGV(TAG, "Unexpected acknowledgement %04X", command_word);
    return;
  }
#ifdef USE_LD2410S_PARAMETERS
  if (ack != 0 && command == PARAMETERS_WRITE_CMD) {
    // Resending the same values would be rejected again; leave configuration mode and read the
    // module's own values back, which republishes them
    ESP_LOGW(TAG, "Module rejected the parameters, ack %04X", ack);
    this->send_command_(CONFIG_MODE_END_CMD);
    this->restart_init_(App.get_loop_component_start_time() + COMMAND_GAP_MS);
    return;
  }
#endif
  if (ack != 0) {
    // Leave the command pending so the timeout resends it
    ESP_LOGW(TAG, "Module rejected command %04X, ack %04X", command, ack);
    return;
  }
  // A short answer leaves the read pending the same way, so init cannot finish without the values
#ifdef USE_LD2410S_PARAMETERS
  if (command == PARAMETERS_READ_CMD && !this->handle_parameters_(&payload[4], len - 4))
    return;
#endif
#ifdef USE_LD2410S_VERSION
  if (command == VERSION_READ_CMD && !this->handle_version_(&payload[4], len - 4))
    return;
#endif
  this->awaiting_ack_ = false;
  this->step_timeouts_ = 0;
  const uint32_t now = App.get_loop_component_start_time();
  this->next_send_at_ = now + COMMAND_GAP_MS;
  if (++this->step_ < this->sequence_len_) {
    return;
  }
  const bool was_init = this->sequence_ == INIT_SEQUENCE;
  this->sequence_ = nullptr;
  if (was_init) {
    ESP_LOGD(TAG, "Setup done");
    this->init_done_ = true;
    this->last_frame_at_ = now;
    this->status_clear_warning();
  }
}

#ifdef USE_LD2410S_PARAMETERS
// The answer lists the parameter values in key order, 32 bits each; returns false when it is cut short
bool LD2410SComponent::handle_parameters_(const uint8_t *values, uint16_t len) {
  if (len < PARAMETER_COUNT * PARAMETER_VALUE_SIZE) {
    ESP_LOGW(TAG, "Parameter answer too short (%u bytes)", len);
    return false;
  }
  // Every parameter must fit the low byte of its little-endian value; otherwise nothing is stored or
  // published and writes stay refused, so a cut-down value is never shown or sent back
  for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
    const uint8_t *v = &values[i * PARAMETER_VALUE_SIZE];
    if ((v[1] | v[2] | v[3]) != 0) {
      ESP_LOGW(TAG, "Parameter %u out of range: %" PRIu32 ", changes stay disabled", i,
               encode_uint32(v[3], v[2], v[1], v[0]));
      this->parameters_known_ = false;
#ifdef USE_NUMBER
      // A value a number showed before the module answered is not confirmed
      for (auto *number : this->numbers_) {
        if (number != nullptr) {
          number->publish_state(NAN);
        }
      }
#endif
      return true;
    }
  }
  for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
    this->parameters_[i] = values[i * PARAMETER_VALUE_SIZE];
  }
  this->parameters_known_ = true;
  this->publish_parameters_();
  ESP_LOGD(TAG, "Parameters: gates %u to %u, no delay %u s, frequencies %u/%u dHz, response speed %u",
           this->parameters_[1], this->parameters_[0], this->parameters_[2], this->parameters_[3], this->parameters_[4],
           this->parameters_[5]);
  return true;
}
#endif

#ifdef USE_LD2410S_VERSION
// Returns false when the answer is cut short
bool LD2410SComponent::handle_version_(const uint8_t *values, uint16_t len) {
  if (len < VERSION_SIZE) {
    ESP_LOGW(TAG, "Version answer too short (%u bytes)", len);
    return false;
  }
  const uint8_t *v = &values[VERSION_TYPE_SIZE];
  char version[18];
  snprintf(version, sizeof(version), "%u.%u.%u", encode_uint16(v[1], v[0]), encode_uint16(v[3], v[2]),
           encode_uint16(v[5], v[4]));
  ESP_LOGD(TAG, "Firmware %s", version);
  if (this->version_text_sensor_ != nullptr) {
    this->version_text_sensor_->publish_state(version);
  }
  return true;
}
#endif

void LD2410SComponent::publish_target_(bool target, uint16_t distance) {
#ifdef USE_BINARY_SENSOR
  if (this->target_binary_sensor_ != nullptr) {
    this->target_binary_sensor_->publish_state(target);
  }
#endif
#ifdef USE_SENSOR
  // The module keeps reporting the last distance after the target left
  SAFE_PUBLISH_SENSOR(this->distance_sensor_, target ? distance : 0);
#endif
}

#ifdef USE_LD2410S_PARAMETERS
void LD2410SComponent::publish_parameters_() {
#ifdef USE_NUMBER
  for (uint8_t i = 0; i < PARAMETER_COUNT; i++) {
    if (this->numbers_[i] != nullptr) {
      this->numbers_[i]->publish_raw(this->parameters_[i]);
    }
  }
#endif
#ifdef USE_SELECT
  if (this->response_speed_select_ != nullptr) {
    const uint8_t speed = this->parameters_[static_cast<uint8_t>(Parameter::PARAMETER_RESPONSE_SPEED)];
    for (uint8_t i = 0; i < RESPONSE_SPEED_COUNT; i++) {
      if (progmem_read_byte(&RESPONSE_SPEEDS[i]) == speed) {
        this->response_speed_select_->publish_state(i);
        return;
      }
    }
    ESP_LOGV(TAG, "Unknown response speed %u", speed);
  }
#endif
}
#endif

}  // namespace esphome::ld2410s
