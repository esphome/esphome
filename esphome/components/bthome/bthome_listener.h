#pragma once
#include "bthome_decoder.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/core/optional.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"

#include <cstdint>
#include <cstddef>
#include <array>
#include <span>

#ifdef USE_BTHOME_DECRYPTION
#include <algorithm>
#include <initializer_list>
#endif

namespace esphome {
namespace bthome {

static const char *TAG = "bthome";

using EncryptionKey = std::array<uint8_t, 16>;

struct BTHomeHeader {
  uint8_t encrypted : 1;      // bit 0: encrypted data
  uint8_t : 1;                // bit 1: reserved
  uint8_t trigger_based : 1;  // bit 2: irregular advertisement interval
  uint8_t : 2;                // bits 3-4: reserved
  uint8_t version : 3;        // bits 5-7: BTHome version (currently 1 or 2)
};
static_assert(sizeof(BTHomeHeader) == 1, "BTHomeHeader must be 1 byte");

class MacAddress {
 public:
  MacAddress() = default;
  MacAddress(const uint8_t *addr);
  MacAddress(uint64_t addr);

  MacAddress &operator=(const uint8_t *addr);

  operator const uint8_t *() const;

  bool operator==(const uint8_t *other) const;
  bool operator==(const MacAddress &other) const;
  bool operator!=(const uint8_t *other) const;
  bool operator!=(const MacAddress &other) const;

  const char *c_str() const;

 protected:
  uint8_t addr_[MAC_ADDRESS_SIZE]{};
};

class MacAddressPtr {
 public:
  MacAddressPtr() = default;
  MacAddressPtr(const uint8_t *addr) : addr_(addr) {}

  operator const uint8_t *() const { return this->addr_; }

  bool operator==(const uint8_t *other) const;
  bool operator==(const MacAddressPtr &other) const;
  bool operator!=(const uint8_t *other) const;
  bool operator!=(const MacAddressPtr &other) const;

  const char *c_str() const;

 protected:
  const uint8_t *addr_{nullptr};
};

class BTHomeObjectHandler {
 public:
  void set_object_type(BTHomeObjectType object_type) { this->object_type_ = object_type; }
  virtual bool process_object(const BTHomeObject &object) = 0;

 protected:
  BTHomeObjectType object_type_{};
};

class BTHomeSensor : public BTHomeObjectHandler, public esphome::sensor::Sensor, public Component {
 public:
  bool process_object(const BTHomeObject &object) override;
};

class DeviceBase {
 public:
  void set_address(const MacAddress &address) { this->address_ = address; }
#ifdef USE_BTHOME_DECRYPTION
  void set_encryption_key(std::initializer_list<uint8_t> key) {
    EncryptionKey k{};
    std::copy(key.begin(), key.end(), k.begin());
    this->encryption_key = k;
  }
#endif
  virtual void set_handler(size_t index, BTHomeObjectHandler *handler) = 0;
  bool parse_data(MacAddressPtr source_address, const uint8_t *data, size_t data_size);

 protected:
  virtual std::span<BTHomeObjectHandler *> get_handlers() = 0;

  MacAddress address_;
  optional<uint8_t> last_packet_id_{};
#ifdef USE_BTHOME_DECRYPTION
  optional<EncryptionKey> encryption_key;
#endif
};

template<size_t NUM_SENSORS> class Device : public DeviceBase {
  void set_handler(size_t index, BTHomeObjectHandler *handler) override { handlers_[index] = handler; }
  std::span<BTHomeObjectHandler *> get_handlers() override { return handlers_; }

 private:
  std::array<BTHomeObjectHandler *, NUM_SENSORS> handlers_{};
};

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
