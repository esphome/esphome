#include "ccs811.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace ccs811 {

static const char *const TAG = "ccs811";

// based on
//  - https://cdn.sparkfun.com/datasheets/BreakoutBoards/CCS811_Programming_Guide.pdf

#define CHECK_TRUE(f, error_code) \
  if (!(f)) { \
    this->mark_failed(); \
    this->error_code_ = (error_code); \
    return; \
  }

#define CHECKED_IO(f) CHECK_TRUE(f, COMMUNICATION_FAILED)

void CCS811Component::setup() {
  // page 9 programming guide - hwid is always 0x81
  uint8_t hw_id;
  CHECKED_IO(this->read_byte(0x20, &hw_id))
  CHECK_TRUE(hw_id == 0x81, INVALID_ID)

  // software reset, page 3 - allowed to fail
  this->write_bytes(0xFF, {0x11, 0xE5, 0x72, 0x8A});
  delay(5);

  // page 10, APP_START
  CHECK_TRUE(!this->status_has_error_(), SENSOR_REPORTED_ERROR)
  CHECK_TRUE(this->status_app_is_valid_(), APP_INVALID)
  CHECK_TRUE(this->write_bytes(0xF4, {}), APP_START_FAILED)
  // App setup, wait for it to load
  delay(1);

  // set MEAS_MODE (page 5)
  uint8_t meas_mode = 0;
  uint32_t interval = this->get_update_interval();
  if (interval >= 60 * 1000)
    meas_mode = 3 << 4;  // sensor takes a reading every 60 seconds
  else if (interval >= 10 * 1000)
    meas_mode = 2 << 4;  // sensor takes a reading every 10 seconds
  else if (interval >= 1 * 1000)
    meas_mode = 1 << 4;  // sensor takes a reading every second
  else
    meas_mode = 4 << 4;  // sensor takes a reading every 250ms

  CHECKED_IO(this->write_byte(0x01, meas_mode))

  if (this->baseline_.has_value()) {
    // baseline available, write to sensor
    this->write_bytes(0x11, decode_value(*this->baseline_));
  }

  auto hardware_version_data = this->read_bytes<1>(0x21);
  auto bootloader_version_data = this->read_bytes<2>(0x23);
  auto application_version_data = this->read_bytes<2>(0x24);

  uint8_t hardware_version = 0;
  uint16_t bootloader_version = 0;
  uint16_t application_version = 0;

  if (hardware_version_data.has_value()) {
    hardware_version = (*hardware_version_data)[0];
  }

  if (bootloader_version_data.has_value()) {
    bootloader_version = encode_uint16((*bootloader_version_data)[0], (*bootloader_version_data)[1]);
  }

  if (application_version_data.has_value()) {
    application_version = encode_uint16((*application_version_data)[0], (*application_version_data)[1]);
  }

  ESP_LOGD(TAG, "hardware_version=0x%x bootloader_version=0x%x application_version=0x%x\n", hardware_version,
           bootloader_version, application_version);
  if (this->version_ != nullptr) {
    char version[20];  // "15.15.15 (0xffff)" is 17 chars, plus NUL, plus wiggle room
    sprintf(version, "%d.%d.%d (0x%02x)", (application_version >> 12 & 15), (application_version >> 8 & 15),
            (application_version >> 4 & 15), application_version);
    ESP_LOGD(TAG, "publishing version state: %s", version);
    this->version_->publish_state(version);
  }
}
void CCS811Component::update() {
  if (!this->status_has_data_()) {
    ESP_LOGD(TAG, "Status indicates no data ready!");
    this->status_set_warning();
    return;
  }

  // page 12 - alg result data
  auto alg_data = this->read_bytes<4>(0x02);
  if (!alg_data.has_value()) {
    ESP_LOGW(TAG, "Reading CCS811 data failed!");
    this->status_set_warning();
    return;
  }
  auto res = *alg_data;
  uint16_t co2 = encode_uint16(res[0], res[1]);
  uint16_t tvoc = encode_uint16(res[2], res[3]);

  // also print baseline
  auto baseline_data = this->read_bytes<2>(0x11);
  uint16_t baseline = 0;
  if (baseline_data.has_value()) {
    baseline = encode_uint16((*baseline_data)[0], (*baseline_data)[1]);
  }

  ESP_LOGD(TAG, "Got co2=%u ppm, tvoc=%u ppb, baseline=0x%04X", co2, tvoc, baseline);

  if (this->co2_ != nullptr)
    this->co2_->publish_state(co2);
  if (this->tvoc_ != nullptr)
    this->tvoc_->publish_state(tvoc);

  this->status_clear_warning();

  this->send_env_data_();
}
void CCS811Component::send_env_data_() {
  if (this->humidity_ == nullptr && this->temperature_ == nullptr)
    return;

  float humidity = NAN;
  if (this->humidity_ != nullptr)
    humidity = this->humidity_->state;
  if (std::isnan(humidity) || humidity < 0 || humidity > 100)
    humidity = 50;
  float temperature = NAN;
  if (this->temperature_ != nullptr)
    temperature = this->temperature_->state;
  if (std::isnan(temperature) || temperature < -25 || temperature > 50)
    temperature = 25;
  // temperature has a 25° offset to allow negative temperatures
  temperature += 25;

  // At page 18 of:
  // https://cdn.sparkfun.com/datasheets/BreakoutBoards/CCS811_Programming_Guide.pdf
  // Reference code:
  // https://github.com/adafruit/Adafruit_CCS811/blob/0990f5c620354d8bc087c4706bec091d8e6e5dfd/Adafruit_CCS811.cpp#L135-L142
  uint16_t hum_conv = static_cast<uint16_t>(lroundf(humidity * 512.0f + 0.5f));
  uint16_t temp_conv = static_cast<uint16_t>(lroundf(temperature * 512.0f + 0.5f));
  this->write_bytes(0x05, {(uint8_t)((hum_conv >> 8) & 0xff), (uint8_t)((hum_conv & 0xff)),
                           (uint8_t)((temp_conv >> 8) & 0xff), (uint8_t)((temp_conv & 0xff))});
}
void CCS811Component::dump_config() {
  ESP_LOGCONFIG(TAG, "CCS811");
  LOG_I2C_DEVICE(this)
  LOG_UPDATE_INTERVAL(this)
  LOG_SENSOR("  ", "CO2 Sensor", this->co2_)
  LOG_SENSOR("  ", "TVOC Sensor", this->tvoc_)
  LOG_TEXT_SENSOR("  ", "Firmware Version Sensor", this->version_)
  if (this->baseline_) {
    ESP_LOGCONFIG(TAG, "  Baseline: %04X", *this->baseline_);
  } else {
    ESP_LOGCONFIG(TAG, "  Baseline: NOT SET");
  }
  if (this->is_failed()) {
    switch (this->error_code_) {
      case COMMUNICATION_FAILED:
        ESP_LOGW(TAG, "Communication failed! Is the sensor connected?");
        break;
      case INVALID_ID:
        ESP_LOGW(TAG, "Sensor reported an invalid ID. Is this a CCS811?");
        break;
      case SENSOR_REPORTED_ERROR:
        ESP_LOGW(TAG, "Sensor reported internal error");
        break;
      case APP_INVALID:
        ESP_LOGW(TAG, "Sensor reported invalid APP installed.");
        break;
      case APP_START_FAILED:
        ESP_LOGW(TAG, "Sensor reported APP start failed.");
        break;
      case UNKNOWN:
      default:
        ESP_LOGW(TAG, "Unknown setup error!");
        break;
    }
  }
}

}  // namespace ccs811
}  // namespace esphome
