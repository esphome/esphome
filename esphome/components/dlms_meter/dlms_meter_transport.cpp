#include "dlms_meter.h"

#include <algorithm>

namespace esphome::dlms_meter {

static constexpr const char *TAG = "dlms_meter";
static constexpr uint8_t HDLC_FLAG = 0x7E;
static constexpr size_t HDLC_HEADER_FORMAT_OFFSET = 1;
static constexpr size_t HDLC_MIN_FRAME_BYTES = 5;
static constexpr uint8_t HDLC_LLC_DEST = 0xE6;
static constexpr uint8_t HDLC_LLC_SRC = 0xE7;
static constexpr uint8_t HDLC_LLC_QUALITY = 0x00;
static constexpr size_t HDLC_LLC_LENGTH = 3;
static constexpr size_t HDLC_FCS_LENGTH = 2;
static constexpr uint16_t HDLC_LENGTH_MASK = 0x07FF;
static constexpr size_t HDLC_MAX_RECEIVE_LENGTH = 600;

TransportType DlmsMeterComponent::detect_transport_() const {
  if (this->receive_buffer_.empty()) {
    return TransportType::UNKNOWN;
  }

  if (this->receive_buffer_[0] == START_BYTE_LONG_FRAME) {
    if (this->receive_buffer_.size() < MBUS_HEADER_INTRO_LENGTH) {
      return TransportType::UNKNOWN;
    }
    if (this->receive_buffer_[MBUS_START2_OFFSET] == START_BYTE_LONG_FRAME &&
        this->receive_buffer_[MBUS_LENGTH1_OFFSET] == this->receive_buffer_[MBUS_LENGTH2_OFFSET]) {
      return TransportType::MBUS;
    }
  }

  if (this->receive_buffer_[0] == HDLC_FLAG) {
    if (this->receive_buffer_.size() < HDLC_MIN_FRAME_BYTES) {
      return TransportType::UNKNOWN;
    }

    const uint8_t format_type = this->receive_buffer_[HDLC_HEADER_FORMAT_OFFSET] & 0xF0;
    if (format_type != 0xA0 && format_type != 0xA1) {
      return TransportType::UNKNOWN;
    }

    const uint16_t frame_length = encode_uint16(this->receive_buffer_[HDLC_HEADER_FORMAT_OFFSET],
                                                this->receive_buffer_[HDLC_HEADER_FORMAT_OFFSET + 1]) &
                                  HDLC_LENGTH_MASK;
    if (frame_length <= HDLC_LLC_LENGTH + HDLC_FCS_LENGTH || frame_length + 1 > HDLC_MAX_RECEIVE_LENGTH) {
      return TransportType::UNKNOWN;
    }
    return TransportType::HDLC;
  }

  return TransportType::UNKNOWN;
}

bool DlmsMeterComponent::uses_hdlc_transport_() const { return this->detect_transport_() == TransportType::HDLC; }

size_t DlmsMeterComponent::max_receive_length_() const {
  switch (this->detect_transport_()) {
    case TransportType::HDLC:
      return HDLC_MAX_RECEIVE_LENGTH;
    case TransportType::MBUS:
      return MBUS_MAX_FRAME_LENGTH * 2;
    case TransportType::UNKNOWN:
    default:
      return std::max(HDLC_MAX_RECEIVE_LENGTH, static_cast<size_t>(MBUS_MAX_FRAME_LENGTH * 2));
  }
}

bool DlmsMeterComponent::parse_mbus_(std::vector<uint8_t> &mbus_payload) {
  ESP_LOGV(TAG, "Parsing M-Bus frames");
  uint16_t frame_offset = 0;

  while (frame_offset < this->receive_buffer_.size()) {
    if (this->receive_buffer_.size() - frame_offset < MBUS_HEADER_INTRO_LENGTH) {
      ESP_LOGE(TAG, "MBUS: Not enough data for frame header (need %d, have %d)", MBUS_HEADER_INTRO_LENGTH,
               (this->receive_buffer_.size() - frame_offset));
      this->receive_buffer_.clear();
      return false;
    }

    if (this->receive_buffer_[frame_offset + MBUS_START1_OFFSET] != START_BYTE_LONG_FRAME ||
        this->receive_buffer_[frame_offset + MBUS_START2_OFFSET] != START_BYTE_LONG_FRAME) {
      frame_offset++;
      continue;
    }

    if (this->receive_buffer_[frame_offset + MBUS_LENGTH1_OFFSET] !=
        this->receive_buffer_[frame_offset + MBUS_LENGTH2_OFFSET]) {
      frame_offset++;
      continue;
    }

    uint8_t frame_length = this->receive_buffer_[frame_offset + MBUS_LENGTH1_OFFSET];
    if (this->receive_buffer_.size() - frame_offset < frame_length + 3) {
      ESP_LOGE(TAG, "MBUS: Frame too big for received data");
      this->receive_buffer_.clear();
      return false;
    }

    size_t required_total = frame_length + MBUS_HEADER_INTRO_LENGTH + MBUS_FOOTER_LENGTH;
    if (this->receive_buffer_.size() - frame_offset < required_total) {
      ESP_LOGE(TAG, "MBUS: Incomplete frame (need %d, have %d)", (unsigned int) required_total,
               this->receive_buffer_.size() - frame_offset);
      this->receive_buffer_.clear();
      return false;
    }

    if (this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH + MBUS_FOOTER_LENGTH - 1] !=
        STOP_BYTE) {
      frame_offset++;
      continue;
    }

    uint8_t checksum = 0;
    for (uint16_t i = 0; i < frame_length; i++) {
      checksum += this->receive_buffer_[frame_offset + MBUS_HEADER_INTRO_LENGTH + i];
    }
    if (checksum != this->receive_buffer_[frame_offset + frame_length + MBUS_HEADER_INTRO_LENGTH]) {
      frame_offset++;
      continue;
    }

    mbus_payload.insert(mbus_payload.end(), &this->receive_buffer_[frame_offset + MBUS_FULL_HEADER_LENGTH],
                        &this->receive_buffer_[frame_offset + MBUS_HEADER_INTRO_LENGTH + frame_length]);
    frame_offset += MBUS_HEADER_INTRO_LENGTH + frame_length + MBUS_FOOTER_LENGTH;
  }

