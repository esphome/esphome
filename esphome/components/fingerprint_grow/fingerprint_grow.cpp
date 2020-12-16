#include "fingerprint_grow.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fingerprint_grow {

static const char* TAG = "fingerprint_grow";

// Based on Adafruit's library: https://github.com/adafruit/Adafruit-Fingerprint-Sensor-Library

void FingerprintGrowComponent::update() {
  if (this->waiting_removal_) {
    if (this->scan_image_(0) == GrowResponse::NO_FINGER) {
      ESP_LOGD(TAG, "Finger removed");
      this->waiting_removal_ = false;
    }
    return;
  }

  if (this->enrollment_image_ > this->enrollment_buffers_) {
    this->finish_enrollment(this->save_fingerprint_());
    return;
  }

  if (this->sensing_pin_ != nullptr && this->sensing_pin_->digital_read() == HIGH) {
    ESP_LOGV(TAG, "No touch sensing");
    return;
  }

  if (this->enrollment_image_ == 0) {
    this->scan_and_match_();
    return;
  }

  uint8_t result = this->scan_image_(this->enrollment_image_);
  if (result == GrowResponse::NO_FINGER) {
    return;
  }
  this->waiting_removal_ = true;
  if (result != GrowResponse::OK) {
    this->finish_enrollment(result);
    return;
  }
  this->enrollment_scan_callback_.call(this->enrollment_image_, this->enrollment_slot_);
  ++this->enrollment_image_;
}

void FingerprintGrowComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Grow Fingerprint Reader...");
  if (this->check_password_()) {
    if (this->new_password_ != nullptr) {
      if (this->set_password_())
        return;
    } else {
      if (this->get_parameters_())
        return;
    }
  }
  this->mark_failed();
}

void FingerprintGrowComponent::enroll_fingerprint(uint16_t finger_id, uint8_t num_buffers) {
  ESP_LOGI(TAG, "Starting enrollment in slot %d", finger_id);
  if (this->enrolling_binary_sensor_ != nullptr) {
    this->enrolling_binary_sensor_->publish_state(true);
  }
  this->enrollment_slot_ = finger_id;
  this->enrollment_buffers_ = num_buffers;
  this->enrollment_image_ = 1;
}

void FingerprintGrowComponent::finish_enrollment(uint8_t result) {
  if (result == GrowResponse::OK) {
    this->enrollment_done_callback_.call(this->enrollment_slot_);
  } else {
    this->enrollment_failed_callback_.call(this->enrollment_slot_);
  }
  this->enrollment_image_ = 0;
  this->enrollment_slot_ = 0;
  if (this->enrolling_binary_sensor_ != nullptr) {
    this->enrolling_binary_sensor_->publish_state(false);
  }
  ESP_LOGI(TAG, "Finished enrollment");
}

void FingerprintGrowComponent::scan_and_match_() {
  if (this->sensing_pin_ != nullptr) {
    ESP_LOGD(TAG, "Scan and match");
  } else {
    ESP_LOGV(TAG, "Scan and match");
  }
  uint8_t result = this->scan_image_(1);
  if (result == GrowResponse::NO_FINGER) {
    return;
  }
  this->waiting_removal_ = true;
  if (result == GrowResponse::OK) {
    this->write_packet_({
      GrowCommand::SEARCH,
      0x01,
      0x00,
      0x00,
      (uint8_t)(this->capacity_ >> 8),
      (uint8_t)(this->capacity_ & 0xFF)
    });
    if (this->read_packet_()) {
      ESP_LOGD(TAG, "Finger searched");
      if (this->packet_.data[0] == GrowResponse::OK) {
        uint16_t finger_id = ((uint16_t)this->packet_.data[1] << 8) | this->packet_.data[2];
        uint16_t confidence = ((uint16_t)this->packet_.data[3] << 8) | this->packet_.data[4];
        if (this->last_finger_id_sensor_ != nullptr) {
          this->last_finger_id_sensor_->publish_state(finger_id);
        }
        if (this->last_confidence_sensor_ != nullptr) {
          this->last_confidence_sensor_->publish_state(confidence);
        }
        this->finger_scan_matched_callback_.call(finger_id, confidence);
      } else {
        switch (this->packet_.data[0]) {
          case GrowResponse::PACKET_RCV_ERR:
            ESP_LOGE(TAG, "Reader failed to process request");
            break;
          case GrowResponse::NOT_FOUND:
            ESP_LOGD(TAG, "Fingerprint not matched to any saved slots");
            this->finger_scan_unmatched_callback_.call();
            break;
          default:
            ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
            break;
        }
      }
    } else {
      ESP_LOGE(TAG, "No valid response received from reader");
    }
  }
}

