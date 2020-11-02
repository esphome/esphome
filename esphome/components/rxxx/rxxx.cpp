#include "rxxx.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rxxx {

static const char* TAG = "rxxx";

void RxxxComponent::update() {
  if (this->waiting_removal_) {
    if (this->finger_->getImage() == FINGERPRINT_NOFINGER) {
      ESP_LOGD(TAG, "Finger removed");
      this->waiting_removal_ = false;
    }
    return;
  }

  if (this->enrollment_image_ > this->enrollment_buffers_) {
    ESP_LOGI(TAG, "Creating model");
    uint8_t result = this->finger_->createModel();
    if (result == FINGERPRINT_OK) {
      ESP_LOGI(TAG, "Storing model");
      result = this->finger_->storeModel(this->enrollment_slot_);
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "Stored model");
        this->get_fingerprint_count_();
      } else {
        switch (result) {
          case FINGERPRINT_PACKETRECIEVEERR:
            ESP_LOGE(TAG, "Communication error");
            break;
          case FINGERPRINT_BADLOCATION:
            ESP_LOGE(TAG, "Invalid slot");
            break;
          case FINGERPRINT_FLASHERR:
            ESP_LOGE(TAG, "Error writing to flash");
            break;
          default:
            ESP_LOGE(TAG, "Unknown error: %d", result);
        }
      }
    } else {
      switch (result) {
        case FINGERPRINT_PACKETRECIEVEERR:
          ESP_LOGE(TAG, "Communication error");
          break;
        case FINGERPRINT_ENROLLMISMATCH:
          ESP_LOGE(TAG, "Scans do not match");
          break;
        default:
          ESP_LOGE(TAG, "Unknown error: %d", result);
      }
    }
    this->finish_enrollment(result);
    return;
  }

  if (this->sensing_pin_ != nullptr && this->sensing_pin_->digital_read() == HIGH) {
    ESP_LOGV(TAG, "No touch sensing");
    return;
  }

  if (this->enrollment_image_ == 0) {
    if (this->sensing_pin_ != nullptr) {
      ESP_LOGD(TAG, "Scan and match");
    } else {
      ESP_LOGV(TAG, "Scan and match");
    }
    this->scan_and_match_();
    return;
  }

  uint8_t result = this->scan_image_(this->enrollment_image_);
  if (result == FINGERPRINT_NOFINGER) {
    return;
  }
  this->waiting_removal_ = true;
  if (result != FINGERPRINT_OK) {
    this->finish_enrollment(result);
    return;
  }
  this->enrollment_scan_callback_.call(this->enrollment_image_, this->enrollment_slot_);
  ++this->enrollment_image_;
}

void RxxxComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Rxxx Fingerprint Sensor...");
  //uint8_t result = this->finger_->checkPassword();
  uint8_t result = FINGERPRINT_PACKETRECIEVEERR;
  //if (result == FINGERPRINT_OK) {
  if (this->finger_->verifyPassword()) {
    ESP_LOGD(TAG, "Password verified");
    if (this->new_password_ != nullptr) {
      ESP_LOGI(TAG, "Setting new password: %d", *this->new_password_);
      result = this->finger_->setPassword(*this->new_password_);
      if (result == FINGERPRINT_OK) {
        ESP_LOGI(TAG, "New password successfully set");
        ESP_LOGI(TAG, "Define the new password in your configuration and reflash now");
        ESP_LOGW(TAG, "!!!Forgetting the password will render your device unusable!!!");
      } else {
        ESP_LOGE(TAG, "Communication error");
        this->mark_failed();
      }
    } else {
      ESP_LOGD(TAG, "Getting parameters");
      result = this->finger_->getParameters();
      if (result != FINGERPRINT_OK) {
        ESP_LOGE(TAG, "Error getting parameters");
        this->mark_failed();
      } else {
        if (this->status_sensor_ != nullptr) {
          this->status_sensor_->publish_state(this->finger_->status_reg);
        }
        if (this->capacity_sensor_ != nullptr) {
          this->capacity_sensor_->publish_state(this->finger_->capacity);
        }
        if (this->security_level_sensor_ != nullptr) {
          this->security_level_sensor_->publish_state(this->finger_->security_level);
        }
        if (this->enrolling_binary_sensor_ != nullptr) {
          this->enrolling_binary_sensor_->publish_state(false);
        }
        this->get_fingerprint_count_();
      }
    }
  } else {
    switch (result) {
      case FINGERPRINT_PACKETRECIEVEERR:
        ESP_LOGE(TAG, "Communication error");
        break;
      case FINGERPRINT_PASSFAIL:
        ESP_LOGE(TAG, "Wrong password");
        break;
      default:
        ESP_LOGE(TAG, "Unknown error: %d", result);
    }
    this->mark_failed();
  }
}

