#pragma once
#include "ble.h"
#include "device.h"

#include <array>

namespace esphome {
namespace bthome {

template<size_t NUM_DEVICES> class DeviceListener : public IBTHomeListener {
 public:
  void set_device(size_t index, DeviceBase *device) { this->devices_[index] = device; }

  bool on_bthome_data(MacAddressPtr source, const uint8_t *data, size_t size) override {
    for (DeviceBase *d : this->devices_) {
      if (d == nullptr)
        continue;
      if (d->parse_data(source, data, size))
        return true;
    }
    return false;
  }

 protected:
  std::array<DeviceBase *, NUM_DEVICES> devices_{};
};

}  // namespace bthome
}  // namespace esphome
