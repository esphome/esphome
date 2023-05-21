#include "hp303b_spi.h"

namespace esphome {
namespace hp303b_spi {
static const char *TAG = "hp303b_spi";

void HP303BComponentSPI::setup() {
  ESP_LOGI(TAG, "HP303B setup started!");
  this->spi_setup();

  this->cs_->digital_write(false);
  delay(10);
  ESP_LOGI(TAG, "SPI setup finished!");
  HP303BComponent::setup();
}

void HP303BComponentSPI::dump_config() {
  HP303BComponent::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

int16_t HP303BComponentSPI::read_byte(uint8_t reg_address) {
  // this function is only made for communication via SPI

  // mask regAddress
  reg_address &= ~HP303B__SPI_RW_MASK;
  // reserve and initialize bus
  // m_spibus->beginTransaction(SPISettings(HP303B__SPI_MAX_FREQ, MSBFIRST, SPI_MODE3));
  // enable ChipSelect for HP303B
  // digitalWrite(m_chipSelect, LOW);
  this->enable();
  // send address with read command to HP303B
  this->write_byte(reg_address | HP303B__SPI_READ_CMD);
  // receive register content from HP303B
  uint8_t ret = this->write_byte(0xFF);  // send a dummy byte while receiving
                                         // disable ChipSelect for HP303B
  // digitalWrite(m_chipSelect, HIGH);
  this->disable();
  // close current SPI transaction
  // m_spibus->endTransaction();
  // return received data
  return ret;
}

int16_t HP303BComponentSPI::read_block(uint8_t reg_address, uint8_t length, uint8_t *buffer) {
  // this function is only made for communication via SPI

  // do not read if there is no buffer
  if (buffer == NULL) {
    return 0;  // 0 bytes were read successfully
  }
  // mask regAddress
  reg_address &= ~HP303B__SPI_RW_MASK;
  // reserve and initialize bus
  // m_spibus->beginTransaction(SPISettings(HP303B__SPI_MAX_FREQ, MSBFIRST, SPI_MODE3));
  // enable ChipSelect for HP303B
  // digitalWrite(m_chipSelect, LOW);
  this->enable();
  // send address with read command to H303B
  this->write_byte(reg_address | HP303B__SPI_READ_CMD);

  // receive register contents from HP303B
  for (uint8_t count = 0; count < length; count++) {
    buffer[count] = this->write_byte(0xFF);  // send a dummy byte while receiving
  }

  // disable ChipSelect for HP303B
  // digitalWrite(m_chipSelect, HIGH);
  this->disable();
  // close current SPI transaction
  // m_spibus->endTransaction();
  // return received data
  return length;
}

int16_t HP303BComponentSPI::write_byte(uint8_t reg_address, uint8_t data, uint8_t check) {
  // this function is only made for communication via SPI

  // mask regAddress
  reg_address &= ~HP303B__SPI_RW_MASK;
  // reserve and initialize bus

  // m_spibus->beginTransaction(SPISettings(HP303B__SPI_MAX_FREQ, MSBFIRST, SPI_MODE3));
  //  enable ChipSelect for HP303B
  // digitalWrite(m_chipSelect, LOW);
  this->enable();
  // send address with read command to HP303B
  this->write_byte(reg_address | HP303B__SPI_WRITE_CMD);

  // write register content from HP303B
  this->write_byte(data);

  // disable ChipSelect for HP303B
  // digitalWrite(m_chipSelect, HIGH);
  this->disable();
  // close current SPI transaction
  // m_spibus->endTransaction();

  // check if necessary
  if (check == 0) {
    // no checking necessary
    return HP303B__SUCCEEDED;
  }
  // checking necessary
  if (read_byte(reg_address) == data) {
    // check passed
    return HP303B__SUCCEEDED;
  } else {
    // check failed
    return HP303B__FAIL_UNKNOWN;
  }
}

int16_t HP303BComponentSPI::set_interrupt_polarity(uint8_t polarity) { return HP303BComponent::P303B__FAIL_UNKNOWN; }

}  // namespace hp303b_spi
}  // namespace esphome