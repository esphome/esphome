#pragma once
#include "bthome_device.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <array>

namespace esphome {
namespace bthome {

static const char *TAG = "bthome";

template<size_t NUM_DEVICES> class DeviceListener : public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_device(size_t index, DeviceBase *device) { devices_[index] = device; }

 protected:
  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    bool matched = false;
    for (auto &service_data : device.get_service_datas()) {
      if (!service_data.uuid.contains(0xD2, 0xFC)) {
        ESP_LOGD(TAG, "not bthome service data");
        continue;
      }

      const uint8_t *data = service_data.data.data();
      size_t data_size = service_data.data.size();
      MacAddressPtr source_address = device.address();

      ESP_LOGD(TAG, "bthome service data!!, length=%zu, from %s", data_size, source_address.c_str());

      if (data_size < 2) {
        ESP_LOGVV(TAG, "BTHome data too short: %zu", data_size);
        return false;
      }

      BTHomeHeader &header = *(BTHomeHeader *) &data[0];

      if (header.version != 0x02) {
        ESP_LOGVV(TAG, "Unsupported BTHome version %u", header.version);
        return false;
      }

      for (DeviceBase *d : this->devices_) {
        if (d->parse_data(source_address, data, data_size)) {
          matched = true;
          break;
        }
      }
    }
    return matched;
  }

  std::array<DeviceBase *, NUM_DEVICES> devices_{};
};

}  // namespace bthome
}  // namespace esphome
