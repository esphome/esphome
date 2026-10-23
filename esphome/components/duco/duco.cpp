#include "duco.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::duco {

ESPHOME_LOG_TAG(TAG, "duco");

static constexpr uint8_t HEADER_START = 0xAA;
static constexpr uint8_t HEADER_END = 0x55;
static constexpr uint8_t STUFFING = 0x01;  // follows an 0xAA inside a message, not part of it
static constexpr uint32_t FRAME_GAP_MS = 50;

static constexpr uint32_t DISCOVERY_START_ID = 0;
static constexpr uint32_t DISCOVERY_STEP_ID = 1;
static constexpr uint32_t DISCOVERY_START_MS = 16000;
static constexpr uint32_t DISCOVERY_STEP_MS = 1600;
static constexpr uint8_t DISCOVERY_MAX_ATTEMPTS = 3;

void Duco::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  // Read first: after a slow loop pass the rest of a message is already waiting, which is not silence
  this->read_available([this, now](uint8_t byte) {
    if (this->parse_byte_(byte)) {
      this->last_byte_ = now;
    } else {
      this->reset_frame_();
    }
  });

  // The last message of a burst has no header after it, so it ends on silence
  if (this->rx_size_ != 0 && now - this->last_byte_ > FRAME_GAP_MS) {
    this->finalize_message_();
    this->reset_frame_();
  }
}

void Duco::reset_frame_() {
  this->rx_size_ = 0;
  this->escaped_ = false;
}

void Duco::close_request(DucoDevice *device) {
#ifdef DUCO_DEVICE_COUNT
  for (auto &pending : this->pending_) {
    if (pending.device == device) {
      pending.function = 0;
      pending.missed = false;
      this->update_warning_();
      return;
    }
  }
#endif
}

bool Duco::parse_byte_(uint8_t byte) {
  const size_t at = this->rx_size_;
  if (at >= MAX_FRAME_SIZE) {
    ESP_LOGV(TAG, "Message longer than %u bytes dropped", static_cast<unsigned>(MAX_FRAME_SIZE));
    return false;
  }
  const uint8_t prev = at > 0 ? this->rx_buffer_[at - 1] : 0;

  if (at == 1 && prev == HEADER_START && byte == HEADER_END)
    return false;  // header of the next message

  if (at >= 2) {
    const bool escaped = this->escaped_;
    this->escaped_ = false;
    if (prev == HEADER_START && !escaped) {
      if (byte == STUFFING) {
        this->escaped_ = true;
        return true;
      }
      if (at >= 3 && byte == HEADER_END) {
        this->rx_size_--;  // drop the 0xAA of the header
        this->finalize_message_();
        return false;
      }
    }
  }

  this->rx_buffer_[this->rx_size_++] = byte;
  return true;
}

void Duco::finalize_message_() {
  const uint8_t *raw = this->rx_buffer_.data();
  // length (function, id and data), function, id, data, CRC low, CRC high
  if (this->rx_size_ < 5 || raw[0] < 2 || this->rx_size_ < raw[0] + 3u) {
    ESP_LOGV(TAG, "Malformed message dropped: %u bytes, length byte %u", this->rx_size_, raw[0]);
    return;
  }
  const uint8_t length = raw[0];

  const uint16_t computed_crc = crc16(raw, length + 1);
  const uint16_t remote_crc = encode_uint16(raw[length + 2], raw[length + 1]);
  if (computed_crc != remote_crc) {
    if (!this->disable_crc_) {
      ESP_LOGV(TAG, "CRC check failed: %04X != %04X", computed_crc, remote_crc);
      return;
    }
    ESP_LOGV(TAG, "CRC check failed, ignored: %04X != %04X", computed_crc, remote_crc);
  }

#ifdef DUCO_DEVICE_COUNT
  // The box first acknowledges a request with function + 1, then answers with function + 2
  const DucoMessage message{raw[2], raw + 3, static_cast<uint8_t>(length - 2)};
  for (auto &pending : this->pending_) {
    if (pending.function != 0 && pending.id == message.id && pending.function + 2 == raw[1]) {
      pending.function = 0;
      pending.missed = false;
      this->update_warning_();
      pending.device->receive_response(message);
      return;
    }
  }
  // Acknowledgements (odd function codes) are expected and never match
  if ((raw[1] & 1) == 0) {
    ESP_LOGV(TAG, "Answer %02X with id %u matches no open request", raw[1], message.id);
  }
#endif
}

#ifdef DUCO_DEVICE_COUNT
void Duco::update_warning_() {
  for (const auto &pending : this->pending_) {
    if (pending.missed) {
      this->status_set_warning(LOG_STR("No answer from the box"));
      return;
    }
  }
  this->status_clear_warning();
}
#endif

void Duco::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Duco:\n"
                "  CRC Disabled: %s",
                YESNO(this->disable_crc_));
}

