#include "ld2410s.h"
#include <algorithm>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::ld2410s {

ESPHOME_LOG_TAG(TAG, "ld2410s");

static constexpr uint8_t SHORT_DATA_FRAME_HEADER = 0x6E;
static constexpr uint8_t SHORT_DATA_FRAME_FOOTER[] = {0x62};
static constexpr uint8_t SHORT_DATA_FRAME_SIZE = 5;  // header, state, distance low, distance high, footer
static constexpr uint8_t STD_DATA_FRAME_HEADER[] = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t STD_DATA_FRAME_FOOTER[] = {0xF8, 0xF7, 0xF6, 0xF5};
static constexpr uint8_t CMD_FRAME_HEADER[] = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[] = {0x04, 0x03, 0x02, 0x01};
static constexpr uint8_t LONG_HEADER_SIZE = 4;
static constexpr uint8_t LENGTH_FIELD_SIZE = 2;
static constexpr uint8_t LONG_PAYLOAD_POS = LONG_HEADER_SIZE + LENGTH_FIELD_SIZE;

static constexpr uint16_t CMD_CONFIRMATION = 0x0100;  // set in the command word of an acknowledgement
static constexpr uint16_t CONFIG_MODE_START_CMD = 0x00FF;
static constexpr uint16_t CONFIG_MODE_START_VALUE = 0x0001;
static constexpr uint16_t CONFIG_MODE_END_CMD = 0x00FE;
static constexpr uint16_t OUTPUT_MODE_SWITCH_CMD = 0x007A;
// Minimal output mode: short frames carrying presence and distance only
static constexpr uint8_t OUTPUT_MODE_MINIMAL[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static constexpr uint8_t CMD_FRAME_MAX_SIZE =
    LONG_PAYLOAD_POS + 2 + sizeof(OUTPUT_MODE_MINIMAL) + sizeof(CMD_FRAME_FOOTER);

// Everything the module needs at start: enter configuration mode, pick the output mode, leave it
static constexpr uint16_t INIT_SEQUENCE[] = {CONFIG_MODE_START_CMD, OUTPUT_MODE_SWITCH_CMD, CONFIG_MODE_END_CMD};
static constexpr uint8_t INIT_SEQUENCE_LENGTH = sizeof(INIT_SEQUENCE) / sizeof(INIT_SEQUENCE[0]);
static constexpr uint8_t INIT_MAX_TIMEOUTS = 2;
static constexpr uint32_t ACK_TIMEOUT_MS = 300;
static constexpr uint32_t COMMAND_GAP_MS = 300;     // pause after an acknowledgement before the next command
static constexpr uint32_t REINIT_PAUSE_MS = 15000;  // pause before starting over after the module stopped answering

static constexpr uint8_t DATA_TYPE_STANDARD = 0x01;

void LD2410S::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2410S:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Presence", this->presence_binary_sensor_);
#endif
}

void LD2410S::loop() {
  uint8_t chunk[16];
  // Take the count once so one pass handles a bounded number of bytes
  size_t avail = this->available();
  while (avail > 0) {
    const size_t len = std::min(avail, sizeof(chunk));
    if (!this->read_array(chunk, len)) {
      break;
    }
    for (size_t i = 0; i < len; i++) {
      this->receive_byte_(chunk[i]);
    }
    avail -= len;
  }
  this->run_init_sequence_(App.get_loop_component_start_time());
}

void LD2410S::run_init_sequence_(uint32_t now) {
  if (this->init_step_ >= INIT_SEQUENCE_LENGTH || static_cast<int32_t>(now - this->next_send_at_) < 0) {
    return;
  }
  const uint16_t command = INIT_SEQUENCE[this->init_step_];
  if (this->awaiting_ack_) {
    if (++this->init_timeouts_ > INIT_MAX_TIMEOUTS) {
      ESP_LOGW(TAG, "No acknowledgement for command %04X, starting over in %" PRIu32 " s", command,
               REINIT_PAUSE_MS / 1000);
      // Leave configuration mode whatever state the module is in, then begin again
      this->send_command_(CONFIG_MODE_END_CMD);
      this->init_step_ = 0;
      this->init_timeouts_ = 0;
      this->awaiting_ack_ = false;
      this->next_send_at_ = now + REINIT_PAUSE_MS;
      return;
    }
    ESP_LOGD(TAG, "No acknowledgement for command %04X, resending", command);
  }
  this->set_init_done_(false);
  this->send_command_(command);
  this->awaiting_ack_ = true;
  this->next_send_at_ = now + ACK_TIMEOUT_MS;
}

