#pragma once

#include "esphome/core/component.h"
#include "esphome/components/pn532/pn532.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace pn532_spi {

class PN532Spi : public pn532::PN532,
                 public spi::SPIDevice<spi::BIT_ORDER_LSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                       spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;

  void dump_config() override;

 protected:
  /// Write the full command given in data to the PN532
  void pn532_write_command(const std::vector<uint8_t> &data) override;

  /** Read a data frame from the PN532 and return the result as a vector.
   *
   * Note that is_ready needs to be checked first before requesting this method.
   *
   * On failure, an empty vector is returned.
   */
  std::vector<uint8_t> pn532_read_data() override;

  /** Checks if the PN532 has set its ready status flag.
   *
   * Procedure goes as follows:
   * - Host sends command to PN532 "write data"
   * - Wait for readiness (until PN532 has processed command) by polling "read status"/is_ready
   * - Parse ACK/NACK frame with "read data" byte
   *
   * - If data required, wait until device reports readiness again
   * - Then call "read data" and read certain number of bytes (length is given at offset 4 of frame)
   */
  bool is_ready() override;

  bool read_ack() override;
};

}  // namespace pn532_spi
}  // namespace esphome
