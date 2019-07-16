#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mcp2515 {
  static const char *TAG = "mcp2515";
class MCP2515 : public canbus::Canbus,
                public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_LEADING,
                                      spi::DATA_RATE_8MHZ> {
 public:
  MCP2515() {};
  MCP2515(const std::string &name){};
  void send(int can_id, uint8_t *data) { ESP_LOGD(TAG, "send: id=%d,  data=%d",can_id,data[0]); };

  void set_cs_pin(GPIOPin *cs_pin) { cs_pin_ = cs_pin; }

 protected:
  GPIOPin *cs_pin_;
};
}  // namespace mcp2515
}  // namespace esphome