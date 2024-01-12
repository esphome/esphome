#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "mbus-frame.h"
#include "mbus-protocol-handler.h"

namespace esphome {
namespace mbus {

class MBus : public uart::UARTDevice, public Component {
 public:
  static const uint8_t PRIMARY_ADDRESS_MAX = 250;
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_secondary_address(uint64_t secondary_address);

  MBus() {
    this->_serialAdapter = new SerialAdapter(this);
    this->_protocol_handler = new MBusProtocolHandler(this, this->_serialAdapter);
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
  uint64_t secondary_address = 0;
  uint8_t primary_address = 0;

  MBusProtocolHandler *_protocol_handler{nullptr};
  SerialAdapter *_serialAdapter{nullptr};

  static void start_scan_primary_addresses(MBus *mbus);
  static void scan_primary_addresses_response_handler(MBusCommand *command, const MBusFrame &response);

  static void start_scan_secondary_addresses(MBus *mbus);
  static void scan_secondary_addresses_response_handler(MBusCommand *command, const MBusFrame &response);
};

}  // namespace mbus
}  // namespace esphome
