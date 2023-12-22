#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "mbus-frame.h"

namespace esphome {
namespace mbus {

#define MBUS_RX_TIMEOUT 100

class MBus : public uart::UARTDevice, public Component {
 public:
  MBus() = default;

  void setup() override;

  void loop() override;

  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void scan_primary_addresses();
  uint8_t scan_primary_address(uint8_t primary_address);

  uint8_t send(MBusFrame &frame);
  std::vector<uint8_t> rx_buffer;
  bool _waiting_for_response{false};
  uint32_t last_send_time{0};
  uint32_t start_time{0};
};

}  // namespace mbus
}  // namespace esphome