uint8_t FingerprintGrowComponent::scan_image_(uint8_t buffer) {
  if (this->sensing_pin_ != nullptr) {
    ESP_LOGD(TAG, "Getting image %d", buffer);
  } else {
    ESP_LOGV(TAG, "Getting image %d", buffer);
  }
  this->write_packet_({GrowCommand::GET_IMAGE});
  if (this->read_packet_()) {
    if (this->packet_.data[0] != GrowResponse::OK) {
      switch (this->packet_.data[0]) {
        case GrowResponse::PACKET_RCV_ERR:
          ESP_LOGE(TAG, "Reader failed to process request");
          break;
        case GrowResponse::NO_FINGER:
          if (this->sensing_pin_ != nullptr) {
            ESP_LOGD(TAG, "No finger");
          } else {
            ESP_LOGV(TAG, "No finger");
          }
          break;
        case GrowResponse::IMAGE_FAIL:
          ESP_LOGE(TAG, "Imaging error");
          break;
        default:
          ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
          break;
      }
      return this->packet_.data[0];
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
    return GrowResponse::TIMEOUT;
  }

  ESP_LOGD(TAG, "Processing image %d", buffer);
  this->write_packet_({GrowCommand::IMAGE_2_TZ, buffer});
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGI(TAG, "Processed image %d", buffer);
        break;
      case GrowResponse::IMAGE_MESS:
        ESP_LOGE(TAG, "Image too messy");
        break;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      case GrowResponse::FEATURE_FAIL:
      case GrowResponse::INVALID_IMAGE:
        ESP_LOGE(TAG, "Could not find fingerprint features");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
    return this->packet_.data[0];
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
    return GrowResponse::TIMEOUT;
  }
}

uint8_t FingerprintGrowComponent::save_fingerprint_() {
  ESP_LOGI(TAG, "Creating model");
  this->write_packet_({GrowCommand::REG_MODEL});
  if (this->read_packet_()) {
    if (this->packet_.data[0] != GrowResponse::OK) {
      switch (this->packet_.data[0]) {
        case GrowResponse::PACKET_RCV_ERR:
          ESP_LOGE(TAG, "Reader failed to process request");
          break;
        case GrowResponse::ENROLL_MISMATCH:
          ESP_LOGE(TAG, "Scans do not match");
          break;
        default:
          ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
          break;
      }
      return this->packet_.data[0];
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
    return GrowResponse::TIMEOUT;
  }

  ESP_LOGI(TAG, "Storing model");
  this->write_packet_({
    GrowCommand::STORE,
    0x01,
    (uint8_t)(this->enrollment_slot_ >> 8),
    (uint8_t)(this->enrollment_slot_ & 0xFF)
  });
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGI(TAG, "Stored model");
        break;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      case GrowResponse::BAD_LOCATION:
        ESP_LOGE(TAG, "Invalid slot");
        break;
      case GrowResponse::FLASH_ERR:
        ESP_LOGE(TAG, "Error writing to flash");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
    return this->packet_.data[0];
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
    return GrowResponse::TIMEOUT;
  }
}

