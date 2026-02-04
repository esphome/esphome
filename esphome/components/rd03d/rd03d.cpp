#include "rd03d.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome::rd03d {

static const char *const TAG = "rd03d";

// Delay before sending configuration commands to allow radar to initialize
static constexpr uint32_t SETUP_TIMEOUT_MS = 100;

// Data frame format (radar -> host)
static constexpr uint8_t FRAME_HEADER[] = {0xAA, 0xFF, 0x03, 0x00};
static constexpr uint8_t FRAME_FOOTER[] = {0x55, 0xCC};

// Command frame format (host -> radar)
static constexpr uint8_t CMD_FRAME_HEADER[] = {0xFD, 0xFC, 0xFB, 0xFA};
static constexpr uint8_t CMD_FRAME_FOOTER[] = {0x04, 0x03, 0x02, 0x01};

// RD-03D tracking mode commands
static constexpr uint16_t CMD_SINGLE_TARGET = 0x0080;
static constexpr uint16_t CMD_MULTI_TARGET = 0x0090;

// Speed sentinel values (cm/s) - radar outputs these when no valid Doppler measurement
// FMCW radars detect motion via Doppler shift; targets with these speeds are likely noise
static constexpr int16_t SPEED_SENTINEL_248 = 248;
static constexpr int16_t SPEED_SENTINEL_256 = 256;

// Decode coordinate/speed value from RD-03D format
// Per datasheet: MSB=1 means positive, MSB=0 means negative
static constexpr int16_t decode_value(uint8_t low_byte, uint8_t high_byte) {
  int16_t value = ((high_byte & 0x7F) << 8) | low_byte;
  if ((high_byte & 0x80) == 0) {
    value = -value;
  }
  return value;
}

// Check if speed value indicates a valid Doppler measurement
// Zero, ±248, or ±256 cm/s are sentinel values from the radar firmware
static constexpr bool is_speed_valid(int16_t speed) {
  int16_t abs_speed = speed < 0 ? -speed : speed;
  return speed != 0 && abs_speed != SPEED_SENTINEL_248 && abs_speed != SPEED_SENTINEL_256;
}

void RD03DComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RD-03D...");
  this->set_timeout(SETUP_TIMEOUT_MS, [this]() { this->apply_config_(); });
}

void RD03DComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RD-03D:");
  if (this->tracking_mode_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Tracking Mode: %s",
                  *this->tracking_mode_ == TrackingMode::SINGLE_TARGET ? "single" : "multi");
  }
  if (this->throttle_ > 0) {
    ESP_LOGCONFIG(TAG, "  Throttle: %ums", this->throttle_);
  }
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Target Count", this->target_count_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Target", this->target_binary_sensor_);
#endif
  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    ESP_LOGCONFIG(TAG, "  Target %d:", i + 1);
#ifdef USE_SENSOR
    LOG_SENSOR("    ", "X", this->targets_[i].x);
    LOG_SENSOR("    ", "Y", this->targets_[i].y);
    LOG_SENSOR("    ", "Speed", this->targets_[i].speed);
    LOG_SENSOR("    ", "Distance", this->targets_[i].distance);
    LOG_SENSOR("    ", "Resolution", this->targets_[i].resolution);
    LOG_SENSOR("    ", "Angle", this->targets_[i].angle);
#endif
#ifdef USE_BINARY_SENSOR
    LOG_BINARY_SENSOR("    ", "Presence", this->target_presence_[i]);
#endif
  }
}

void RD03DComponent::loop() {
  while (this->available()) {
    uint8_t byte = this->read();
    ESP_LOGVV(TAG, "Received byte: 0x%02X, buffer_pos: %d", byte, this->buffer_pos_);

    // Check if we're looking for frame header
    if (this->buffer_pos_ < FRAME_HEADER_SIZE) {
      if (byte == FRAME_HEADER[this->buffer_pos_]) {
        this->buffer_[this->buffer_pos_++] = byte;
      } else if (byte == FRAME_HEADER[0]) {
        // Start over if we see a potential new header
        this->buffer_[0] = byte;
        this->buffer_pos_ = 1;
      } else {
        this->buffer_pos_ = 0;
      }
      continue;
    }

    // Accumulate data bytes
    this->buffer_[this->buffer_pos_++] = byte;

    // Check if we have a complete frame
    if (this->buffer_pos_ == FRAME_SIZE) {
      // Validate footer
      if (this->buffer_[FRAME_SIZE - 2] == FRAME_FOOTER[0] && this->buffer_[FRAME_SIZE - 1] == FRAME_FOOTER[1]) {
        this->process_frame_();
      } else {
        ESP_LOGW(TAG, "Invalid frame footer: 0x%02X 0x%02X (expected 0x55 0xCC)", this->buffer_[FRAME_SIZE - 2],
                 this->buffer_[FRAME_SIZE - 1]);
      }
      this->buffer_pos_ = 0;
    }
  }
}

