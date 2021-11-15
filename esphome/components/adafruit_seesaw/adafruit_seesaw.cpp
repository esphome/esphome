#include "adafruit_seesaw.h"
#include "esphome/core/log.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace adafruit_seesaw {

static const char *const TAG = "adafruit_seesaw";

/**
 * @brief Set the mode of a GPIO Pin
 *
 * @param pin the pin number - the pin number on the SAMD09 breakout, they correspond to the number on the
 * silkscreen.
 * @param mode the mode to set the pin up with. INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN
 */
void AdafruitSeesaw::pin_mode(uint8_t pin, uint8_t mode) {
  if (pin >= 32) {
    pin_mode_bulk(0, 1ul << (pin - 32), mode);
  } else {
    pin_mode_bulk(1ul << pin, mode);
  }
}

/**
 * @brief Sets the mode of multiple GPIO Pins
 *
 * @param pins a group of pins - the pins that match up on the SAMD09 breakout, they correspond to the number on the
 * silkscreen
 * @param mode the mode to set the pin up with. INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN
 */
void AdafruitSeesaw::pin_mode_bulk(uint32_t pins, uint8_t mode) {
  switch (mode) {
    case OUTPUT:
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRSET_BULK, pins);
      break;
    case INPUT:
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRCLR_BULK, pins);
      break;
    case INPUT_PULLUP:
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRCLR_BULK, pins);
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_PULLENSET, pins);
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_BULK_SET, pins);
      break;
    case INPUT_PULLDOWN:
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRCLR_BULK, pins);
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_PULLENSET, pins);
      this->write_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_BULK_CLR, pins);
      break;
  }
}

/**
 * @brief Sets the mode of multiple GPIO Pins. This method supports pins in A and B groups
 *
 * @param pins_a bitmask of the pins to write on Port A
 * @param pins_b bitmask of the pins to write on Port B
 * @param mode mode to set the pin up with. INPUT, OUTPUT, INPUT_PULLUP, INPUT_PULLDOWN
 */
void AdafruitSeesaw::pin_mode_bulk(uint32_t pins_a, uint32_t pins_b, uint8_t mode) {
  uint64_t pins = (uint64_t) pins_a << 32 | (uint64_t) pins_b;

  switch (mode) {
    case OUTPUT:
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRSET_BULK, pins);
      break;
    case INPUT:
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRCLR_BULK, pins);
      break;
    case INPUT_PULLUP:
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRCLR_BULK, pins);
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_PULLENSET, pins);
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_BULK_SET, pins);
      break;
    case INPUT_PULLDOWN:
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_DIRCLR_BULK, pins);
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_PULLENSET, pins);
      this->write_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_BULK_CLR, pins);
      break;
  }
}

/**
 * @brief Read the current status of a GPIO Pin
 *
 * @param pin the pin number - the pin number on the SAMD09 breakout, they correspond to the number on the
 * silkscreen.
 * @return the status of the pin - HIGH or LOW (1 or 0)
 */
bool AdafruitSeesaw::digital_read(uint8_t pin) {
  if (pin >= 32) {
    return digital_read_bulk_b((1ul << (pin - 32))) != 0;
  } else {
    return this->digital_read_bulk((1ul << pin)) != 0;
  }
}

/**
 * @brief Read the status of multiple pins on Port B
 *
 * @param pins a bitmask of the pins to read - on the SAMD09 breakout, they correspond to the number on the
 * silkscreen.
 * @return the status of the passed pins.
 */