  if (mbus_payload.empty()) {
    ESP_LOGE(TAG, "MBUS: No valid frame found");
    this->receive_buffer_.clear();
    return false;
  }
  return true;
}

bool DlmsMeterComponent::parse_hdlc_(std::vector<uint8_t> &dlms_payload) {
  ESP_LOGV(TAG, "Parsing HDLC frame");
  static constexpr std::array<uint8_t, HDLC_LLC_LENGTH> HDLC_LLC_HEADER = {HDLC_LLC_DEST, HDLC_LLC_SRC,
                                                                           HDLC_LLC_QUALITY};

  const auto frame_start = std::find(this->receive_buffer_.begin(), this->receive_buffer_.end(), HDLC_FLAG);
  if (frame_start == this->receive_buffer_.end()) {
    ESP_LOGE(TAG, "HDLC: Start flag not found");
    this->receive_buffer_.clear();
    return false;
  }

  const size_t frame_start_offset = std::distance(this->receive_buffer_.begin(), frame_start);
  if (this->receive_buffer_.size() - frame_start_offset < HDLC_MIN_FRAME_BYTES) {
    ESP_LOGE(TAG, "HDLC: Not enough data for header");
    this->receive_buffer_.clear();
    return false;
  }

  const uint16_t frame_length =
      encode_uint16(this->receive_buffer_[frame_start_offset + HDLC_HEADER_FORMAT_OFFSET],
                    this->receive_buffer_[frame_start_offset + HDLC_HEADER_FORMAT_OFFSET + 1]) &
      HDLC_LENGTH_MASK;
  if (frame_length <= HDLC_LLC_LENGTH + HDLC_FCS_LENGTH) {
    ESP_LOGE(TAG, "HDLC: Invalid frame length %u", frame_length);
    this->receive_buffer_.clear();
    return false;
  }

  const size_t frame_end_offset = frame_start_offset + frame_length + 1;
  if (frame_end_offset >= this->receive_buffer_.size()) {
    ESP_LOGE(TAG, "HDLC: Incomplete frame (need %u bytes, have %u)", static_cast<unsigned int>(frame_end_offset + 1),
             static_cast<unsigned int>(this->receive_buffer_.size()));
    this->receive_buffer_.clear();
    return false;
  }

  if (this->receive_buffer_[frame_end_offset] != HDLC_FLAG) {
    ESP_LOGE(TAG, "HDLC: End flag missing at expected offset %u", static_cast<unsigned int>(frame_end_offset));
    this->receive_buffer_.clear();
    return false;
  }

  const auto frame_end = this->receive_buffer_.begin() + static_cast<std::ptrdiff_t>(frame_end_offset);
  const auto llc = std::search(frame_start + 1, frame_end, HDLC_LLC_HEADER.begin(), HDLC_LLC_HEADER.end());
  if (llc == frame_end) {
    ESP_LOGE(TAG, "HDLC: LLC header not found");
    this->receive_buffer_.clear();
    return false;
  }

  const auto payload_start = llc + HDLC_LLC_LENGTH;
  if (frame_end - payload_start <= static_cast<std::ptrdiff_t>(HDLC_FCS_LENGTH)) {
    ESP_LOGE(TAG, "HDLC: No DLMS payload after LLC header");
    this->receive_buffer_.clear();
    return false;
  }

  dlms_payload.assign(payload_start, frame_end - HDLC_FCS_LENGTH);
  return true;
}

}  // namespace esphome::dlms_meter
