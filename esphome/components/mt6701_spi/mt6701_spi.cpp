#include "mt6701_spi.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mt6701_spi {

ESPHOME_LOG_TAG(TAG, "mt6701_spi");

// The frame's 6-bit CRC (x^6 + x + 1, MSB first) over its first 18 bits is the core 8-bit CRC with the
// polynomial shifted up by two, so a good frame checks to zero as a whole
static constexpr uint8_t CRC6_AS_CRC8_POLY = 0x0C;

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
  }
}

void MT6701SPIComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MT6701 (SSI/SPI):");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
}

bool MT6701SPIComponent::read_count(uint16_t &count) {
  // The 24-bit SSI frame is a 14-bit angle, 4-bit status and 6-bit CRC, most significant bit first
  uint8_t buffer[3] = {0, 0, 0};
  this->enable();
  this->read_array(buffer, 3);
  this->disable();
  if (crc8(buffer, 3, 0x00, CRC6_AS_CRC8_POLY, true) != 0)
    return false;

  const uint32_t frame = encode_uint24(buffer[0], buffer[1], buffer[2]);
  count = frame >> 10;
  this->status_ = (frame >> 6) & 0x0F;
  return true;
}

}  // namespace esphome::mt6701_spi
