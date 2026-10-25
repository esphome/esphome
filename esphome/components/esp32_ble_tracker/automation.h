#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/ble_device_base/automation.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome::esp32_ble_tracker {
#ifdef USE_ESP32_BLE_DEVICE
class ESPBTAdvertiseTrigger final : public Trigger<const ESPBTDevice &>, public ESPBTDeviceListener {
 public:
  explicit ESPBTAdvertiseTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }
  /// Table of MACs ended by 0; must outlive the trigger.
  void set_addresses(const uint64_t *addresses) { this->addresses_ = addresses; }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->addresses_ != nullptr && !ble_device_base::mac_in_table(this->addresses_, device.address_uint64()))
      return false;
    this->trigger(device);
    return true;
  }

 protected:
  const uint64_t *addresses_{nullptr};
};

class BLEServiceDataAdvertiseTrigger final : public Trigger<const adv_data_t &>, public ESPBTDeviceListener {
 public:
  explicit BLEServiceDataAdvertiseTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_service_uuid16(uint16_t uuid) { this->uuid_ = ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->uuid_ = ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->uuid_ = ESPBTUUID::from_raw(uuid); }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->address_ && device.address_uint64() != this->address_) {
      return false;
    }
    for (auto &service_data : device.get_service_datas()) {
      if (service_data.uuid == this->uuid_) {
        this->trigger(service_data.data);
        return true;
      }
    }
    return false;
  }

 protected:
  uint64_t address_ = 0;
  ESPBTUUID uuid_;
};

class BLEManufacturerDataAdvertiseTrigger final : public Trigger<const adv_data_t &>, public ESPBTDeviceListener {
 public:
  explicit BLEManufacturerDataAdvertiseTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_manufacturer_uuid16(uint16_t uuid) { this->uuid_ = ESPBTUUID::from_uint16(uuid); }
  void set_manufacturer_uuid32(uint32_t uuid) { this->uuid_ = ESPBTUUID::from_uint32(uuid); }
  void set_manufacturer_uuid128(uint8_t *uuid) { this->uuid_ = ESPBTUUID::from_raw(uuid); }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->address_ && device.address_uint64() != this->address_) {
      return false;
    }
    for (auto &manufacturer_data : device.get_manufacturer_datas()) {
      if (manufacturer_data.uuid == this->uuid_) {
        this->trigger(manufacturer_data.data);
        return true;
      }
    }
    return false;
  }

 protected:
  uint64_t address_ = 0;
  ESPBTUUID uuid_;
};

#endif  // USE_ESP32_BLE_DEVICE

class BLEEndOfScanTrigger final : public Trigger<>, public ESPBTDeviceListener {
 public:
  explicit BLEEndOfScanTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }

#ifdef USE_ESP32_BLE_DEVICE
  bool parse_device(const ESPBTDevice &device) override { return false; }
#endif
  void on_scan_end() override { this->trigger(); }
};

}  // namespace esphome::esp32_ble_tracker

#endif
