#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "mbus-frame.h"
#include "mbus-protocol-handler.h"

namespace esphome {
namespace mbus {

class MBus : public uart::UARTDevice, public Component {
 public:
  static const uint8_t PRIMARY_ADDRESS_MAX = 1;
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  MBus() {
    this->_serialAdapter = new SerialAdapter(this);
    this->_protocol_handler = new MBusProtocolHandler(this->_serialAdapter);
  }
  ~MBus() {
    if (this->_serialAdapter != nullptr) {
      delete this->_serialAdapter;
      this->_serialAdapter = nullptr;
    }
    if (this->_protocol_handler != nullptr) {
      delete this->_protocol_handler;
      this->_protocol_handler = nullptr;
    }
  }

 protected:
  void scan_primary_addresses();
  static void scan_primary_addresses_response_handler(MBusCommand *command, const MBusFrame &response);
  // int8_t scan_primary_address(uint8_t primary_address);
  // int8_t init_slaves();
  // int8_t scan_slaves();
  // void scan_secondary_adresses();

  MBusProtocolHandler *_protocol_handler{nullptr};
  SerialAdapter *_serialAdapter{nullptr};
};

}  // namespace mbus
}  // namespace esphome
