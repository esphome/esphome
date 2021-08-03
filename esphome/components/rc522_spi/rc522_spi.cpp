#include "rc522_spi.h"
#include "esphome/core/log.h"

// Based on:
// - https://github.com/miguelbalboa/rfid

namespace esphome {
namespace rc522_spi {

static const char *const TAG = "rc522_spi";

void RC522Spi::setup() {
  ESP_LOGI(TAG, "SPI Setup");
  this->spi_setup();

  RC522::setup();
}

void RC522Spi::dump_config() {
  RC522::dump_config();
  LOG_PIN("  CS Pin: ", this->cs_);
}

/**
 * Reads a uint8_t from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
uint8_t RC522Spi::pcd_read_register(PcdRegister reg  ///< The register to read from. One of the PCD_Register enums.
) {
  uint8_t value;
  enable();
  transfer_byte(0x80 | reg);
  value = read_byte();
  disable();
  ESP_LOGVV(TAG, "read_register_(%d) -> %d", reg, value);
  return value;
}

/**
 * Reads a number of uint8_ts from the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void RC522Spi::pcd_read_register(PcdRegister reg,  ///< The register to read from. One of the PCD_Register enums.
                                 uint8_t count,    ///< The number of uint8_ts to read
                                 uint8_t *values,  ///< uint8_t array to store the values in.
                                 uint8_t rx_align  ///< Only bit positions rxAlign..7 in values[0] are updated.
) {
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  std::string buf;
  buf = "Rx";
  char cstrb[20];
#endif
  if (count == 0) {
    return;
  }

  // Serial.print(F("Reading "));   Serial.print(count); Serial.println(F(" uint8_ts from register."));
  uint8_t address = 0x80 | reg;  // MSB == 1 is for reading. LSB is not used in address. Datasheet section 8.1.2.3.
  uint8_t index = 0;             // Index in values array.
  enable();
  count--;  // One read is performed outside of the loop

  write_byte(address);  // Tell MFRC522 which address we want to read
  if (rx_align) {       // Only update bit positions rxAlign..7 in values[0]
    // Create bit mask for bit positions rxAlign..7
    uint8_t mask = 0xFF << rx_align;
    // Read value and tell that we want to read the same address again.
    uint8_t value = transfer_byte(address);
    // Apply mask to both current value of values[0] and the new data in value.
    values[0] = (values[0] & ~mask) | (value & mask);
    index++;

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    sprintf(cstrb, " %x", values[0]);
    buf.append(cstrb);
#endif
  }
  while (index < count) {
    values[index] = transfer_byte(address);  // Read value and tell that we want to read the same address again.

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    sprintf(cstrb, " %x", values[index]);
    buf.append(cstrb);
#endif

    index++;
  }
  values[index] = transfer_byte(0);  // Read the final uint8_t. Send 0 to stop reading.

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  buf = buf + " ";
  sprintf(cstrb, "%x", values[index]);
  buf.append(cstrb);

  ESP_LOGVV(TAG, "read_register_array_(%x, %d, , %d) -> %s", reg, count, rx_align, buf.c_str());
#endif
  disable();
}

void RC522Spi::pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                  uint8_t value     ///< The value to write.
) {
  enable();
  // MSB == 0 is for writing. LSB is not used in address. Datasheet section 8.1.2.3.
  transfer_byte(reg);
  transfer_byte(value);
  disable();
}

/**
 * Writes a number of uint8_ts to the specified register in the MFRC522 chip.
 * The interface is described in the datasheet section 8.1.2.
 */
void RC522Spi::pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                                  uint8_t count,    ///< The number of uint8_ts to write to the register
                                  uint8_t *values   ///< The values to write. uint8_t array.
) {
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  std::string buf;
  buf = "Tx";
  char cstrb[20];
#endif

  enable();
  transfer_byte(reg);

  for (uint8_t index = 0; index < count; index++) {
    transfer_byte(values[index]);

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    sprintf(cstrb, " %x", values[index]);
    buf.append(cstrb);
#endif
  }
  disable();
  ESP_LOGVV(TAG, "write_register_(%d, %d) -> %s", reg, count, buf.c_str());
}

}  // namespace rc522_spi
}  // namespace esphome
