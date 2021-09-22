#include "rc522_i2c.h"
#include "esphome/core/log.h"

namespace esphome {
namespace rc522_i2c {

static const char *const TAG = "rc522_i2c";

void RC522I2C::dump_config() {
  RC522::dump_config();
  LOG_I2C_DEVICE(this);
}

/**
 * Reads a uint8_t from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
uint8_t RC522I2C::pcd_read_register(PcdRegister reg  ///< The register to read from. One of the PCD_Register enums.
) {
  uint8_t value;
  if (!read_byte(reg >> 1, &value))
    return 0;
  ESP_LOGVV(TAG, "read_register_(%x) -> %u", reg, value);
  return value;
}

/**
 * Reads a number of uint8_ts from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void RC522I2C::pcd_read_register(PcdRegister reg,  ///< The register to read from. One of the PCD_Register enums.
                                 uint8_t count,    ///< The number of uint8_ts to read
                                 uint8_t *values,  ///< uint8_t array to store the values in.
                                 uint8_t rx_align  ///< Only bit positions rxAlign..7 in values[0] are updated.
) {
  if (count == 0) {
    return;
  }

  uint8_t b = values[0];
  read_bytes(reg >> 1, values, count);

  if (rx_align)  // Only update bit positions rxAlign..7 in values[0]
  {
    // Create bit mask for bit positions rxAlign..7
    uint8_t mask = 0xFF << rx_align;
    // Apply mask to both current value of values[0] and the new data in values array.
    values[0] = (b & ~mask) | (values[0] & mask);
  }
}

void RC522I2C::pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                  uint8_t value     ///< The value to write.
) {
  this->write_byte(reg >> 1, value);
}

/**
 * Writes a number of uint8_ts to the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void RC522I2C::pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                  uint8_t count,    ///< The number of uint8_ts to write to the register
                                  uint8_t *values   ///< The values to write. uint8_t array.
) {
  write_bytes(reg >> 1, values, count);
}

}  // namespace rc522_i2c
}  // namespace esphome
