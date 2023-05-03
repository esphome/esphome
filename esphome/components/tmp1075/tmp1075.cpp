#include "esphome/core/log.h"
#include "tmp1075.h"

namespace esphome {
namespace tmp1075 {

static const char *const TAG = "tmp1075";

constexpr uint8_t REG_TEMP = 0x0;   // Temperature result
constexpr uint8_t REG_CFGR = 0x1;   // Configuration
constexpr uint8_t REG_LLIM = 0x2;   // Low limit
constexpr uint8_t REG_HLIM = 0x3;   // High limit
constexpr uint8_t REG_DIEID = 0xF;  // Device ID

constexpr uint16_t EXPECT_DIEID = 0x0075;  // Expected Device ID.

static uint16_t temp2regvalue(const float temp);
static float regvalue2temp(const uint16_t regvalue);

void TMP1075Sensor::setup() {
  uint8_t dieID;
  if (!this->read_byte(REG_DIEID, &dieID)) {
    ESP_LOGW(TAG, "'%s' - unable to read ID", this->name_.c_str());
    this->mark_failed();
    return;
  }
  if (dieID != EXPECT_DIEID) {
    ESP_LOGW(TAG, "'%s' - unexpected ID 0x%x found, expected 0x%x", this->name_.c_str(), dieID, EXPECT_DIEID);
    this->mark_failed();
    return;
  }

  this->write_config();
}

void TMP1075Sensor::update() {
  uint16_t regvalue;
  if (!read_byte_16(REG_TEMP, &regvalue)) {
    ESP_LOGW(TAG, "'%s' - unable to read temperature register", this->name_.c_str());
    this->status_set_warning();
    return;
  }

  const float temp = regvalue2temp(regvalue);
  this->publish_state(temp);
}

void TMP1075Sensor::dump_config() {
  LOG_SENSOR("", "TMP1075 Sensor", this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "  Communication with TMP1075 failed!");
    return;
  }
  ESP_LOGCONFIG(TAG, "  limit low  : %.4f °C", alert_limit_low_);
  ESP_LOGCONFIG(TAG, "  limit high : %.4f °C", alert_limit_high_);
  ESP_LOGCONFIG(TAG, "  oneshot    : %d", config_.fields.oneshot);
  ESP_LOGCONFIG(TAG, "  rate       : %d", config_.fields.rate);
  ESP_LOGCONFIG(TAG, "  fault_count: %d", config_.fields.faults);
  ESP_LOGCONFIG(TAG, "  polarity   : %d", config_.fields.polarity);
  ESP_LOGCONFIG(TAG, "  alert_mode : %d", config_.fields.alert_mode);
  ESP_LOGCONFIG(TAG, "  shutdown   : %d", config_.fields.shutdown);
}

void TMP1075Sensor::set_alert_limit_low(const float temp) { this->alert_limit_low_ = temp; }
void TMP1075Sensor::set_alert_limit_high(const float temp) { this->alert_limit_high_ = temp; }
void TMP1075Sensor::set_oneshot(const bool oneshot) { config_.fields.oneshot = oneshot; }
void TMP1075Sensor::set_conversion_rate(const enum eConversionRate rate) { config_.fields.rate = rate; }
void TMP1075Sensor::set_fault_count(const int faults) {
  if (faults < 1) {
    ESP_LOGE(TAG, "'%s' - fault_count too low: %d", this->name_.c_str(), faults);
    return;
  }
  if (faults > 4) {
    ESP_LOGE(TAG, "'%s' - fault_count too high: %d", this->name_.c_str(), faults);
    return;
  }
  config_.fields.faults = faults - 1;
}
void TMP1075Sensor::set_alert_polarity(const bool polarity) { config_.fields.polarity = polarity; }
void TMP1075Sensor::set_alert_mode(const bool alert_mode) { config_.fields.alert_mode = alert_mode; }
void TMP1075Sensor::set_shutdown(const bool shutdown) { config_.fields.shutdown = shutdown; }

void TMP1075Sensor::load_config() {
  uint16_t regvalue;
  if (!read_byte_16(REG_CFGR, &regvalue)) {
    ESP_LOGW(TAG, "'%s' - unable to read configuration register", this->name_.c_str());
    return;
  }
  config_.regvalue = regvalue;
  ESP_LOGD(TAG, "'%s' - loaded config register %04x", this->name_.c_str(), config_.regvalue);
  logd_config();
}

void TMP1075Sensor::logd_config() {
  ESP_LOGD(TAG, "  oneshot   : %d", config_.fields.oneshot);
  ESP_LOGD(TAG, "  rate      : %d", config_.fields.rate);
  ESP_LOGD(TAG, "  faults    : %d", config_.fields.faults);
  ESP_LOGD(TAG, "  polarity  : %d", config_.fields.polarity);
  ESP_LOGD(TAG, "  alert_mode: %d", config_.fields.alert_mode);
  ESP_LOGD(TAG, "  shutdown  : %d", config_.fields.shutdown);
}

void TMP1075Sensor::write_config() {
  send_alert_limit_low();
  send_alert_limit_high();
  send_config();
}

void TMP1075Sensor::send_config() {
  ESP_LOGD(TAG, "'%s' - sending configuration %04x", this->name_.c_str(), config_.regvalue);
  logd_config();
  if (!this->write_byte_16(REG_CFGR, config_.regvalue)) {
    ESP_LOGW(TAG, "'%s' - unable to write configuration register", this->name_.c_str());
    return;
  }
}

void TMP1075Sensor::send_alert_limit_low() {
  ESP_LOGD(TAG, "'%s' - sending alert limit low %.3f °C", this->name_.c_str(), alert_limit_low_);
  const uint16_t regvalue = temp2regvalue(alert_limit_low_);
  if (!this->write_byte_16(REG_LLIM, regvalue)) {
    ESP_LOGW(TAG, "'%s' - unable to write low limit register", this->name_.c_str());
    return;
  }
}

void TMP1075Sensor::send_alert_limit_high() {
  ESP_LOGD(TAG, "'%s' - sending alert limit high %.3f °C", this->name_.c_str(), alert_limit_high_);
  const uint16_t regvalue = temp2regvalue(alert_limit_high_);
  if (!this->write_byte_16(REG_HLIM, regvalue)) {
    ESP_LOGW(TAG, "'%s' - unable to write high limit register", this->name_.c_str());
    return;
  }
}

static uint16_t temp2regvalue(const float temp) {
  const uint16_t regvalue = temp / 0.0625f;
  return regvalue << 4;
}

static float regvalue2temp(const uint16_t regvalue) {
  const int16_t signed_value = regvalue;
  return (signed_value >> 4) * 0.0625f;
}

}  // namespace tmp1075
}  // namespace esphome