uint8_t Duco::next_id_() {
  do {
    this->last_id_++;
  } while (this->last_id_ == HEADER_START || this->last_id_ == HEADER_END);
  return this->last_id_;
}

void Duco::send(DucoDevice *device, uint8_t function, std::initializer_list<uint8_t> data) {
  if (data.size() > MAX_REQUEST_DATA) {
    ESP_LOGE(TAG, "Request of %u bytes is too long", static_cast<unsigned>(data.size()));
    return;
  }
  // Not byte stuffed, as in the author's original and no capture shows a request with 0xAA. An 0xAA followed by any
  // byte but 0x01 or 0x55 reads the same with or without unstuffing on the box, so only those rare pairs are
  // unverified.
  const size_t size = data.size();
  const uint8_t id = this->next_id_();
  uint8_t frame[5 + MAX_REQUEST_DATA + 2] = {HEADER_START, HEADER_END, static_cast<uint8_t>(size + 2), function, id};
  std::copy_n(data.begin(), size, frame + 5);
  const uint16_t crc = crc16(frame + 2, size + 3);
  frame[size + 5] = crc & 0xFF;
  frame[size + 6] = crc >> 8;
  this->write_array(frame, size + 7);
  this->flush();

#ifdef DUCO_DEVICE_COUNT
  for (auto &pending : this->pending_) {
    if (pending.device == device) {
      pending.missed = pending.function != 0;
      if (pending.missed) {
        ESP_LOGV(TAG, "No answer to request %02X with id %u", pending.function, pending.id);
        this->update_warning_();
      }
      pending.id = id;
      pending.function = function;
      break;
    }
  }
#endif

  ESP_LOGV_HEX(TAG, frame + 2, size + 5, sizeof(frame), "Sent %s");
}

static const LogString *node_type_name(uint8_t type) {
  switch (type) {
    case 8:
      return LOG_STR("UCBAT");
    case 9:
      return LOG_STR("UC");
    case 10:
      return LOG_STR("UCRH");
    case 12:
      return LOG_STR("UCCO2");
    case 13:
      return LOG_STR("VLV");
    case 17:
      return LOG_STR("BOX");
    case 18:
      return LOG_STR("SWITCH");
    default:
      return LOG_STR("UNKNOWN");
  }
}

void DucoDiscovery::setup() {
  // Give the box time to start before walking its node addresses
  this->set_timeout(DISCOVERY_START_ID, DISCOVERY_START_MS, [this]() {
    this->set_interval(DISCOVERY_STEP_ID, DISCOVERY_STEP_MS, [this]() { this->update(); });
  });
}

void DucoDiscovery::update() {
  if (this->attempts_ == DISCOVERY_MAX_ATTEMPTS) {
    ESP_LOGV(TAG, "No answer for node %u", this->next_node_);
    this->next_node_++;
    this->attempts_ = 0;
  }
  if (this->next_node_ >= NODE_COUNT) {
    this->cancel_interval(DISCOVERY_STEP_ID);
    this->parent_->close_request(this);  // discovery never sends again, so a last miss must not stay a warning
    return;
  }
  // Without an answer by the next step the same node is asked again
  ESP_LOGV(TAG, "Discovering node %u", this->next_node_);
  this->attempts_++;
  this->parent_->send(this, FUNCTION_NODE, {0x01, this->next_node_});
}

void DucoDiscovery::receive_response(const DucoMessage &message) {
  // Only the latest attempt is open, so a late answer to an earlier one never arrives here
  if (message.size >= 1 && this->next_node_ < NODE_COUNT) {
    if (message.data[0] != 0) {
      ESP_LOGV(TAG, "Found node %u: type %u", this->next_node_, message.data[0]);
    }
    this->node_types_[this->next_node_++] = message.data[0];
    this->attempts_ = 0;
  }
}

void DucoDiscovery::dump_config() {
  ESP_LOGCONFIG(TAG, "Discovered nodes:");
  for (uint8_t node = 0; node < NODE_COUNT; node++) {
    const uint8_t type = this->node_types_[node];
    if (type != 0) {
      ESP_LOGCONFIG(TAG, "  Node %u: type %u (%s)", node, type, LOG_STR_ARG(node_type_name(type)));
    }
  }
  if (this->next_node_ < NODE_COUNT) {
    ESP_LOGCONFIG(TAG, "  Still discovering, at node %u", this->next_node_);
  }
}

#ifdef USE_TIME
void DucoTime::update() {
  const ESPTime now = this->clock_->now();
  if (!now.is_valid()) {
    ESP_LOGV(TAG, "Time is not valid yet");
    return;
  }
  const uint32_t ts = now.timestamp;
  this->parent_->send(nullptr, FUNCTION_PARAMETER,
                      {0x05, static_cast<uint8_t>(ts), static_cast<uint8_t>(ts >> 8), static_cast<uint8_t>(ts >> 16),
                       static_cast<uint8_t>(ts >> 24), 0x00, 0x00, 0x00, 0x00});
}
#endif

}  // namespace esphome::duco
