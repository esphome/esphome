#include "ade7880_spi.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/components/ade7880_base/ade7880_registers.h"

namespace esphome {
namespace ade7880_spi {

static const char *const TAG = "ade7880";

void ADE7880SPI::setup() {
  this->spi_setup();
  ade7880_base::ADE7880::setup();
}

void ADE7880SPI::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7880_spi:");
  LOG_PIN("  CS Pin: ", this->cs_);
  ade7880_base::ADE7880::dump_config();
}

void ADE7880SPI::lock_communication_mode() {
  for (int i = 0; i < 3; i++) {
    this->write_u8_register16(ade7880_base::RESERVED_EBFF, 0x00);
  }
  this->write_u8_register16(ade7880_base::CONFIG2, ade7880_base::CONFIG2_SPI_LOCK);
}

// Register types
// unsigned 8-bit (uint8_t)
// signed 10-bit - 16-bit ZP on wire (int16_t, needs sign extension)
// unsigned 16-bit (uint16_t)
// unsigned 20-bit - 32-bit ZP on wire (uint32_t)
// signed 24-bit - 32-bit ZPSE on wire (int32_t, needs sign extension)
// signed 24-bit - 32-bit ZP on wire (int32_t, needs sign extension)
// signed 24-bit - 32-bit SE on wire (int32_t)
// signed 28-bit - 32-bit ZP on wire (int32_t, needs sign extension)
// unsigned 32-bit (uint32_t)
// signed 32-bit (int32_t)

uint8_t ADE7880SPI::read_u8_register16(uint16_t a_register) {
  uint8_t in;
  this->enable();
  this->write_byte(0x01);
  this->write_byte16(a_register);
  in = this->read_byte();
  this->disable();
  return in;
}

int16_t ADE7880SPI::read_s16_register16(uint16_t a_register) {
  return static_cast<int16_t>(this->read_u16_register16(a_register));
}

uint16_t ADE7880SPI::read_u16_register16(uint16_t a_register) {
  uint16_t in;

  this->enable();
  this->write_byte(0x01);
  this->write_byte16(a_register);
  this->read_array(reinterpret_cast<uint8_t *>(&in), sizeof(in));
  this->disable();
  return convert_big_endian(in);
}

int32_t ADE7880SPI::read_s32_register16(uint16_t a_register) {
  return static_cast<int32_t>(this->read_u32_register16(a_register));
}

uint32_t ADE7880SPI::read_u32_register16(uint16_t a_register) {
  uint32_t in;

  this->enable();
  this->write_byte(0x01);
  this->write_byte16(a_register);
  this->read_array(reinterpret_cast<uint8_t *>(&in), sizeof(in));
  this->disable();
  return convert_big_endian(in);
}

void ADE7880SPI::write_u8_register16(uint16_t a_register, uint8_t value) {
  this->enable();
  this->write_byte(0x00);
  this->write_byte16(a_register);
  this->write_byte(value);
  this->disable();
}

void ADE7880SPI::write_s16_register16(uint16_t a_register, int16_t value) {
  this->write_u16_register16(a_register, static_cast<uint16_t>(value));
}

void ADE7880SPI::write_u16_register16(uint16_t a_register, uint16_t value) {
  uint16_t out = convert_big_endian(value);
  this->enable();
  this->write_byte(0x00);
  this->write_byte16(a_register);
  this->write_byte(value);
  this->write_array(reinterpret_cast<uint8_t *>(&out), sizeof(out));
  this->disable();
}

void ADE7880SPI::write_s32_register16(uint16_t a_register, int32_t value) {
  this->write_u32_register16(a_register, static_cast<uint32_t>(value));
}

void ADE7880SPI::write_u32_register16(uint16_t a_register, uint32_t value) {
  uint32_t out = convert_big_endian(value);
  this->enable();
  this->write_byte(0x00);
  this->write_byte16(a_register);
  this->write_byte(value);
  this->write_array(reinterpret_cast<uint8_t *>(&out), sizeof(out));
  this->disable();
}

}  // namespace ade7880_spi
}  // namespace esphome
