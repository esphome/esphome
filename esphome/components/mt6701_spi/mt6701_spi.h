#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/components/mt6701/mt6701.h"
#include "esphome/components/spi/spi.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

namespace esphome::mt6701_spi {

/// MT6701 driver over the SSI (SPI-compatible) interface.
///
/// SSI is read-only: it cannot write configuration registers, but it does
/// report the magnetic-field status, push button and track-loss flags that the
/// I2C interface does not expose. Each 24-bit frame is CRC-checked.
class MT6701SPIComponent final : public mt6701::MT6701Component,
                                 public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                                       spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(push_button)
  SUB_BINARY_SENSOR(track_loss)
#endif
#ifdef USE_TEXT_SENSOR
  SUB_TEXT_SENSOR(field_status)
#endif

 public:
  void setup() override;
  void dump_config() override;

 protected:
  bool read_count(uint16_t &count) override;

  // Last raw 4-bit status nibble published; 0xFF means "nothing published yet".
  uint8_t last_status_{0xFF};
  // Entity publishing from read_count() is held back until the setup probe has
  // confirmed the encoder is present.
  bool setup_complete_{false};
};

}  // namespace esphome::mt6701_spi
