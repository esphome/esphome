#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace mcp3008 {

class MCP3008Sensor;

class MCP3008 : public Component,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_1MHZ> {  // At 3.3V 2MHz is too fast 1.35MHz is about right
 public:
  MCP3008() = default;

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  float read_data_(uint8_t pin);

  friend class MCP3008Sensor;
};

class MCP3008Sensor : public PollingComponent, public sensor::Sensor {
 public:
  MCP3008Sensor(MCP3008 *parent, std::string name, uint8_t pin);

  void setup() override;
  void update() override;

 protected:
  MCP3008 *parent_;
  uint8_t pin_;
};

}  // namespace mcp3008
}  // namespace esphome
