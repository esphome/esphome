#pragma once
#include "bthome.h"
#include "remote_device.h"
#include "esphome/components/ble_device_base/ble_device.h"

#include <array>

namespace esphome::bthome::client {

template<size_t NUM_DEVICES> class DeviceListener : public ble_device_base::ESPBTDeviceListener {
 public:
  void set_device(size_t index, RemoteDeviceBase *device) { this->devices_[index] = device; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
    bool matched = false;
    for (const auto &service_data : device.get_service_datas()) {
      if (!service_data.uuid.contains(BTHOME_SVC_UUID_LOW, BTHOME_SVC_UUID_HIGH))
        continue;

      const uint8_t *data = service_data.data.data();
      const size_t data_size = service_data.data.size();
      if (data_size == 0)
        continue;

      const BTHomeHeader header{data[0]};
      if (header.version() != BTHOME_VERSION_2)
        continue;

      if (this->on_bthome_data(device.address(), data, data_size))
        matched = true;
    }
    return matched;
  }

 private:
  bool on_bthome_data(MacAddressPtr source, const uint8_t *data, size_t size) {
    for (RemoteDeviceBase *d : this->devices_) {
      if (d == nullptr)
        continue;
      if (d->parse_data(source, data, size))
        return true;
    }
    return false;
  }

  std::array<RemoteDeviceBase *, NUM_DEVICES> devices_{};
};

}  // namespace esphome::bthome::client
