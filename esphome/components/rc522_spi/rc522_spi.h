/**
 * Library based on https://github.com/miguelbalboa/rfid
 * and adapted to ESPHome by @glmnet
 *
 * original authors Dr.Leong, Miguel Balboa, Søren Thing Andersen, Tom Clement, many more! See GitLog.
 *
 *
 */

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/rc522/rc522.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace rc522_spi {

class RC522Spi : public rc522::RC522,
                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;

  void dump_config() override;

 protected:
  uint8_t pcd_read_register(PcdRegister reg  ///< The register to read from. One of the PCD_Register enums.
                            ) override;

  /**
   * Reads a number of uint8_ts from the specified register in the MFRC522 chip.
   * The interface is described in the datasheet section 8.1.2.
   */
  void pcd_read_register(PcdRegister reg,  ///< The register to read from. One of the PCD_Register enums.
                         uint8_t count,    ///< The number of uint8_ts to read
                         uint8_t *values,  ///< uint8_t array to store the values in.
                         uint8_t rx_align  ///< Only bit positions rxAlign..7 in values[0] are updated.
                         ) override;
  void pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                          uint8_t value     ///< The value to write.
                          ) override;

  /**
   * Writes a number of uint8_ts to the specified register in the MFRC522 chip.
   * The interface is described in the datasheet section 8.1.2.
   */
  void pcd_write_register(PcdRegister reg,  ///< The register to write to. One of the PCD_Register enums.
                          uint8_t count,    ///< The number of uint8_ts to write to the register
                          uint8_t *values   ///< The values to write. uint8_t array.
                          ) override;
};

}  // namespace rc522_spi
}  // namespace esphome
