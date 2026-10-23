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

void Duco::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_byte_ > FRAME_GAP_MS) {
    // The last message of a burst has no header after it, so it ends on silence
    this->finalize_message_();
    this->rx_size_ = 0;
    this->last_byte_ = now;
  }

  this->read_available([this, now](uint8_t byte) {
    if (this->parse_byte_(byte)) {
      this->last_byte_ = now;
    } else {
      this->rx_size_ = 0;
    }
  });
}

bool Duco::parse_byte_(uint8_t byte) {
  const size_t at = this->rx_size_;
  if (at >= MAX_FRAME_SIZE)
    return false;
  const uint8_t prev = at > 0 ? this->rx_buffer_[at - 1] : 0;

  if (at == 1 && prev == HEADER_START && byte == HEADER_END)
    return false;  // header of the next message

  if (at >= 2) {
    if (prev == HEADER_START && byte == STUFFING && !this->removed_last_) {
      this->removed_last_ = true;
      return true;
    }
    this->removed_last_ = false;

    if (at >= 3 && prev == HEADER_START && byte == HEADER_END) {
      this->rx_size_--;  // drop the 0xAA of the header
      this->finalize_message_();
      return false;
    }
  }

  this->rx_buffer_[this->rx_size_++] = byte;
  return true;
}

void Duco::finalize_message_() {
  const uint8_t *raw = this->rx_buffer_.data();
  // length (function, id and data), function, id, data, CRC low, CRC high
  if (this->rx_size_ < 5 || raw[0] < 2 || this->rx_size_ < raw[0] + 3u)
    return;
  const uint8_t length = raw[0];

  const uint16_t computed_crc = crc16(raw, length + 1);
  const uint16_t remote_crc = encode_uint16(raw[length + 2], raw[length + 1]);
  if (computed_crc != remote_crc) {
    if (!this->disable_crc_) {
      ESP_LOGW(TAG, "CRC check failed: %04X != %04X", computed_crc, remote_crc);
      return;
    }
    ESP_LOGD(TAG, "CRC check failed, ignored: %04X != %04X", computed_crc, remote_crc);
  }

  const DucoMessage message{raw[1], raw[2], raw + 3, static_cast<uint8_t>(length - 2)};
  const uint8_t slot = message.id % PENDING_SLOTS;
  DucoDevice *device = this->pending_device_[slot];
  if (device == nullptr || this->pending_id_[slot] != message.id)
    return;
  if (device->receive_response(message))
    this->pending_device_[slot] = nullptr;
}

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

uint8_t Duco::send(DucoDevice *device, uint8_t function, std::initializer_list<uint8_t> data) {
  const size_t size = std::min(data.size(), MAX_REQUEST_DATA);
  const uint8_t id = this->next_id_();
  uint8_t frame[5 + MAX_REQUEST_DATA + 2] = {HEADER_START, HEADER_END, static_cast<uint8_t>(size + 2), function, id};
  std::copy_n(data.begin(), size, frame + 5);
  const uint16_t crc = crc16(frame + 2, size + 3);
  frame[size + 5] = crc & 0xFF;
  frame[size + 6] = crc >> 8;
  this->write_array(frame, size + 7);
  this->flush();

  // A request that was never answered gives up its slot to the one sent PENDING_SLOTS later
  const uint8_t slot = id % PENDING_SLOTS;
  this->pending_device_[slot] = device;
  this->pending_id_[slot] = id;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex[format_hex_pretty_size(sizeof(frame))];
  ESP_LOGV(TAG, "Sent %s", format_hex_pretty_to(hex, frame + 2, size + 5, ' '));
#endif
  return id;
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
    this->set_interval(DISCOVERY_STEP_ID, DISCOVERY_STEP_MS, [this]() { this->request_next_node_(); });
  });
}

void DucoDiscovery::request_next_node_() {
  if (this->next_node_ >= NODE_COUNT) {
    this->cancel_interval(DISCOVERY_STEP_ID);
    return;
  }
  // Without an answer by the next step the same node is asked again
  ESP_LOGD(TAG, "Discovering node %u", this->next_node_);
  this->request_id_ = this->parent_->send(this, FUNCTION_NODE, {0x01, this->next_node_});
  this->waiting_ = true;
}

bool DucoDiscovery::receive_response(const DucoMessage &message) {
  if (message.function != FUNCTION_NODE + 2)
    return false;
  if (this->waiting_ && message.id == this->request_id_ && message.size >= 1) {
    this->node_types_[this->next_node_++] = message.data[0];
    this->waiting_ = false;
  }
  return true;
}

void DucoDiscovery::update() {
  ESP_LOGI(TAG, "Discovered nodes:");
  for (uint8_t node = 0; node < NODE_COUNT; node++) {
    const uint8_t type = this->node_types_[node];
    if (type != 0) {
      ESP_LOGI(TAG, "  Node %u: type %u (%s)", node, type, LOG_STR_ARG(node_type_name(type)));
    }
  }
}

#ifdef USE_TIME
void DucoTime::update() {
  const ESPTime now = this->clock_->now();
  if (!now.is_valid()) {
    ESP_LOGW(TAG, "Time is not valid yet");
    return;
  }
  const uint32_t ts = now.timestamp;
  this->parent_->send(this, FUNCTION_PARAMETER,
                      {0x05, static_cast<uint8_t>(ts), static_cast<uint8_t>(ts >> 8), static_cast<uint8_t>(ts >> 16),
                       static_cast<uint8_t>(ts >> 24), 0x00, 0x00, 0x00, 0x00});
}
#endif

}  // namespace esphome::duco