uint32_t AdafruitSeesaw::digital_read_bulk_b(uint32_t pins) {
  uint64_t buffer = 0;
  this->read_64((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_BULK, &buffer);

  uint32_t result =
      ((uint32_t) buffer << 24 | ((uint32_t) buffer << 16) | ((uint32_t) buffer << 8) | ((uint32_t) buffer));

  return buffer & pins;
}

/**
 * @brief Read the status of multiple pins on Port A
 *
 * @param pins a bitmask of the pins to read - on the SAMD09 breakout, they correspond to the number on the
 * silkscreen.
 * @return the status of the passed pins.
 */
uint32_t AdafruitSeesaw::digital_read_bulk(uint32_t pins) {
  uint32_t buffer = 0;
  this->read_32((uint16_t) SEESAW_GPIO_BASE << 8 | SEESAW_GPIO_BULK, &buffer);

  return buffer & pins;
}

/**
 * @brief Writes a 8 bit value to the connected device
 *
 * @param reg a bitmask of the base register and secondary register
 * @param value data to write to register
 * @return i2c::ErrorCode
 */
i2c::ErrorCode AdafruitSeesaw::write_8(uint16_t reg, uint8_t value) {
  std::vector<uint8_t> data;
  data.push_back(reg >> 8);
  data.push_back(reg >> 0);
  data.push_back(value);
  return this->write(data.data(), data.size());
}

/**
 * @brief Writes a 16 bit value to the connected device
 *
 * @param reg a bitmask of the base register and secondary register
 * @param value data to write to register
 * @return i2c::ErrorCode
 */
i2c::ErrorCode AdafruitSeesaw::write_16(uint16_t reg, uint16_t value) {
  std::vector<uint8_t> data;
  data.push_back(reg >> 8);
  data.push_back(reg >> 0);
  data.push_back(value >> 8);
  data.push_back(value >> 0);
  return this->write(data.data(), data.size());
}

/**
 * @brief Writes a 32 bit value to the connected device
 *
 * @param reg a bitmask of the base register and secondary register
 * @param value data to write to register
 * @return i2c::ErrorCode
 */
i2c::ErrorCode AdafruitSeesaw::write_32(uint16_t reg, uint32_t value) {
  std::vector<uint8_t> data;
  data.push_back(reg >> 8);
  data.push_back(reg >> 0);
  data.push_back(value >> 24);
  data.push_back(value >> 16);
  data.push_back(value >> 8);
  data.push_back(value >> 0);
  return this->write(data.data(), data.size());
}

/**
 * @brief Writes a 64 bit value to the connected device
 *
 * @param reg a bitmask of the base register and secondary register
 * @param value data to write to register
 * @return i2c::ErrorCode
 */
i2c::ErrorCode AdafruitSeesaw::write_64(uint16_t reg, uint64_t value) {
  std::vector<uint8_t> data;
  data.push_back(reg >> 8);
  data.push_back(reg >> 0);
  data.push_back(value >> 56);
  data.push_back(value >> 48);
  data.push_back(value >> 40);
  data.push_back(value >> 32);
  data.push_back(value >> 24);
  data.push_back(value >> 16);
  data.push_back(value >> 8);
  data.push_back(value >> 0);
  return this->write(data.data(), data.size());
}

/**
 * @brief Reads a 32 bit value from the connected device
 *
 * @param reg a bitmask of the base register and secondary register
 * @param value data to push the return value into
 * @return i2c::ErrorCode
 */
i2c::ErrorCode AdafruitSeesaw::read_32(uint16_t reg, uint32_t *value) {
  uint8_t reg_data[2];
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
  i2c::ErrorCode err = this->write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return err;
  uint8_t recv[4];
  err = this->read(recv, 4);
  if (err != i2c::ERROR_OK)
    return err;
  *value = 0;
  *value |= ((uint32_t) recv[0]) << 24;
  *value |= ((uint32_t) recv[1]) << 16;
  *value |= ((uint32_t) recv[2]) << 8;
  *value |= ((uint32_t) recv[3]);
  return i2c::ERROR_OK;
}

/**
 * @brief Reads a 64 bit value from the connected device
 *
 * @param reg a bitmask of the base register and secondary register
 * @param value data to push the return value into
 * @return i2c::ErrorCode
 */
i2c::ErrorCode AdafruitSeesaw::read_64(uint16_t reg, uint64_t *value) {
  uint8_t reg_data[2];
  reg_data[0] = reg >> 8;
  reg_data[1] = reg >> 0;
  i2c::ErrorCode err = this->write(reg_data, 2);
  if (err != i2c::ERROR_OK)
    return err;
  uint8_t recv[8];
  err = this->read(recv, 8);
  if (err != i2c::ERROR_OK)
    return err;
  *value = 0;
  *value |= ((uint64_t) recv[0]) << 56;
  *value |= ((uint64_t) recv[1]) << 48;
  *value |= ((uint64_t) recv[2]) << 40;
  *value |= ((uint64_t) recv[3]) << 32;
  *value |= ((uint64_t) recv[4]) << 24;
  *value |= ((uint64_t) recv[5]) << 16;
  *value |= ((uint64_t) recv[6]) << 8;
  *value |= ((uint64_t) recv[7]);
  return i2c::ERROR_OK;
}

}  // namespace adafruit_seesaw
}  // namespace esphome
