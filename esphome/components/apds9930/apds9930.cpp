#include "apds9930.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::apds9930 {

ESPHOME_LOG_TAG(TAG, "apds9930");

// Register addresses
static constexpr uint8_t APDS9930_ENABLE = 0x00;
static constexpr uint8_t APDS9930_ATIME = 0x01;
static constexpr uint8_t APDS9930_PTIME = 0x02;
static constexpr uint8_t APDS9930_PPULSE = 0x0E;
static constexpr uint8_t APDS9930_CONTROL = 0x0F;
static constexpr uint8_t APDS9930_ID = 0x12;
static constexpr uint8_t APDS9930_STATUS = 0x13;

// Command byte: repeated byte and auto-increment protocols
static constexpr uint8_t APDS9930_CMD = 0x80;
static constexpr uint8_t APDS9930_CMD_AUTO_INCREMENT = 0xA0;

static constexpr uint8_t APDS9930_CHIP_ID = 0x39;

// ENABLE register bits
static constexpr uint8_t APDS9930_PON = 0x01;
static constexpr uint8_t APDS9930_AEN = 0x02;
static constexpr uint8_t APDS9930_PEN = 0x04;

// STATUS register bits
static constexpr uint8_t APDS9930_AVALID = 0x01;
static constexpr uint8_t APDS9930_PVALID = 0x02;

// CONTROL register fields
static constexpr uint8_t CONTROL_PDRIVE_SHIFT = 6;
static constexpr uint8_t CONTROL_PDIODE_SHIFT = 4;
static constexpr uint8_t CONTROL_PGAIN_SHIFT = 2;
static constexpr uint8_t CONTROL_AGAIN_SHIFT = 0;
static constexpr uint8_t CONTROL_PDIODE_CH1 = 0b10;  // The only supported proximity diode

static constexpr uint8_t APDS9930_ATIME_VALUE = 0xED;   // 19 cycles, about 52 ms
static constexpr uint8_t APDS9930_PTIME_VALUE = 0xFF;   // 2.73 ms
static constexpr uint8_t APDS9930_PPULSE_VALUE = 0x08;  // 8 pulses

// Lux calculation coefficients
static constexpr float APDS9930_DF = 52.0f;
static constexpr float APDS9930_GA = 0.49f;
static constexpr float APDS9930_ALS_B = 1.862f;
static constexpr float APDS9930_ALS_C = 0.746f;
static constexpr float APDS9930_ALS_D = 1.291f;
static constexpr float APDS9930_ALSIT_MS = 2.73f * (256 - APDS9930_ATIME_VALUE);

static constexpr uint8_t AMBIENT_GAINS[4] = {1, 8, 16, 120};
static constexpr uint8_t PROXIMITY_GAINS[4] = {1, 2, 4, 8};
static constexpr float LED_CURRENTS_MA[4] = {100.0f, 50.0f, 25.0f, 12.5f};

// Lux per count at 1x gain and the fixed integration time
static constexpr float LUX_PER_COUNT_1X = APDS9930_GA * APDS9930_DF / APDS9930_ALSIT_MS;
// Each integration cycle adds up to 1024 counts, so this is the ceiling of both light channels
static constexpr uint16_t ALS_SATURATION = 1024 * (256 - APDS9930_ATIME_VALUE);

bool APDS9930Component::write_reg_(uint8_t reg, uint8_t value) { return this->write_byte(reg | APDS9930_CMD, value); }

void APDS9930Component::setup() {
  uint8_t id;
  if (!this->read_byte(APDS9930_ID | APDS9930_CMD, &id)) {
    this->mark_failed(LOG_STR("Communication failed"));
    return;
  }
  if (id != APDS9930_CHIP_ID) {
    ESP_LOGE(TAG, "Wrong ID 0x%02X (expected 0x%02X)", id, APDS9930_CHIP_ID);
    this->mark_failed(LOG_STR("Wrong chip ID"));
    return;
  }

  const uint8_t control =
      (this->led_drive_ & 0b11) << CONTROL_PDRIVE_SHIFT | CONTROL_PDIODE_CH1 << CONTROL_PDIODE_SHIFT |
      (this->proximity_gain_ & 0b11) << CONTROL_PGAIN_SHIFT | (this->ambient_gain_ & 0b11) << CONTROL_AGAIN_SHIFT;

  uint8_t enable = APDS9930_PON;
  if (this->illuminance_sensor_ != nullptr)
    enable |= APDS9930_AEN;
  if (this->proximity_sensor_ != nullptr)
    enable |= APDS9930_PEN;

  if (!this->write_reg_(APDS9930_ENABLE, 0x00) || !this->write_reg_(APDS9930_ATIME, APDS9930_ATIME_VALUE) ||
      !this->write_reg_(APDS9930_PTIME, APDS9930_PTIME_VALUE) ||
      !this->write_reg_(APDS9930_PPULSE, APDS9930_PPULSE_VALUE) || !this->write_reg_(APDS9930_CONTROL, control) ||
      !this->write_reg_(APDS9930_ENABLE, enable)) {
    this->mark_failed(LOG_STR("Configuration failed"));
  }
}

void APDS9930Component::dump_config() {
  ESP_LOGCONFIG(TAG,
                "APDS9930:\n"
                "  LED Drive: %.1f mA\n"
                "  Proximity Gain: %ux\n"
                "  Ambient Light Gain: %ux",
                LED_CURRENTS_MA[this->led_drive_], PROXIMITY_GAINS[this->proximity_gain_],
                AMBIENT_GAINS[this->ambient_gain_]);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Proximity", this->proximity_sensor_);
}

void APDS9930Component::update() {
  // STATUS, CH0DATAL/H, CH1DATAL/H, PDATAL/H
  uint8_t raw[7];
  if (!this->read_bytes(APDS9930_STATUS | APDS9930_CMD_AUTO_INCREMENT, raw, sizeof(raw))) {
    this->status_set_warning(LOG_STR("Reading data failed"));
    return;
  }
  this->status_clear_warning();
  const uint8_t status = raw[0];

  if (this->illuminance_sensor_ != nullptr && (status & APDS9930_AVALID) != 0) {
    const uint16_t ch0 = encode_uint16(raw[2], raw[1]);
    const uint16_t ch1 = encode_uint16(raw[4], raw[3]);
    if (ch0 >= ALS_SATURATION || ch1 >= ALS_SATURATION) {
      // A saturated channel would give a lux value that is too low, not too high
      ESP_LOGW(TAG, "Light channel saturated, lower ambient_light_gain");
      this->illuminance_sensor_->publish_state(NAN);
    } else {
      const float iac = std::max({ch0 - APDS9930_ALS_B * ch1, APDS9930_ALS_C * ch0 - APDS9930_ALS_D * ch1, 0.0f});
      this->illuminance_sensor_->publish_state(iac * LUX_PER_COUNT_1X / AMBIENT_GAINS[this->ambient_gain_]);
    }
  }

  if (this->proximity_sensor_ != nullptr && (status & APDS9930_PVALID) != 0) {
    this->proximity_sensor_->publish_state(encode_uint16(raw[6], raw[5]));
  }
}

}  // namespace esphome::apds9930
