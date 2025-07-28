#include "qwiic_pir.h"
#include "esphome/core/log.h"

namespace esphome {
namespace qwiic_pir {

static const char *const TAG = "qwiic_pir";

void QwiicPIRComponent::setup() {
  // Verify I2C communcation by reading and verifying the chip ID
  uint8_t chip_id;
  if (!this->read_byte(QWIIC_PIR_CHIP_ID, &chip_id)) {
    ESP_LOGE(TAG, "Failed to read chip ID");
    this->error_code_ = ERROR_COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (chip_id != QWIIC_PIR_DEVICE_ID) {
    ESP_LOGE(TAG, "Unknown chip ID");
    this->error_code_ = ERROR_WRONG_CHIP_ID;
    this->mark_failed();
    return;
  }

  if (!this->write_byte_16(QWIIC_PIR_DEBOUNCE_TIME, this->debounce_time_)) {
    ESP_LOGE(TAG, "Failed to configure debounce time");
    this->error_code_ = ERROR_COMMUNICATION_FAILED;
    this->mark_failed();
    return;
  }

  if (this->debounce_mode_ == NATIVE_DEBOUNCE_MODE) {
    // Publish the starting raw state of the PIR sensor
    // If NATIVE mode, the binary_sensor state would be unknown until a motion event
    if (!this->read_byte(QWIIC_PIR_EVENT_STATUS, &this->event_register_.reg)) {
      ESP_LOGE(TAG, "Failed to read initial state");
      this->error_code_ = ERROR_COMMUNICATION_FAILED;
      this->mark_failed();
      return;
    }

    this->publish_state(this->event_register_.raw_reading);
  }
}

void QwiicPIRComponent::loop() {
  // Read Event Register
  if (!this->read_byte(QWIIC_PIR_EVENT_STATUS, &this->event_register_.reg)) {
    ESP_LOGW(TAG, ESP_LOG_MSG_COMM_FAIL);
    return;
  }

  if (this->debounce_mode_ == HYBRID_DEBOUNCE_MODE) {
    // Use a combination of the raw sensor reading and the device's event detection to determine state
    //  - The device is hardcoded to use a debounce time of 1 ms in this mode
    //  - Any event, even if it is object_removed, implies motion was active since the last loop, so publish true
    //  - Use ESPHome's built-in filters for debouncing
    this->publish_state(this->event_register_.raw_reading || this->event_register_.event_available);

    if (this->event_register_.event_available) {
      this->clear_events_();
    }
  } else if (this->debounce_mode_ == NATIVE_DEBOUNCE_MODE) {
    // Uses the device's firmware to debounce the signal
    //  - Follows the logic of SparkFun's example implementation:
    //    https://github.com/sparkfun/SparkFun_Qwiic_PIR_Arduino_Library/blob/master/examples/Example2_PrintPIRStatus/Example2_PrintPIRStatus.ino
    //    (accessed July 2023)
    //  - Is unreliable at detecting an object being removed, especially at debounce rates even slightly large
    if (this->event_register_.event_available) {
      // If an object is detected, publish true
      if (this->event_register_.object_detected)
        this->publish_state(true);

      // If an object has been removed, publish false
      if (this->event_register_.object_removed)
        this->publish_state(false);

      this->clear_events_();
    }
  } else if (this->debounce_mode_ == RAW_DEBOUNCE_MODE) {
    // Publishes the raw PIR sensor reading with no further logic
    //  - May miss a very short motion detection if the ESP's loop time is slow
    this->publish_state(this->event_register_.raw_reading);
  }
}

void QwiicPIRComponent::dump_config() {
  static const char *const RAW = "RAW";
  static const char *const NATIVE = "NATIVE";
  static const char *const HYBRID = "HYBRID";

  const char *debounce_mode_str = RAW;
  if (this->debounce_mode_ == NATIVE_DEBOUNCE_MODE) {
    debounce_mode_str = NATIVE;
  } else if (this->debounce_mode_ == HYBRID_DEBOUNCE_MODE) {
    debounce_mode_str = HYBRID;
  }

  ESP_LOGCONFIG(TAG,
                "Qwiic PIR:\n"
                "  Debounce Mode: %s",
                debounce_mode_str);
  if (this->debounce_mode_ == NATIVE_DEBOUNCE_MODE) {
    ESP_LOGCONFIG(TAG, "  Debounce Time: %ums", this->debounce_time_);
  }

  switch (this->error_code_) {
    case NONE:
      break;
    case ERROR_COMMUNICATION_FAILED:
      ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
      break;
    case ERROR_WRONG_CHIP_ID:
      ESP_LOGE(TAG, "Unknown chip ID");
      break;
    default:
      ESP_LOGE(TAG, "Error %d", (int) this->error_code_);
      break;
  }

  LOG_I2C_DEVICE(this);
  LOG_BINARY_SENSOR("  ", "Binary Sensor", this);
}

void QwiicPIRComponent::clear_events_() {
  // Clear event status register
  if (!this->write_byte(QWIIC_PIR_EVENT_STATUS, 0x00))
    ESP_LOGW(TAG, "Failed to clear events");
}

}  // namespace qwiic_pir
}  // namespace esphome
