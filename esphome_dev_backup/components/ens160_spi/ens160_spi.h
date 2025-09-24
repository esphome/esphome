#pragma once

#include "esphome/components/ens160_base/ens160_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace ens160_spi {

class ENS160SPIComponent : public esphome::ens160_base::ENS160Component,
                           public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                 spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_200KHZ> {
  void setup() override;
  void dump_config() override;

  bool read_byte(uint8_t a_register, uint8_t *data) override;
  bool write_byte(uint8_t a_register, uint8_t data) override;
  bool read_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
  bool write_bytes(uint8_t a_register, uint8_t *data, size_t len) override;
};

}  // namespace ens160_spi
}  // namespace esphome
