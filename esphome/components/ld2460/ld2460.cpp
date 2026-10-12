#include "ld2460.h"

#include "esphome/core/hal.h"

#include <cmath>
#include <cstring>
#include <numbers>

namespace esphome::ld2460 {

ESPHOME_LOG_TAG(TAG, "ld2460");

static constexpr uint8_t HEADER_FOOTER_SIZE = 4;
// PROGMEM keeps the patterns out of RAM on the ESP8266; they are copied out for each compare
static constexpr uint8_t CMD_FRAME_HEADER[HEADER_FOOTER_SIZE] PROGMEM = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[HEADER_FOOTER_SIZE] PROGMEM = {0x04, 0x03, 0x02, 0x01};
static constexpr uint8_t DATA_FRAME_HEADER[HEADER_FOOTER_SIZE] PROGMEM = {0xF4, 0xF3, 0xF2, 0xF1};
static constexpr uint8_t DATA_FRAME_FOOTER[HEADER_FOOTER_SIZE] PROGMEM = {0xF8, 0xF7, 0xF6, 0xF5};

static constexpr uint8_t CMD_REPORT_DATA = 0x04;
static constexpr uint8_t CMD_QUERY_INSTALLATION_MODE = 0x0A;
static constexpr uint8_t ACK_FRAME_LENGTH = 12;
// Header, command, length (12, little endian), payload 0x01, footer
static constexpr uint8_t QUERY_INSTALLATION_MODE_FRAME[] PROGMEM = {
    0xFD, 0xFC, 0xFB, 0xFA, CMD_QUERY_INSTALLATION_MODE, 0x0C, 0x00, 0x01, 0x04, 0x03, 0x02, 0x01,
};

static bool matches(const uint8_t *bytes, const uint8_t *pattern) {
  uint8_t copy[HEADER_FOOTER_SIZE];
  progmem_memcpy(copy, pattern, sizeof(copy));
  return std::memcmp(bytes, copy, sizeof(copy)) == 0;
}

static constexpr float RADIANS_TO_DEGREES = 180.0f / std::numbers::pi_v<float>;

void LD2460Component::setup() { this->send_query_(); }

void LD2460Component::send_query_() {
  uint8_t frame[sizeof(QUERY_INSTALLATION_MODE_FRAME)];
  progmem_memcpy(frame, QUERY_INSTALLATION_MODE_FRAME, sizeof(frame));
  this->write_array(frame, sizeof(frame));
}

void LD2460Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2460:");
#ifdef USE_SENSOR
  LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetCount", this->target_count_sensor_);
  for (const auto &t : this->target_sensors_) {
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetX", t.x);
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetY", t.y);
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetDistance", t.distance);
    LOG_SENSOR_WITH_DEDUP_SAFE("  ", "TargetAngle", t.angle);
  }
#endif
}

void LD2460Component::loop() {
  size_t avail = this->available();
  uint8_t buf[MAX_LINE_LENGTH];
  while (avail > 0) {
    size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      break;
    }
    avail -= to_read;

    for (size_t i = 0; i < to_read; i++) {
      this->readline_(buf[i]);
    }
  }
}

void LD2460Component::handle_periodic_data_() {
  const uint8_t *data = this->buffer_data_;
  if (!matches(data, DATA_FRAME_HEADER) || data[4] != CMD_REPORT_DATA) {
    ESP_LOGW(TAG, "Invalid periodic data header");
    return;
  }
  uint16_t packet_len = encode_uint16(data[6], data[5]);
  if (packet_len != this->buffer_pos_) {
    ESP_LOGW(TAG, "Periodic packet length mismatch: %u != %u", packet_len, this->buffer_pos_);
    return;
  }

  if (this->query_pending_) {
    // The module may have booted after us and missed the query sent from setup()
    this->query_pending_ = false;
    this->send_query_();
  }

#ifdef USE_SENSOR
  uint8_t num_targets = std::min<uint8_t>((packet_len - 11) / 4, MAX_TARGETS);
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    TargetSensors &t = this->target_sensors_[i];
    if (i >= num_targets) {
      t.last_x = INT16_MIN;
      t.last_y = INT16_MIN;
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.x);
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.y);
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.distance);
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.angle);
      continue;
    }
    const uint8_t *p = &data[7 + i * 4];
    auto raw_x = static_cast<int16_t>(encode_uint16(p[1], p[0]));
    auto raw_y = static_cast<int16_t>(encode_uint16(p[3], p[2]));
    if (raw_x == t.last_x && raw_y == t.last_y) {
      continue;
    }
    t.last_x = raw_x;
    t.last_y = raw_y;

    // Coordinates are reported in 0.1 m steps
    float x = raw_x * 0.1f;
    float y = raw_y * 0.1f;
    SAFE_PUBLISH_SENSOR(t.x, x);
    SAFE_PUBLISH_SENSOR(t.y, y);
    if (t.distance.has_sensor()) {
      SAFE_PUBLISH_SENSOR(t.distance, std::sqrt(x * x + y * y));
    }
    if (t.angle.has_sensor()) {
      float angle;
      if (this->installation_mode_ == InstallationMode::INSTALLATION_MODE_TOP) {
        angle = std::atan2(y, x) * RADIANS_TO_DEGREES;
        if (angle < 0.0f) {
          angle += 360.0f;
        }
      } else {
        angle = std::atan2(x, y) * RADIANS_TO_DEGREES;
      }
      SAFE_PUBLISH_SENSOR(t.angle, angle);
    }
  }
  SAFE_PUBLISH_SENSOR(this->target_count_sensor_, num_targets);
#endif

  this->data_callback_.call();
}

void LD2460Component::readline_(uint8_t readch) {
  if (this->buffer_pos_ >= MAX_LINE_LENGTH) {
    ESP_LOGW(TAG, "Max frame length exceeded; ignoring");
    this->buffer_pos_ = 0;
  }
  this->buffer_data_[this->buffer_pos_++] = readch;

  if (this->buffer_pos_ < HEADER_FOOTER_SIZE) {
    return;
  }
  const uint8_t *tail = &this->buffer_data_[this->buffer_pos_ - HEADER_FOOTER_SIZE];
  bool is_data = readch == progmem_read_byte(&DATA_FRAME_FOOTER[3]) && matches(tail, DATA_FRAME_FOOTER);
  if (!is_data && (readch != progmem_read_byte(&CMD_FRAME_FOOTER[3]) || !matches(tail, CMD_FRAME_FOOTER))) {
    return;
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex_buf[format_hex_pretty_size(MAX_LINE_LENGTH)];
  ESP_LOGV(TAG, "Frame: %s", format_hex_pretty_to(hex_buf, this->buffer_data_, this->buffer_pos_));
#endif

  if (is_data) {
    this->handle_periodic_data_();
  } else if (this->buffer_pos_ == ACK_FRAME_LENGTH &&
             encode_uint16(this->buffer_data_[6], this->buffer_data_[5]) == ACK_FRAME_LENGTH &&
             matches(this->buffer_data_, CMD_FRAME_HEADER) && this->buffer_data_[4] == CMD_QUERY_INSTALLATION_MODE) {
    this->installation_mode_ = static_cast<InstallationMode>(this->buffer_data_[7]);
    this->query_pending_ = false;
    ESP_LOGV(TAG, "Installation mode: %u", this->buffer_data_[7]);
#ifdef USE_SENSOR
    // Angles depend on the mode, so republish every target on the next frame
    for (auto &t : this->target_sensors_) {
      t.last_x = INT16_MIN;
    }
#endif
  }
  this->buffer_pos_ = 0;
}

}  // namespace esphome::ld2460
