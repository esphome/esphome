#include "qwiic_pir.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::qwiic_pir {

ESPHOME_LOG_TAG(TAG, "qwiic_pir");

static constexpr uint8_t REG_CHIP_ID = 0x00;
static constexpr uint8_t REG_EVENT_STATUS = 0x03;
static constexpr uint8_t REG_DEBOUNCE_TIME = 0x05;  // milliseconds, 16 bits

static constexpr uint8_t DEVICE_ID = 0x72;

// REG_EVENT_STATUS bits
static constexpr uint8_t EVENT_RAW_READING = 0x01;      // raw state of the PIR sensor
static constexpr uint8_t EVENT_AVAILABLE = 0x02;        // a debounced object was detected or removed
static constexpr uint8_t EVENT_OBJECT_REMOVED = 0x04;   // a debounced object is no longer detected
static constexpr uint8_t EVENT_OBJECT_DETECTED = 0x08;  // a debounced object has been detected

// Indexed by DebounceMode
PROGMEM_STRING_TABLE(DebounceModeStrings, "RAW", "NATIVE", "HYBRID");

void QwiicPIRComponent::setup() {
  if (!this->check_chip_id(this, REG_CHIP_ID, DEVICE_ID))
    return;

  if (!this->setup_check(this->write_byte_16(REG_DEBOUNCE_TIME, this->debounce_time_),
                         LOG_STR("Failed to configure debounce time")))
    return;

  if (this->debounce_mode_ == DebounceMode::DEBOUNCE_MODE_NATIVE) {
    // NATIVE only publishes on motion events, so start from the raw state
    uint8_t event;
    if (!this->setup_check(this->read_byte(REG_EVENT_STATUS, &event), LOG_STR("Failed to read initial state")))
      return;
    this->publish_state((event & EVENT_RAW_READING) != 0);
  }
}

void QwiicPIRComponent::loop() {
  uint8_t event;
  if (!this->status_comm_check(this->read_byte(REG_EVENT_STATUS, &event)))
    return;

  switch (this->debounce_mode_) {
    case DebounceMode::DEBOUNCE_MODE_HYBRID:
      // The device debounces for 1 ms and ESPHome filters do the rest; any event means motion since the last loop
      this->publish_state((event & (EVENT_RAW_READING | EVENT_AVAILABLE)) != 0);
      break;
    case DebounceMode::DEBOUNCE_MODE_NATIVE:
      // The device's own debounce, as in SparkFun's Example2_PrintPIRStatus; it often misses objects being removed
      if (event & EVENT_AVAILABLE) {
        if (event & EVENT_OBJECT_DETECTED)
          this->publish_state(true);
        if (event & EVENT_OBJECT_REMOVED)
          this->publish_state(false);
      }
      break;
    case DebounceMode::DEBOUNCE_MODE_RAW:
      // Can miss a very short detection when the loop is slow
      this->publish_state((event & EVENT_RAW_READING) != 0);
      break;
  }

  // HYBRID and NATIVE clear a handled event; the last bus step of the poll decides the warning
  bool ok = true;
  if (this->debounce_mode_ != DebounceMode::DEBOUNCE_MODE_RAW && (event & EVENT_AVAILABLE))
    ok = this->write_byte(REG_EVENT_STATUS, 0x00);
  this->status_comm_result(ok);
}

void QwiicPIRComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Qwiic PIR:\n"
                "  Debounce Mode: %s",
                LOG_STR_ARG(DebounceModeStrings::get_log_str(static_cast<uint8_t>(this->debounce_mode_), 0)));
  if (this->debounce_mode_ == DebounceMode::DEBOUNCE_MODE_NATIVE) {
    ESP_LOGCONFIG(TAG, "  Debounce Time: %ums", this->debounce_time_);
  }
  LOG_I2C_DEVICE(this);
  LOG_BINARY_SENSOR("  ", "Binary Sensor", this);
}

}  // namespace esphome::qwiic_pir
