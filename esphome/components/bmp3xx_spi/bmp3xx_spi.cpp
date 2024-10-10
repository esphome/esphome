#include "bmp3xx_spi.h"
#include <cinttypes>
#include "esphome/core/helpers.h"

namespace esphome {
namespace bmp3xx_spi {

static const char *const TAG = "bmp3xx_spi.sensor";

uint8_t set_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num | mask;
}

uint8_t clear_bit(uint8_t num, int position) {
  int mask = 1 << position;
  return num & ~mask;
}

void BMP3XXSPIComponent::setup() {
  this->spi_setup();
  BMP3XXComponent::setup();
}

// SPI read
// Reading is done by lowering CSB and first sending one control byte. The control bytes consist of the SPI register
// address (= full register address without bit 7) and the read command (bit 7 = RW = ‘1’). After writing the control
// byte, one dummy byte is sent and there after data bytes. The register address is automatically incremented.
// https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp390-ds002.pdf

bool BMP3XXSPIComponent::read_byte(uint8_t a_register, uint8_t *data) {
  this->enable();
  uint8_t rg = set_bit(a_register, 7);
  this->transfer_byte(rg);
  this->transfer_byte(0);
  *data = this->transfer_byte(0);
  ESP_LOGVV(TAG, "Read register %02x (sent %02x), Byte received: %02x", a_register, rg, *data);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::write_byte(uint8_t a_register, uint8_t data) {
  this->enable();
  uint8_t rg = clear_bit(a_register, 7);
  ESP_LOGVV(TAG, "Write register %02x (sent %02x), Byte received: %02x", a_register, rg, data);
  this->transfer_byte(rg);
  this->transfer_byte(data);
  this->disable();
  return true;
}

bool BMP3XXSPIComponent::read_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  uint8_t rg = set_bit(a_register, 7);
  this->transfer_byte(rg);
  this->read_array(data, len);
  ESP_LOGVV(TAG, "Read bytes. register %02x (sent %02x), Bytes received: %s", a_register, rg,
            format_hex_pretty(data, len).c_str());

  this->disable();
  return true;
}

bool BMP3XXSPIComponent::write_bytes(uint8_t a_register, uint8_t *data, size_t len) {
  this->enable();
  this->transfer_byte(clear_bit(a_register, 7));
  this->transfer_array(data, len);
  this->disable();
  return true;
}

}  // namespace bmp3xx_spi
}  // namespace esphome
