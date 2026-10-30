#pragma once

#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

// No platform #ifdef: ble_device_base provides the BLE types on every platform; this
// component is only compiled when configured (which requires a BLE hub). bindkey (AES-CCM)
// decryption availability is selected per platform in the .cpp.
namespace esphome::bthome_mithermometer {

class BTHomeMiThermometer final : public ble_device_base::ESPBTDeviceListener, public Component {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(battery_level)
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(signal_strength)

 public:
  void set_address(uint64_t address) { this->address_ = address; }
  void set_bindkey(std::initializer_list<uint8_t> bindkey);

  void dump_config() override;
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

 protected:
  bool handle_service_data_(const ble_device_base::ServiceData &service_data,
                            const ble_device_base::ESPBTDevice &device);
  bool decrypt_bthome_payload_(const std::vector<uint8_t> &data, uint64_t source_address,
                               std::vector<uint8_t> &payload) const;

  uint64_t address_{0};
  optional<uint8_t> last_packet_id_{};
  bool has_bindkey_{false};
  uint8_t bindkey_[16];
};

}  // namespace esphome::bthome_mithermometer
