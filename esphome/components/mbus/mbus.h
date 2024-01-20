#pragma once

#include "esphome/components/mbus/mbus_frame.h"
#include "esphome/components/mbus/mbus_protocol_handler.h"
#include "esphome/components/mbus/mbus_sensor_base.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace mbus {

class MBus : public uart::UARTDevice, public Component {
 public:
  static const uint8_t PRIMARY_ADDRESS_MAX = 250;
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_secondary_address(uint64_t secondary_address) { this->secondary_address_ = secondary_address; };
  void set_interval(uint16_t interval) { this->interval_ = interval; };
  void add_sensor(MBusSensorBase *sensor) { this->sensors_.push_back(sensor); }

  MBus() {
    this->serialAdapter_ = new SerialAdapter(this);
    this->protocol_handler_ = new MBusProtocolHandler(this, this->serialAdapter_);
  }

  ~MBus() {
    if (this->serialAdapter_ != nullptr) {
      delete this->serialAdapter_;
      this->serialAdapter_ = nullptr;
    }
    if (this->protocol_handler_ != nullptr) {
      delete this->protocol_handler_;
      this->protocol_handler_ = nullptr;
    }
  }

 protected:
  uint64_t secondary_address_{0};
  uint8_t primary_address_{0};
  uint16_t interval_{1};

  MBusProtocolHandler *protocol_handler_{nullptr};
  SerialAdapter *serialAdapter_{nullptr};
  std::vector<MBusSensorBase *> sensors_;

  static void start_scan_primary_addresses(MBus *mbus);
  static void scan_primary_addresses_response_handler(MBusCommand *command, const MBusFrame &response);

  static void start_scan_secondary_addresses(MBus *mbus);
  static void scan_secondary_addresses_response_handler(MBusCommand *command, const MBusFrame &response);

  static void start_reading_data(MBus *mbus);
  static void reading_data_response_handler(MBusCommand *command, const MBusFrame &response);
};

}  // namespace mbus
}  // namespace esphome
