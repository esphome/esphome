#include "mt6701_spi.h"
#include "esphome/core/log.h"

namespace esphome::mt6701_spi {

static const char *const TAG = "mt6701_spi";

void MT6701SPIComponent::setup() {
  ESP_LOGCONFIG(TAG, "Running setup");
  this->spi_setup();
  // Probe: require several CRC-valid frames, at least one of them non-zero.
  // Random noise passes the 6-bit CRC ~1/64 of the time, and a dead or
  // stuck-low data line reads all-zero frames which pass trivially
  // (crc6(0) == 0) - neither may count as a present encoder. A real encoder
  // sitting at exactly count 0 with zero jitter would be misdetected here, but
  // that is a 1-in-16384 alignment and the angle LSBs always jitter in
  // practice.
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
  if (this->is_failed())
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
}

bool MT6701SPIComponent::read_count(uint16_t &count) {
  // The SSI frame is 24 bits: 14-bit angle, 4-bit status, 6-bit CRC.
  uint8_t buffer[3] = {0, 0, 0};
  this->enable();
  this->read_array(buffer, 3);
  this->disable();

  // Frame layout: 14-bit angle, 4-bit status, 6-bit CRC (most significant
  // first). The CRC covers the top 18 bits (angle + status).
  uint32_t frame = encode_uint24(buffer[0], buffer[1], buffer[2]);
  uint32_t data18 = frame >> 6;
  uint8_t crc = frame & 0x3F;
  if (mt6701::crc6_mt6701(data18) != crc)
    return false;

  count = data18 >> 4;
  uint8_t status = data18 & 0x0F;  // bit3 track loss, bit2 push button, bits1:0 field strength

  // Publish only after the setup probe confirmed the device: noise frames that
  // happen to pass the CRC during probing must not emit entity states. The
  // 0xFF sentinel stays untouched until then, so the first post-setup read
  // publishes the initial states.
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
        case static_cast<uint8_t>(mt6701::MT6701FieldStatus::NORMAL):
          text = "OK";
          break;
        case static_cast<uint8_t>(mt6701::MT6701FieldStatus::TOO_STRONG):
          text = "TOO_STRONG";
          break;
        case static_cast<uint8_t>(mt6701::MT6701FieldStatus::TOO_WEAK):
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