void LD2410S::send_command_(uint16_t command) {
  uint8_t frame[CMD_FRAME_MAX_SIZE];
  memcpy(frame, CMD_FRAME_HEADER, LONG_HEADER_SIZE);
  uint8_t pos = LONG_PAYLOAD_POS;
  frame[pos++] = command & 0xFF;
  frame[pos++] = command >> 8;
  switch (command) {
    case OUTPUT_MODE_SWITCH_CMD:
      memcpy(&frame[pos], OUTPUT_MODE_MINIMAL, sizeof(OUTPUT_MODE_MINIMAL));
      pos += sizeof(OUTPUT_MODE_MINIMAL);
      break;
    case CONFIG_MODE_START_CMD:
      frame[pos++] = CONFIG_MODE_START_VALUE & 0xFF;
      frame[pos++] = CONFIG_MODE_START_VALUE >> 8;
      break;
    default:
      break;
  }
  frame[LONG_HEADER_SIZE] = pos - LONG_PAYLOAD_POS;  // payload length, little-endian
  frame[LONG_HEADER_SIZE + 1] = 0;
  memcpy(&frame[pos], CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER));
  pos += sizeof(CMD_FRAME_FOOTER);
  this->write_array(frame, pos);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char hex_buf[format_hex_pretty_size(CMD_FRAME_MAX_SIZE)];
  ESP_LOGVV(TAG, "> %s", format_hex_pretty_to(hex_buf, frame, pos, ' '));
#endif
}

void LD2410S::receive_byte_(uint8_t byte) {
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
    const bool std_prefix = memcmp(this->rx_buffer_, STD_DATA_FRAME_HEADER, this->rx_len_) == 0;
    const bool cmd_prefix = memcmp(this->rx_buffer_, CMD_FRAME_HEADER, this->rx_len_) == 0;
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
      this->reset_frame_();
      return;
    }
    this->expected_len_ = LONG_PAYLOAD_POS + payload_len + sizeof(CMD_FRAME_FOOTER);
    return;
  }

  if (this->rx_len_ >= this->expected_len_) {
    this->handle_frame_();
    this->reset_frame_();
  }
}

void LD2410S::handle_frame_() {
  const uint8_t *footer;
  uint8_t payload_pos;
  switch (this->frame_type_) {
    case FrameType::SHORT_DATA:
      footer = SHORT_DATA_FRAME_FOOTER;
      payload_pos = 1;
      break;
    case FrameType::STD_DATA:
      footer = STD_DATA_FRAME_FOOTER;
      payload_pos = LONG_PAYLOAD_POS;
      break;
    default:
      footer = CMD_FRAME_FOOTER;
      payload_pos = LONG_PAYLOAD_POS;
      break;
  }
  const uint8_t footer_size = this->frame_type_ == FrameType::SHORT_DATA ? 1 : sizeof(CMD_FRAME_FOOTER);
  if (memcmp(&this->rx_buffer_[this->rx_len_ - footer_size], footer, footer_size) != 0) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char hex_buf[format_hex_pretty_size(RX_BUFFER_SIZE)];
    ESP_LOGV(TAG, "Footer does not match header: %s",
             format_hex_pretty_to(hex_buf, this->rx_buffer_, this->rx_len_, ' '));
#endif
    return;
  }
  const uint8_t *payload = &this->rx_buffer_[payload_pos];
  const uint16_t payload_len = this->rx_len_ - payload_pos - footer_size;
  switch (this->frame_type_) {
    case FrameType::SHORT_DATA:
      // [state][distance low][distance high]; states 0 and 1 mean no target
      this->publish_presence_(payload[0] > 1);
      break;
    case FrameType::STD_DATA:
      this->handle_data_frame_(payload, payload_len);
      break;
    default:
      this->handle_command_ack_(payload, payload_len);
      break;
  }
}

void LD2410S::handle_data_frame_(const uint8_t *payload, uint16_t len) {
  // [type][state][distance low][distance high]...; the module only sends these before the init
  // sequence switches it to minimal output
  if (len >= 4 && payload[0] == DATA_TYPE_STANDARD) {
    this->publish_presence_(payload[1] > 1);
  } else if (len >= 1) {
    ESP_LOGV(TAG, "Ignoring data frame type %02X", payload[0]);
  }
}

void LD2410S::handle_command_ack_(const uint8_t *payload, uint16_t len) {
  if (len < 4) {
    return;
  }
  const uint16_t command_word = encode_uint16(payload[1], payload[0]);
  const uint16_t ack = encode_uint16(payload[3], payload[2]);
  if (!this->awaiting_ack_ || command_word != (INIT_SEQUENCE[this->init_step_] | CMD_CONFIRMATION)) {
    ESP_LOGV(TAG, "Unexpected acknowledgement %04X", command_word);
    return;
  }
  if (ack != 0) {
    // Leave the command pending so the timeout resends it
    ESP_LOGW(TAG, "Module rejected command %04X, ack %04X", INIT_SEQUENCE[this->init_step_], ack);
    return;
  }
  this->awaiting_ack_ = false;
  this->init_timeouts_ = 0;
  this->init_step_++;
  this->next_send_at_ = App.get_loop_component_start_time() + COMMAND_GAP_MS;
  if (this->init_step_ >= INIT_SEQUENCE_LENGTH) {
    this->set_init_done_(true);
  }
}

void LD2410S::set_init_done_(bool done) {
  if (done == this->init_done_) {
    return;
  }
  this->init_done_ = done;
  if (done) {
    ESP_LOGD(TAG, "Setup done");
    this->status_clear_warning();
  } else {
    this->status_set_warning();
  }
}

void LD2410S::publish_presence_(bool presence) {
#ifdef USE_BINARY_SENSOR
  if (this->presence_binary_sensor_ != nullptr) {
    this->presence_binary_sensor_->publish_state(presence);
  }
#endif
}

}  // namespace esphome::ld2410s