bool FingerprintGrowComponent::check_password_() {
  ESP_LOGD(TAG, "Checking password");
  this->write_packet_({
    GrowCommand::VERIFY_PASSWORD,
    (uint8_t)(this->password_ >> 24),
    (uint8_t)(this->password_ >> 16),
    (uint8_t)(this->password_ >> 8),
    (uint8_t)(this->password_ & 0xFF)
  });
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGD(TAG, "Password verified");
        return true;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      case GrowResponse::PASSWORD_FAIL:
        ESP_LOGE(TAG, "Wrong password");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
  return false;
}

bool FingerprintGrowComponent::set_password_() {
  ESP_LOGI(TAG, "Setting new password: %d", *this->new_password_);
  this->write_packet_({
    GrowCommand::SET_PASSWORD,
    (uint8_t)(this->new_password_ >> 24),
    (uint8_t)(this->new_password_ >> 16),
    (uint8_t)(this->new_password_ >> 8),
    (uint8_t)(this->new_password_ & 0xFF)
  });
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGI(TAG, "New password successfully set");
        ESP_LOGI(TAG, "Define the new password in your configuration and reflash now");
        ESP_LOGW(TAG, "!!!Forgetting the password will render your device unusable!!!");
        return true;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
  return false;
}

bool FingerprintGrowComponent::get_parameters_() {
  ESP_LOGD(TAG, "Getting parameters");
  this->write_packet_({GrowCommand::READ_SYS_PARAM});
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGD(TAG, "Got parameters");
        if (this->status_sensor_ != nullptr) {
          this->status_sensor_->publish_state(((uint16_t)packet.data[1] << 8) | packet.data[2]);
        }
        this->capacity_ = ((uint16_t)packet.data[5] << 8) | packet.data[6];
        if (this->capacity_sensor_ != nullptr) {
          this->capacity_sensor_->publish_state(this->capacity_);
        }
        if (this->security_level_sensor_ != nullptr) {
          this->security_level_sensor_->publish_state(((uint16_t)packet.data[7] << 8) | packet.data[8]);
        }
        if (this->enrolling_binary_sensor_ != nullptr) {
          this->enrolling_binary_sensor_->publish_state(false);
        }
        this->get_fingerprint_count_();
        return true;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
  return false
}

void FingerprintGrowComponent::get_fingerprint_count_() {
  ESP_LOGD(TAG, "Getting fingerprint count");
  this->write_packet_({GrowCommand::TEMPLATE_COUNT});
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGD(TAG, "Got fingerprint count");
        if (this->fingerprint_count_sensor_ != nullptr) {
          this->fingerprint_count_sensor_->publish_state(
            ((uint16_t)this->packet_.data[1] << 8) | this->packet_.data[2]
          );
        }
        break;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
}

void FingerprintGrowComponent::delete_fingerprint(uint16_t finger_id) {
  ESP_LOGI(TAG, "Deleting fingerprint in slot %d", finger_id);
  this->write_packet_({GrowCommand::DELETE, (uint8_t)(finger_id >> 8), (uint8_t)(finger_id & 0xFF), 0x00, 0x01});
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGI(TAG, "Deleted fingerprint");
        this->get_fingerprint_count_();
        break;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      case GrowResponse::DELETE_FAIL:
        ESP_LOGE(TAG, "Reader failed to delete fingerprint");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
}

void FingerprintGrowComponent::delete_all_fingerprints() {
  ESP_LOGI(TAG, "Deleting all stored fingerprints");
  this->write_packet_({GrowCommand::EMPTY});
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGI(TAG, "Deleted all fingerprints");
        this->get_fingerprint_count_();
        break;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      case GrowResponse::DB_CLEAR_FAIL:
        ESP_LOGE(TAG, "Reader failed to clear fingerprint library");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
}

void FingerprintGrowComponent::led_control(bool state) {
  ESP_LOGD(TAG, "Setting LED");
  if (state)
    this->write_packet_({GrowCommand::LED_ON});
  else
    this->write_packet_({GrowCommand::LED_OFF});
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGD(TAG, "LED set");
        break;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        ESP_LOGE(TAG, "Try aura_led_control instead");
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
}

