#include "ld2460.h"

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <cmath>
#include <cstring>
#include <numbers>

namespace esphome::ld2460 {

ESPHOME_LOG_TAG(TAG, "ld2460");

static constexpr uint8_t FOOTER_SIZE = 4;
// PROGMEM keeps the patterns out of RAM on the ESP8266; they are copied out for each compare
static constexpr uint8_t CMD_FRAME_FOOTER[FOOTER_SIZE] PROGMEM = {0x04, 0x03, 0x02, 0x01};
static constexpr uint8_t DATA_FRAME_FOOTER[FOOTER_SIZE] PROGMEM = {0xF8, 0xF7, 0xF6, 0xF5};
static constexpr uint8_t DATA_FRAME_FIRST = 0xF4;
// Header (4), command (1), length (2), footer (4)
static constexpr uint8_t MIN_FRAME_LENGTH = 11;

static constexpr uint8_t CMD_REPORT_DATA = 0x04;
static constexpr uint8_t CMD_QUERY_INSTALLATION_MODE = 0x0A;
static constexpr uint8_t ACK_FRAME_LENGTH = 12;
static constexpr uint8_t MAX_QUERY_ATTEMPTS = 3;
// Header, command, length (12, little endian), payload 0x01, footer
static constexpr uint8_t QUERY_INSTALLATION_MODE_FRAME[] PROGMEM = {
    0xFD, 0xFC, 0xFB, 0xFA, CMD_QUERY_INSTALLATION_MODE, 0x0C, 0x00, 0x01, 0x04, 0x03, 0x02, 0x01,
};

static bool has_footer(const uint8_t *frame, size_t len) {
  if (len < MIN_FRAME_LENGTH)
    return false;
  uint8_t footer[FOOTER_SIZE];
  progmem_memcpy(footer, frame[0] == DATA_FRAME_FIRST ? DATA_FRAME_FOOTER : CMD_FRAME_FOOTER, sizeof(footer));
  return std::memcmp(&frame[len - FOOTER_SIZE], footer, sizeof(footer)) == 0;
}

static constexpr float RADIANS_TO_DEGREES = 180.0f / std::numbers::pi_v<float>;

void LD2460Component::setup() { this->send_query_(); }

void LD2460Component::send_query_() {
  uint8_t frame[sizeof(QUERY_INSTALLATION_MODE_FRAME)];
  progmem_memcpy(frame, QUERY_INSTALLATION_MODE_FRAME, sizeof(frame));
  this->write_array(frame, sizeof(frame));
}

void LD2460Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LD2460:\n"
                "  Installation mode: %s",
                this->query_attempts_ != 0                                            ? LOG_STR_LITERAL("unknown")
                : this->installation_mode_ == InstallationMode::INSTALLATION_MODE_TOP ? LOG_STR_LITERAL("top")
                                                                                      : LOG_STR_LITERAL("side"));
#ifdef USE_SENSOR
  LOG_SENSOR_WITH_DEDUP("  ", "TargetCount", this->target_count_sensor_);
  for (const auto &t : this->target_sensors_) {
    LOG_SENSOR_WITH_DEDUP("  ", "TargetX", t.x);
    LOG_SENSOR_WITH_DEDUP("  ", "TargetY", t.y);
    LOG_SENSOR_WITH_DEDUP("  ", "TargetDistance", t.distance);
    LOG_SENSOR_WITH_DEDUP("  ", "TargetAngle", t.angle);
  }
#endif
}

void LD2460Component::loop() {
  this->read_available<MAX_FRAME_LENGTH>([this](uint8_t b) { this->readline_(b); });
}

void LD2460Component::handle_periodic_data_(const uint8_t *data, uint8_t packet_len) {
  if (data[4] != CMD_REPORT_DATA) {
    ESP_LOGW(TAG, "Invalid periodic data header");
    return;
  }
  if ((packet_len - MIN_FRAME_LENGTH) % 4 != 0) {
    ESP_LOGW(TAG, "Periodic packet length %u is not a whole number of targets", packet_len);
    return;
  }

  if (this->query_attempts_ != 0) {
    if (this->query_attempts_ < MAX_QUERY_ATTEMPTS) {
      // The module may have booted after us and missed the earlier query
      this->query_attempts_++;
      this->send_query_();
    } else if (this->query_attempts_ == MAX_QUERY_ATTEMPTS) {
      this->query_attempts_++;
      ESP_LOGW(TAG, "No installation mode reply after %u queries; assuming side mount", MAX_QUERY_ATTEMPTS);
    }
  }

#ifdef USE_SENSOR
  uint8_t num_targets = std::min<uint8_t>((packet_len - MIN_FRAME_LENGTH) / 4, MAX_TARGETS);
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
    auto raw_x = encode_value_le<int16_t>(p);
    auto raw_y = encode_value_le<int16_t>(&p[2]);
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
  const uint8_t *frame = this->reader_.feed(readch);
  while (frame != nullptr) {
    const uint8_t len = this->reader_.frame_length();
    if (!has_footer(frame, len)) {
      frame = this->reader_.resync();
      continue;
    }
    ESP_LOGV_HEX(TAG, frame, len, MAX_FRAME_LENGTH, "Frame: %s");
    if (frame[0] == DATA_FRAME_FIRST) {
      this->handle_periodic_data_(frame, len);
    } else {
      this->handle_ack_(frame, len);
    }
    frame = this->reader_.next();
  }
}

void LD2460Component::handle_ack_(const uint8_t *data, uint8_t len) {
  if (len != ACK_FRAME_LENGTH) {
    ESP_LOGW(TAG, "Dropping malformed command frame");
    return;
  }
  if (data[4] != CMD_QUERY_INSTALLATION_MODE) {
    ESP_LOGD(TAG, "Ignoring reply to command 0x%02X", data[4]);
    return;
  }
  const uint8_t mode = data[7];
  if (mode != static_cast<uint8_t>(InstallationMode::INSTALLATION_MODE_SIDE) &&
      mode != static_cast<uint8_t>(InstallationMode::INSTALLATION_MODE_TOP)) {
    ESP_LOGW(TAG, "Unexpected installation mode %u", mode);
    return;
  }
  this->installation_mode_ = static_cast<InstallationMode>(mode);
  this->query_attempts_ = 0;
  ESP_LOGD(TAG, "Installation mode: %s",
           mode == static_cast<uint8_t>(InstallationMode::INSTALLATION_MODE_TOP) ? LOG_STR_LITERAL("top")
                                                                                 : LOG_STR_LITERAL("side"));
#ifdef USE_SENSOR
  // Angles depend on the mode, so republish every target on the next frame
  for (auto &t : this->target_sensors_) {
    t.last_x = INT16_MIN;
  }
#endif
}

}  // namespace esphome::ld2460
