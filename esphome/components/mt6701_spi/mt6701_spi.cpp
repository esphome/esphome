#include "mt6701_spi.h"
#include "esphome/core/log.h"

namespace esphome::mt6701_spi {

static const char *const TAG = "mt6701_spi";

// ESPHome's core CRC helpers are 8 and 16 bits wide; the SSI frame uses a 6-bit CRC.
uint8_t crc6_mt6701(uint32_t data18) {
  uint8_t crc = 0;
  for (int8_t i = 17; i >= 0; i--) {
    uint8_t bit = ((data18 >> i) & 0x01) ^ ((crc >> 5) & 0x01);
    crc = (crc << 1) & 0x3F;
    if (bit != 0)
      crc ^= 0x03;  // x^6 + x + 1 -> feedback taps at bit 1 and bit 0
  }
  return crc;
}

void MT6701SPIComponent::setup() {
  this->spi_setup();
  // Noise passes the 6-bit CRC 1 time in 64 and a stuck-low data line reads all-zero
  // frames that pass it too, so a present encoder must send 3 valid frames, one non-zero.
  uint8_t valid = 0;
  bool nonzero = false;
  for (uint8_t i = 0; i < 10 && (valid < 3 || !nonzero); i++) {
    uint16_t count;
    if (this->read_count(count)) {
      valid++;
      if (count != 0)
        nonzero = true;
    }
  }
  if (valid < 3 || !nonzero) {
    ESP_LOGE(TAG, "No valid MT6701 frame received (check wiring / CS pin)");
    this->mark_failed();
    return;
  }
  this->setup_complete_ = true;
}

void MT6701SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MT6701 (SSI/SPI):");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

bool MT6701SPIComponent::read_count(uint16_t &count) {
  // The 24-bit SSI frame is a 14-bit angle, 4-bit status and 6-bit CRC over the
  // first 18 bits, most significant bit first.
  uint8_t buffer[3] = {0, 0, 0};
  this->enable();
  this->read_array(buffer, 3);
  this->disable();

  uint32_t frame = encode_uint24(buffer[0], buffer[1], buffer[2]);
  uint32_t data18 = frame >> 6;
  uint8_t crc = frame & 0x3F;
  if (crc6_mt6701(data18) != crc)
    return false;

  count = data18 >> 4;
  uint8_t status = data18 & 0x0F;  // bit3 track loss, bit2 push button, bits1:0 field strength

  // Hold entity states back until the setup probe has confirmed the encoder; the
  // 0xFF sentinel then makes the first read after setup publish them.
  if (this->setup_complete_ && status != this->last_status_) {
    this->last_status_ = status;
#ifdef USE_BINARY_SENSOR
    if (this->push_button_binary_sensor_ != nullptr)
      this->push_button_binary_sensor_->publish_state((status & 0x04) != 0);
    if (this->track_loss_binary_sensor_ != nullptr)
      this->track_loss_binary_sensor_->publish_state((status & 0x08) != 0);
#endif
#ifdef USE_TEXT_SENSOR
    if (this->field_status_text_sensor_ != nullptr) {
      const char *text;
      switch (status & 0x03) {
        case static_cast<uint8_t>(MT6701FieldStatus::MT6701_FIELD_STATUS_NORMAL):
          text = "OK";
          break;
        case static_cast<uint8_t>(MT6701FieldStatus::MT6701_FIELD_STATUS_TOO_STRONG):
          text = "TOO_STRONG";
          break;
        case static_cast<uint8_t>(MT6701FieldStatus::MT6701_FIELD_STATUS_TOO_WEAK):
          text = "TOO_WEAK";
          break;
        default:  // 0b11 is reserved in the datasheet
          text = "UNKNOWN";
          break;
      }
      this->field_status_text_sensor_->publish_state(text);
    }
#endif
  }
  return true;
}

}  // namespace esphome::mt6701_spi