void FingerprintGrowComponent::aura_led_control(uint8_t state, uint8_t speed, uint8_t color, uint8_t count) {
  const uint32_t now = millis();
  const uint32_t elapsed = now - this->last_aura_led_control_;
  if (elapsed < this->last_aura_led_duration_) {
    delay(this->last_aura_led_duration_ - elapsed);
  }
  ESP_LOGD(TAG, "Setting Aura LED");
  this->write_packet_({GrowCommand::AURA_CONFIG, state, speed, color, count});
  if (this->read_packet_()) {
    switch (this->packet_.data[0]) {
      case GrowResponse::OK:
        ESP_LOGD(TAG, "Aura LED set");
        this->last_aura_led_control_ = millis();
        this->last_aura_led_duration_ = 10 * speed * count;
        break;
      case GrowResponse::PACKET_RCV_ERR:
        ESP_LOGE(TAG, "Reader failed to process request");
        break;
      default:
        ESP_LOGE(TAG, "Unknown response received from reader: %d", this->packet_.data[0]);
        ESP_LOGE(TAG, "Try led_control instead");
        break;
    }
  } else {
    ESP_LOGE(TAG, "No valid response received from reader");
  }
}

void FingerprintGrowComponent::write_packet_(const uint8_t data[]) {
  this->write((uint8_t)(START_CODE >> 8));
  this->write((uint8_t)(START_CODE & 0xFF));
  this->write(this->address_[0]);
  this->write(this->address_[1]);
  this->write(this->address_[2]);
  this->write(this->address_[3]);
  this->write(GrowPacketType::COMMAND);

  uint16_t wire_length = sizeof(data) + 2;
  this->write((uint8_t)(wire_length >> 8));
  this->write((uint8_t)(wire_length & 0xFF));

  uint16_t sum = ((wire_length) >> 8) + ((wire_length) & 0xFF) + GrowPacketType::COMMAND;
  for (uint8_t i = 0; i < sizeof(data); i++) {
    this->write(data[i]);
    sum += data[i];
  }

  this->write((uint8_t)(sum >> 8));
  this->write((uint8_t)(sum & 0xFF));
}

bool FingerprintGrowComponent::read_packet_() {
  uint8_t byte;
  uint16_t idx = 0;

  for (uint16_t timer = 0; timer >= 1000; timer++) {
    if (!this->available()) {
      delay(1);
      continue;
    }
    byte = this->read();
    switch (idx) {
    case 0:
      if (byte != (uint8_t)(START_CODE >> 8))
        continue;
      break;
    case 1:
      if (byte != (uint8_t)(START_CODE & 0xFF)) {
        idx = 0;
        continue;
      }
      break;
    case 2:
    case 3:
    case 4:
    case 5:
      if (byte != this->address[idx - 2]) {
        idx = 0;
        continue;
      }
      break;
    case 6:
      if (byte != GrowPacketType::ACK) {
        idx = 0;
        continue;
      }
      break;
    case 7:
      this->packet_.length = (uint16_t)byte << 8;
      break;
    case 8:
      this->packet_.length |= byte;
      break;
    default:
      this->packet_.data[idx - 9] = byte;
      if ((idx - 8) == this->packet_.length) {
        return true;
      }
      break;
    }
    idx++;
  }
  return false;
}

void FingerprintGrowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "GROW_FINGERPRINT_READER:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Fingerprint Count", this->fingerprint_count_sensor_);
  LOG_SENSOR("  ", "Status", this->status_sensor_);
  LOG_SENSOR("  ", "Capacity", this->capacity_sensor_);
  LOG_SENSOR("  ", "Security Level", this->security_level_sensor_);
  LOG_SENSOR("  ", "Last Finger ID", this->last_finger_id_sensor_);
  LOG_SENSOR("  ", "Last Confidence", this->last_confidence_sensor_);
}

}  // namespace fingerprint_grow
}  // namespace esphome
