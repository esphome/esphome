#include "ld2451.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::ld2451 {

ESPHOME_LOG_TAG(TAG, "ld2451");

// Angles are sent offset by 0x80, so 0x80 is straight ahead
static constexpr uint8_t ANGLE_OFFSET = 0x80;
// Protocol V1.03 documents 0x00 as approaching, but radars watching traffic send 0x01, matching the alarm byte
static constexpr uint8_t DIRECTION_APPROACHING = 0x01;

void warn_too_many_targets(size_t len) {
  ESP_LOGW(TAG, "Skipped a data frame of %u bytes; more than %u targets are not supported", static_cast<unsigned>(len),
           MAX_TARGETS);
}

void LD2451Component::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2451:");
#ifdef USE_SENSOR
  LOG_SENSOR_WITH_DEDUP("  ", "TargetCount", this->target_count_sensor_);
  for (const auto &t : this->target_sensors_) {
    LOG_SENSOR_WITH_DEDUP("  ", "TargetAngle", t.angle);
    LOG_SENSOR_WITH_DEDUP("  ", "TargetDistance", t.distance);
    LOG_SENSOR_WITH_DEDUP("  ", "TargetSpeed", t.speed);
    LOG_SENSOR_WITH_DEDUP("  ", "TargetSnr", t.snr);
  }
#endif
}

void LD2451Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  auto handler = [this](const uint8_t *frame, size_t len) { return this->handle_frame_(frame, len); };
  this->read_frames<MAX_FRAME_LENGTH>(this->reader_, handler, now, this->last_rx_);
}

// Handles the frame when its footer and structure are in place; returns false otherwise
bool LD2451Component::handle_frame_(const uint8_t *frame, size_t len) {
  if (!ld24xx::has_footer(frame, len)) {
    ESP_LOGW(TAG, "Invalid footer");
    ESP_LOGV_HEX(TAG, frame, len, MAX_FRAME_LENGTH, "Rejected: %s");
    return false;
  }
  ESP_LOGV_HEX(TAG, frame, len, MAX_FRAME_LENGTH, "Frame: %s");
  if (frame[0] != DataFrame::FIRST) {
    return true;  // A reply to a configuration command, none of which are sent yet
  }
  return this->handle_data_(&frame[PAYLOAD_OFFSET], len - PAYLOAD_OFFSET - ld24xx::HEADER_FOOTER_SIZE);
}

// Payload: target count, alarm, then TARGET_SIZE bytes per target
bool LD2451Component::handle_data_(const uint8_t *payload, size_t len) {
  // An idle radar may send a frame with no payload
  const uint8_t num_targets = len == 0 ? 0 : payload[0];
  if (len != 0 && len != 2 + num_targets * TARGET_SIZE) {
    ESP_LOGW(TAG, "Payload of %u bytes does not match a target count of %u", static_cast<unsigned>(len), num_targets);
    return false;
  }
  // MAX_FRAME_LENGTH bounds len, and so the count
#ifdef USE_SENSOR
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    TargetSensors &t = this->target_sensors_[i];
    if (i >= num_targets) {
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.angle);
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.distance);
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.speed);
      SAFE_PUBLISH_SENSOR_UNKNOWN(t.snr);
      continue;
    }
    const uint8_t *p = &payload[2 + i * TARGET_SIZE];
    const auto angle = static_cast<int8_t>(p[0] - ANGLE_OFFSET);
    const int16_t speed = p[2] == DIRECTION_APPROACHING ? -p[3] : p[3];
    ESP_LOGV(TAG, "Target %u: angle %d, distance %u m, speed %d km/h, SNR %u", i + 1, angle, p[1], speed, p[4]);
    SAFE_PUBLISH_SENSOR(t.angle, angle);
    SAFE_PUBLISH_SENSOR(t.distance, p[1]);
    SAFE_PUBLISH_SENSOR(t.speed, speed);
    SAFE_PUBLISH_SENSOR(t.snr, p[4]);
  }
  SAFE_PUBLISH_SENSOR(this->target_count_sensor_, num_targets);
#endif
  return true;
}

}  // namespace esphome::ld2451