void RD03DComponent::process_frame_() {
  // Apply throttle if configured
  if (this->throttle_ > 0) {
    uint32_t now = millis();
    if (now - this->last_publish_time_ < this->throttle_) {
      return;
    }
    this->last_publish_time_ = now;
  }

  uint8_t target_count = 0;

  for (uint8_t i = 0; i < MAX_TARGETS; i++) {
    // Calculate offset for this target's data
    // Header is 4 bytes, each target is 8 bytes
    uint8_t offset = FRAME_HEADER_SIZE + (i * TARGET_DATA_SIZE);

    // Extract raw bytes for this target
    // Note: Despite datasheet Table 5-2 showing order as X, Y, Speed, Resolution,
    // actual radar output has Resolution before Speed (verified empirically -
    // stationary targets were showing non-zero speed with original field order)
    uint8_t x_low = this->buffer_[offset + 0];
    uint8_t x_high = this->buffer_[offset + 1];
    uint8_t y_low = this->buffer_[offset + 2];
    uint8_t y_high = this->buffer_[offset + 3];
    uint8_t res_low = this->buffer_[offset + 4];
    uint8_t res_high = this->buffer_[offset + 5];
    uint8_t speed_low = this->buffer_[offset + 6];
    uint8_t speed_high = this->buffer_[offset + 7];

    // Decode values per RD-03D format
    int16_t x = decode_value(x_low, x_high);
    int16_t y = decode_value(y_low, y_high);
    int16_t speed = decode_value(speed_low, speed_high);
    uint16_t resolution = (res_high << 8) | res_low;

    // Check if target is present
    // Requires non-zero coordinates AND valid speed (not a sentinel value)
    // FMCW radars detect motion via Doppler; sentinel speed indicates no real target
    bool has_position = (x != 0 || y != 0);
    bool has_valid_speed = is_speed_valid(speed);
    bool target_present = has_position && has_valid_speed;
    if (target_present) {
      target_count++;
    }

#ifdef USE_SENSOR
    this->publish_target_(i, x, y, speed, resolution);
#endif

#ifdef USE_BINARY_SENSOR
    if (this->target_presence_[i] != nullptr) {
      this->target_presence_[i]->publish_state(target_present);
    }
#endif
  }

#ifdef USE_SENSOR
  if (this->target_count_sensor_ != nullptr) {
    this->target_count_sensor_->publish_state(target_count);
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (this->target_binary_sensor_ != nullptr) {
    this->target_binary_sensor_->publish_state(target_count > 0);
  }
#endif
}

#ifdef USE_SENSOR
void RD03DComponent::publish_target_(uint8_t target_num, int16_t x, int16_t y, int16_t speed, uint16_t resolution) {
  TargetSensor &target = this->targets_[target_num];
  bool valid = is_speed_valid(speed);

  // Publish X coordinate (mm) - NaN if target invalid
  if (target.x != nullptr) {
    target.x->publish_state(valid ? static_cast<float>(x) : NAN);
  }

  // Publish Y coordinate (mm) - NaN if target invalid
  if (target.y != nullptr) {
    target.y->publish_state(valid ? static_cast<float>(y) : NAN);
  }

  // Publish speed (convert from cm/s to mm/s) - NaN if target invalid
  if (target.speed != nullptr) {
    target.speed->publish_state(valid ? static_cast<float>(speed) * 10.0f : NAN);
  }

  // Publish resolution (mm)
  if (target.resolution != nullptr) {
    target.resolution->publish_state(resolution);
  }

  // Calculate and publish distance (mm) - NaN if target invalid
  if (target.distance != nullptr) {
    if (valid) {
      target.distance->publish_state(std::hypot(static_cast<float>(x), static_cast<float>(y)));
    } else {
      target.distance->publish_state(NAN);
    }
  }

  // Calculate and publish angle (degrees) - NaN if target invalid
  // Angle is measured from the Y axis (radar forward direction)
  if (target.angle != nullptr) {
    if (valid) {
      float angle = std::atan2(static_cast<float>(x), static_cast<float>(y)) * 180.0f / M_PI;
      target.angle->publish_state(angle);
    } else {
      target.angle->publish_state(NAN);
    }
  }
}
#endif

void RD03DComponent::send_command_(uint16_t command, const uint8_t *data, uint8_t data_len) {
  // Send header
  this->write_array(CMD_FRAME_HEADER, sizeof(CMD_FRAME_HEADER));

  // Send length (command word + data)
  uint16_t len = 2 + data_len;
  this->write_byte(len & 0xFF);
  this->write_byte((len >> 8) & 0xFF);

  // Send command word (little-endian)
  this->write_byte(command & 0xFF);
  this->write_byte((command >> 8) & 0xFF);

  // Send data if any
  if (data != nullptr && data_len > 0) {
    this->write_array(data, data_len);
  }

  // Send footer
  this->write_array(CMD_FRAME_FOOTER, sizeof(CMD_FRAME_FOOTER));

  ESP_LOGD(TAG, "Sent command 0x%04X with %d bytes of data", command, data_len);
}

void RD03DComponent::apply_config_() {
  if (this->tracking_mode_.has_value()) {
    uint16_t mode_cmd = (*this->tracking_mode_ == TrackingMode::SINGLE_TARGET) ? CMD_SINGLE_TARGET : CMD_MULTI_TARGET;
    this->send_command_(mode_cmd);
  }
}

}  // namespace esphome::rd03d
