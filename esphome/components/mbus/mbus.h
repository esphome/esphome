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

  MBus() : serial_adapter_(SerialAdapter(this)), protocol_handler_(MBusProtocolHandler(this, &this->serial_adapter_)) {}

 protected:
  uint64_t secondary_address_{0};
  uint8_t primary_address_{0};
  uint16_t interval_{1};

  SerialAdapter serial_adapter_;
  MBusProtocolHandler protocol_handler_;
  std::vector<MBusSensorBase *> sensors_;

  static void start_scan_primary_addresses(MBus &mbus);
  static void scan_primary_addresses_response_handler(const MBusCommand &command, const MBusFrame &response);

  static void start_scan_secondary_addresses(MBus &mbus);
  static void scan_secondary_addresses_response_handler(const MBusCommand &command, const MBusFrame &response);

  static void start_reading_data(MBus &mbus);
  static void reading_data_response_handler(const MBusCommand &command, const MBusFrame &response);
};

}  // namespace mbus
}  // namespace esphome