void RxxxComponent::enroll_fingerprint(uint16_t finger_id, uint8_t num_buffers) {
  ESP_LOGI(TAG, "Starting enrollment in slot %d", finger_id);
  if (this->enrolling_binary_sensor_ != nullptr) {
    this->enrolling_binary_sensor_->publish_state(true);
  }
  this->enrollment_slot_ = finger_id;
  this->enrollment_buffers_ = num_buffers;
  this->enrollment_image_ = 1;
}

void RxxxComponent::finish_enrollment(uint8_t result) {
  if (result == FINGERPRINT_OK) {
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

void RxxxComponent::scan_and_match_() {
  uint8_t result = this->scan_image_(1);
  if (result == FINGERPRINT_NOFINGER) {
    return;
  }
  this->waiting_removal_ = true;
  if (result == FINGERPRINT_OK) {
    result = this->finger_->fingerSearch();
    ESP_LOGD(TAG, "Finger searched");
    if (result == FINGERPRINT_OK) {
      if (this->last_finger_id_sensor_ != nullptr) { 
        this->last_finger_id_sensor_->publish_state(this->finger_->fingerID);
      }
      if (this->last_confidence_sensor_ != nullptr) { 
        this->last_confidence_sensor_->publish_state(this->finger_->confidence);
      }
      this->finger_scan_matched_callback_.call(this->finger_->fingerID, this->finger_->confidence);
    } else {
      switch (result) {
        case FINGERPRINT_PACKETRECIEVEERR:
          ESP_LOGE(TAG, "Communication error");
          break;
        case FINGERPRINT_NOTFOUND:
          ESP_LOGD(TAG, "Fingerprint not matched to any saved slots");
          this->finger_scan_unmatched_callback_.call();
          break;
        default:
          ESP_LOGE(TAG, "Unknown error: %d", result);
      }
    }
  }
}

uint8_t RxxxComponent::scan_image_(uint8_t buffer) {
  if (this->sensing_pin_ != nullptr) {
    ESP_LOGD(TAG, "Getting image %d", buffer);
  } else {
    ESP_LOGV(TAG, "Getting image %d", buffer);
  }
  uint8_t p = this->finger_->getImage();
  if (p != FINGERPRINT_OK) {
    switch (p) {
      case FINGERPRINT_PACKETRECIEVEERR:
        ESP_LOGE(TAG, "Communication error");
        return p;
      case FINGERPRINT_NOFINGER:
        if (this->sensing_pin_ != nullptr) {
          ESP_LOGD(TAG, "No finger");
        } else {
          ESP_LOGV(TAG, "No finger");
        }
        return p;
      case FINGERPRINT_IMAGEFAIL:
        ESP_LOGE(TAG, "Imaging error");
        return p;
      default:
        ESP_LOGE(TAG, "Unknown error: %d", p);
        return p;
    }
  }

  ESP_LOGD(TAG, "Processing image %d", buffer);
  p = this->finger_->image2Tz(buffer);
  switch (p) {
    case FINGERPRINT_OK:
      ESP_LOGI(TAG, "Processed image %d", buffer);
      return p;
    case FINGERPRINT_IMAGEMESS:
      ESP_LOGE(TAG, "Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      ESP_LOGE(TAG, "Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
    case FINGERPRINT_INVALIDIMAGE:
      ESP_LOGE(TAG, "Could not find fingerprint features");
      return p;
    default:
      ESP_LOGE(TAG, "Unknown error: %d", p);
      return p;
  }
}

void RxxxComponent::get_fingerprint_count_() {
  ESP_LOGD(TAG, "Getting fingerprint count");
  uint8_t result = this->finger_->getTemplateCount();
  switch (result) {
    case FINGERPRINT_OK:
      ESP_LOGD(TAG, "Got fingerprint count");
      if (this->fingerprint_count_sensor_ != nullptr) { 
        this->fingerprint_count_sensor_->publish_state(this->finger_->templateCount);
      }
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      ESP_LOGE(TAG, "Communication error");
      break;
    default:
      ESP_LOGE(TAG, "Unknown error: %d", result);
  }
}

void RxxxComponent::delete_fingerprint(uint16_t finger_id) {
  ESP_LOGI(TAG, "Deleting fingerprint in slot %d", finger_id);
  uint8_t result = this->finger_->deleteModel(finger_id);
  switch (result) {
    case FINGERPRINT_OK:
      ESP_LOGI(TAG, "Deleted fingerprint");
      this->get_fingerprint_count_();
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      ESP_LOGE(TAG, "Communication error");
      break;
    case FINGERPRINT_DELETEFAIL:
      ESP_LOGE(TAG, "Failed to delete fingerprint");
      break;
    default:
      ESP_LOGE(TAG, "Unknown error: %d", result);
  }
}

void RxxxComponent::delete_all_fingerprints() {
  ESP_LOGI(TAG, "Deleting all stored fingerprints");
  uint8_t result = this->finger_->emptyDatabase();
  switch (result) {
    case FINGERPRINT_OK:
      ESP_LOGI(TAG, "Deleted all fingerprints");
      this->get_fingerprint_count_();
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      ESP_LOGE(TAG, "Communication error");
      break;
    case FINGERPRINT_DBCLEARFAIL:
      ESP_LOGE(TAG, "Failed to clear fingerprint library");
      break;
    default:
      ESP_LOGE(TAG, "Unknown error: %d", result);
  }
}

void RxxxComponent::led_control(bool state) {
  ESP_LOGD(TAG, "Setting LED");
  uint8_t result = this->finger_->LEDcontrol(state);
  switch (result) {
    case FINGERPRINT_OK:
      ESP_LOGD(TAG, "LED set");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      ESP_LOGE(TAG, "Communication error");
      break;
    default:
      ESP_LOGE(TAG, "Unknown error: %d", result);
      ESP_LOGE(TAG, "Try aura_led_control instead");
  }
}

void RxxxComponent::aura_led_control(uint8_t state, uint8_t speed, uint8_t color, uint8_t count) {
  const uint32_t now = millis();
  const uint32_t elapsed = now - this->last_aura_led_control_;
  if (elapsed < this->last_aura_led_duration_) {
      delay(this->last_aura_led_duration_ - elapsed);
  }
  ESP_LOGD(TAG, "Setting Aura LED");
  uint8_t result = this->finger_->LEDcontrol(state, speed, color, count);
  switch (result) {
    case FINGERPRINT_OK:
      ESP_LOGD(TAG, "Aura LED set");
      this->last_aura_led_control_ = millis();
      this->last_aura_led_duration_ = 10 * speed * count;
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      ESP_LOGE(TAG, "Communication error");
      break;
    default:
      ESP_LOGE(TAG, "Unknown error: %d", result);
      ESP_LOGE(TAG, "Try led_control instead");
  }
}

void RxxxComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "RXXX_FINGERPRINT_READER:");
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Fingerprint Count", this->fingerprint_count_sensor_);
  LOG_SENSOR("  ", "Status", this->status_sensor_);
  LOG_SENSOR("  ", "Capacity", this->capacity_sensor_);
  LOG_SENSOR("  ", "Security Level", this->security_level_sensor_);
  LOG_SENSOR("  ", "Last Finger ID", this->last_finger_id_sensor_);
  LOG_SENSOR("  ", "Last Confidence", this->last_confidence_sensor_);
}

}  // namespace rxxx
}  // namespace esphome