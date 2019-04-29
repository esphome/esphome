#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ssd1306_base/ssd1306_base.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace ssd1306_spi {

class SPISSD1306 : public ssd1306_base::SSD1306, public spi::SPIDevice {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { dc_pin_ = dc_pin; }

  void setup() override;

  void dump_config() override;

 protected:
  void command(uint8_t value) override;

  void write_display_data() override;
  bool is_device_msb_first() override;
  bool is_device_high_speed() override;

  GPIOPin *dc_pin_;
};

}  // namespace ssd1306_spi
}  // namespace esphome
