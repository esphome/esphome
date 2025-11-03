#include "i2c_sensirion.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace sensirion_common {

static const char *const TAG = "sensirion_i2c";
// To avoid memory allocations for small writes a stack buffer is used
static const size_t BUFFER_STACK_SIZE = 16;

bool SensirionI2CDevice::read_data(uint16_t *data, const uint8_t len) {
  const uint8_t num_bytes = len * 3;
  uint8_t buf[num_bytes];

  this->last_error_ = this->read(buf, num_bytes);
  if (this->last_error_ != i2c::ERROR_OK) {
    return false;
  }

  for (uint8_t i = 0; i < len; i++) {
    const uint8_t j = 3 * i;
    // Use MSB first since Sensirion devices use CRC-8 with MSB first
    uint8_t crc = crc8(&buf[j], 2, 0xFF, CRC_POLYNOMIAL, true);
    if (crc != buf[j + 2]) {
      ESP_LOGE(TAG, "CRC invalid @ %d! 0x%02X != 0x%02X", i, buf[j + 2], crc);
      this->last_error_ = i2c::ERROR_CRC;
      return false;
    }
    data[i] = encode_uint16(buf[j], buf[j + 1]);
  }
  return true;
}
/***
 * write command with parameters and insert crc
 * use stack array for less than 4 parameters. Most Sensirion I2C commands have less parameters
 */
bool SensirionI2CDevice::write_command_(uint16_t command, CommandLen command_len, const uint16_t *data,
                                        const uint8_t data_len) {
  uint8_t temp_stack[BUFFER_STACK_SIZE];
  std::unique_ptr<uint8_t[]> temp_heap;
  uint8_t *temp;
  size_t required_buffer_len = data_len * 3 + 2;

  // Is a dynamic allocation required ?
  if (required_buffer_len >= BUFFER_STACK_SIZE) {
    temp_heap = std::unique_ptr<uint8_t[]>(new uint8_t[required_buffer_len]);
    temp = temp_heap.get();
  } else {
    temp = temp_stack;
  }
  // First byte or word is the command
  uint8_t raw_idx = 0;
  if (command_len == 1) {
    temp[raw_idx++] = command & 0xFF;
  } else {
    // command is 2 bytes
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    temp[raw_idx++] = command >> 8;
    temp[raw_idx++] = command & 0xFF;
#else
    temp[raw_idx++] = command & 0xFF;
    temp[raw_idx++] = command >> 8;
#endif
  }
  // add parameters followed by crc
  // skipped if len == 0
  for (size_t i = 0; i < data_len; i++) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    temp[raw_idx++] = data[i] >> 8;
    temp[raw_idx++] = data[i] & 0xFF;
#else
    temp[raw_idx++] = data[i] & 0xFF;
    temp[raw_idx++] = data[i] >> 8;
#endif
    // Use MSB first since Sensirion devices use CRC-8 with MSB first
    uint8_t crc = crc8(&temp[raw_idx - 2], 2, 0xFF, CRC_POLYNOMIAL, true);
    temp[raw_idx++] = crc;
  }
  this->last_error_ = this->write(temp, raw_idx);
  return this->last_error_ == i2c::ERROR_OK;
}

bool SensirionI2CDevice::get_register_(uint16_t reg, CommandLen command_len, uint16_t *data, const uint8_t len,
                                       const uint8_t delay_ms) {
  if (!this->write_command_(reg, command_len, nullptr, 0)) {
    ESP_LOGE(TAG, "Write failed: reg=0x%X (%d) err=%d,", reg, command_len, this->last_error_);
    return false;
  }
  delay(delay_ms);
  bool result = this->read_data(data, len);
  if (!result) {
    ESP_LOGE(TAG, "Read failed: reg=0x%X err=%d,", reg, this->last_error_);
  }
  return result;
}

}  // namespace sensirion_common
}  // namespace esphome
