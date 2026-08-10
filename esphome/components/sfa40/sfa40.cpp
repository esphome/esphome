#include "sfa40.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::sfa40 {

static const char *const TAG = "sfa40";

// SFA40 Datasheet: https://sensirion.com/media/documents/5B06EDD9/69F84BD8/Sensirion_Datasheet_SFA40.pdf

static const uint16_t SFA40_CMD_START_MEASUREMENT = 0x00AC;
static const uint16_t SFA40_CMD_STOP_MEASUREMENT = 0x50D2;
static const uint16_t SFA40_CMD_READ_MEASURE_PROD = 0xC0EB;
// B4 (engineering-sample) command codes. Commands from here: https://github.com/DFRobot/DFRobot_SFA40
static const uint16_t SFA40_CMD_READ_MEASURE_B4 = 0xE06D;
static const uint16_t SFA40_CMD_READ_ID_PROD = 0x02CE;
static const uint16_t SFA40_CMD_READ_ID_B4 = 0x0559;
static const uint8_t STATUS_NOT_READY = 0x01;
static const uint8_t STATUS_OUT_OF_SPEC = 0x02;

static uint64_t raw_to_serial(const uint16_t *raw, size_t words) {
  uint64_t serial = 0;
  for (size_t i = 0; i < words; i++) {
    serial = (serial << 16) | raw[i];
  }
  return serial;
}

static void raw_to_marking(const uint16_t *raw, size_t words, char *out, size_t out_len) {
  if (out_len < words * 2 + 1) {
    return;
  }
  for (size_t i = 0; i < words; i++) {
    out[i * 2] = static_cast<char>(raw[i] >> 8);
    out[i * 2 + 1] = static_cast<char>(raw[i] & 0xFF);
  }
  out[words * 2] = '\0';
}

void SFA40Component::setup() {
  this->write_command(SFA40_CMD_STOP_MEASUREMENT);
  this->set_timeout(25, [this]() {
    if (!this->detect_protocol_()) {
      ESP_LOGE(TAG, "Failed to detect SFA40 protocol");
      this->error_code_ = PROTOCOL_DETECTION_FAILED;
      this->mark_failed();
      return;
    }
    if (!this->write_command(SFA40_CMD_START_MEASUREMENT)) {
      ESP_LOGE(TAG, "Failed to start measurements");
      this->error_code_ = MEASUREMENT_INIT_FAILED;
      this->mark_failed();
      return;
    }
    this->initialized_ = true;
    ESP_LOGD(TAG, "Measurement started");
  });
}

bool SFA40Component::detect_protocol_() {
  uint16_t raw[5] = {};
  if (this->get_register(SFA40_CMD_READ_ID_PROD, raw, 3, 5)) {
    this->protocol_version_ = ProtocolVersion::PRODUCTION;
    this->serial_number_ = raw_to_serial(raw, 3);
    ESP_LOGD(TAG, "Detected production SFA40, serial number: %012" PRIX64, this->serial_number_);
    return true;
  }
  if (this->get_register(SFA40_CMD_READ_ID_B4, raw, 5, 5)) {
    this->protocol_version_ = ProtocolVersion::PROTOTYPE;
    raw_to_marking(raw, 5, this->device_marking_, sizeof(this->device_marking_));
    ESP_LOGD(TAG, "Detected engineering-sample SFA40, marking: '%s'", this->device_marking_);
    return true;
  }
  return false;
}

void SFA40Component::dump_config() {
  ESP_LOGCONFIG(TAG, "sfa40:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    switch (this->error_code_) {
      case PROTOCOL_DETECTION_FAILED:
        ESP_LOGW(TAG, "Protocol detection failed!");
        break;
      case MEASUREMENT_INIT_FAILED:
        ESP_LOGW(TAG, "Measurement initialization failed!");
        break;
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
  LOG_UPDATE_INTERVAL(this);
  switch (this->protocol_version_) {
    case ProtocolVersion::PRODUCTION:
      ESP_LOGCONFIG(TAG, "  Protocol: production\n  Serial Number: %012" PRIX64, this->serial_number_);
      break;
    case ProtocolVersion::PROTOTYPE:
      ESP_LOGCONFIG(TAG, "  Protocol: prototype (B4)\n  Marking: '%s'", this->device_marking_);
      break;
    default:
      ESP_LOGCONFIG(TAG, "  Protocol: (detecting...)");
      break;
  }
  ESP_LOGCONFIG(TAG, "  Wait for ready: %s", YESNO(this->wait_for_ready_));
  LOG_SENSOR("  ", "Formaldehyde", this->formaldehyde_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
}

void SFA40Component::update() {
  if (!this->initialized_ || this->protocol_version_ == ProtocolVersion::UNKNOWN) {
    return;
  }

  const uint16_t read_cmd = (this->protocol_version_ == ProtocolVersion::PRODUCTION) ? SFA40_CMD_READ_MEASURE_PROD
                                                                                     : SFA40_CMD_READ_MEASURE_B4;

  if (!this->write_command(read_cmd)) {
    ESP_LOGW(TAG, "Error reading measurement");
    this->status_set_warning();
    return;
  }

  this->set_timeout(5, [this]() {
    uint16_t raw[4];
    if (!this->read_data(raw, 4)) {
      ESP_LOGW(TAG, "Error reading measurement data");
      this->status_set_warning();
      return;
    }

    const uint8_t status = raw[3] >> 8;
    const bool sensor_not_ready = (status & STATUS_NOT_READY) != 0;
    const bool sensor_out_of_spec = (status & STATUS_OUT_OF_SPEC) != 0;

    if (this->formaldehyde_sensor_ != nullptr) {
      if (sensor_out_of_spec) {
        ESP_LOGW(TAG, "Skipping formaldehyde publish: sensor out of spec (status=0x%02X)", status);
      } else if (this->wait_for_ready_ && sensor_not_ready) {
        ESP_LOGD(TAG, "Skipping formaldehyde publish: sensor warming up");
      } else {
        this->formaldehyde_sensor_->publish_state(static_cast<float>(raw[0]) / 10.0f);
      }
    }

    if (this->humidity_sensor_ != nullptr) {
      this->humidity_sensor_->publish_state(clamp(125.0f * static_cast<float>(raw[1]) / 65535.0f - 6.0f, 0.0f, 100.0f));
    }

    if (this->temperature_sensor_ != nullptr) {
      this->temperature_sensor_->publish_state(175.0f * (static_cast<float>(raw[2]) / 65535.0f) - 45.0f);
    }

    this->status_clear_warning();
  });
}

}  // namespace esphome::sfa40
