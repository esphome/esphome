#pragma once
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/helpers.h"

#include <vector>

namespace esphome {
namespace sensirion_common {

/**
 * Implementation of I2C functions for Sensirion sensors
 * Sensirion data requires CRC checking.
 * Each 16 bit word is/must be followed 8 bit CRC code
 * (Applies to read and write - note the I2C command code doesn't need a CRC)
 * Format:
 *   | 16 Bit Command Code | 16 bit Data word 1 | CRC of DW 1 | 16 bit Data word 1 | CRC of DW 2 | ..
 */
static const uint8_t CRC_POLYNOMIAL = 0x31;  // default for Sensirion

class SensirionI2CDevice : public i2c::I2CDevice {
 public:
  enum CommandLen : uint8_t { ADDR_8_BIT = 1, ADDR_16_BIT = 2 };

  /** Read data words from I2C device.
   * handles CRC check used by Sensirion sensors
   * @param data pointer to raw result
   * @param len number of words to read
   * @return true if reading succeeded
   */
  bool read_data(uint16_t *data, uint8_t len);

  /** Read 1 data word from I2C device.
   * @param data reference to raw result
   * @return true if reading succeeded
   */
  bool read_data(uint16_t &data) { return this->read_data(&data, 1); }

  /** get data words from I2C register.
   * handles CRC check used by Sensirion sensors
   * @param  I2C register
   * @param data pointer to raw result
   * @param len number of words to read
   * @param delay milliseconds to to wait between sending the I2C command and reading the result
   * @return true if reading succeeded
   */
  bool get_register(uint16_t command, uint16_t *data, uint8_t len, uint8_t delay = 0) {
    return get_register_(command, ADDR_16_BIT, data, len, delay);
  }
  /** Read 1 data word from 16 bit I2C register.
   * @param  I2C register
   * @param data reference to raw result
   * @param delay milliseconds to to wait between sending the I2C command and reading the result
   * @return true if reading succeeded
   */
  bool get_register(uint16_t i2c_register, uint16_t &data, uint8_t delay = 0) {
    return this->get_register_(i2c_register, ADDR_16_BIT, &data, 1, delay);
  }

  /** get data words from I2C register.
   * handles CRC check used by Sensirion sensors
   * @param  I2C register
   * @param data pointer to raw result
   * @param len number of words to read
   * @param delay milliseconds to to wait between sending the I2C command and reading the result
   * @return true if reading succeeded
   */
  bool get_8bit_register(uint8_t i2c_register, uint16_t *data, uint8_t len, uint8_t delay = 0) {
    return get_register_(i2c_register, ADDR_8_BIT, data, len, delay);
  }

  /** Read 1 data word from 8 bit I2C register.
   * @param  I2C register
   * @param data reference to raw result
   * @param delay milliseconds to to wait between sending the I2C command and reading the result
   * @return true if reading succeeded
   */
  bool get_8bit_register(uint8_t i2c_register, uint16_t &data, uint8_t delay = 0) {
    return this->get_register_(i2c_register, ADDR_8_BIT, &data, 1, delay);
  }

  /** Write a command to the I2C device.
   * @param command I2C command to send
   * @return true if reading succeeded
   */
  template<class T> bool write_command(T i2c_register) { return write_command(i2c_register, nullptr, 0); }

  /** Write a command and one data word to the I2C device .
   * @param command I2C command to send
   * @param data argument for the I2C command
   * @return true if reading succeeded
   */
  template<class T> bool write_command(T i2c_register, uint16_t data) { return write_command(i2c_register, &data, 1); }

  /** Write a command with arguments as words
   * @param i2c_register I2C command to send - an be uint8_t or uint16_t
   * @param data vector<uint16> arguments for the I2C command
   * @return true if reading succeeded
   */
  template<class T> bool write_command(T i2c_register, const std::vector<uint16_t> &data) {
    return write_command_(i2c_register, sizeof(T), data.data(), data.size());
  }

  /** Write a command with arguments as words
   * @param i2c_register I2C command to send - an be uint8_t or uint16_t
   * @param data arguments for the I2C command
   * @param len number of arguments (words)
   * @return true if reading succeeded
   */
  template<class T> bool write_command(T i2c_register, const uint16_t *data, uint8_t len) {
    // limit to 8 or 16 bit only
    static_assert(sizeof(i2c_register) == 1 || sizeof(i2c_register) == 2, "Only 8 or 16 bit command types supported");
    return write_command_(i2c_register, CommandLen(sizeof(T)), data, len);
  }

 protected:
  /** Write a command with arguments as words
   * @param command I2C command to send can be uint8_t or uint16_t
   * @param command_len either 1 for short 8 bit command or 2 for 16 bit command codes
   * @param data arguments for the I2C command
   * @param data_len number of arguments (words)
   * @return true if reading succeeded
   */
  bool write_command_(uint16_t command, CommandLen command_len, const uint16_t *data, uint8_t data_len);

  /** get data words from I2C register.
   * handles CRC check used by Sensirion sensors
   * @param  I2C register
   * @param command_len either 1 for short 8 bit command or 2 for 16 bit command codes
   * @param data pointer to raw result
   * @param len number of words to read
   * @param delay milliseconds to to wait between sending the I2C command and reading the result
   * @return true if reading succeeded
   */
  bool get_register_(uint16_t reg, CommandLen command_len, uint16_t *data, uint8_t len, uint8_t delay);

  /** last error code from I2C operation
   */
  i2c::ErrorCode last_error_;
};

}  // namespace sensirion_common
}  // namespace esphome
