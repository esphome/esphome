// Platform-neutral BLE advertisement triggers: ESPBTDeviceListener subclasses
// registered on a BLEHub, exposed by each tracker under its own automation
// names. parse_device()'s return feeds the "Found device" suppression.
// Constructors are templated on the hub type so this header also builds with
// no tracker present (host unit tests).

#pragma once

#include "ble_device.h"

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <initializer_list>

namespace esphome::ble_device_base {

// on_ble_advertise: fires on every BLE advertisement, optionally filtered to one or more MACs.
class ESPBTAdvertiseTrigger final : public Trigger<const ESPBTDevice &>, public ESPBTDeviceListener {
 public:
  template<typename Hub> explicit ESPBTAdvertiseTrigger(Hub *parent) { parent->register_listener(this); }

  void set_addresses(std::initializer_list<uint64_t> addresses) { this->addresses_ = addresses; }

  bool parse_device(const ESPBTDevice &device) override {
    if (!this->addresses_.empty() && std::find(this->addresses_.begin(), this->addresses_.end(),
                                               device.address_uint64()) == this->addresses_.end()) {
      return false;
    }
    this->trigger(device);
    return true;
  }

 protected:
  FixedVector<uint64_t> addresses_;
};

// on_ble_service_data_advertise: fires when an advertisement contains service
// data for the given UUID. Optional single-MAC filter.
class BLEServiceDataAdvertiseTrigger final : public Trigger<const adv_data_t &>, public ESPBTDeviceListener {
 public:
  template<typename Hub> explicit BLEServiceDataAdvertiseTrigger(Hub *parent) { parent->register_listener(this); }

  void set_service_uuid16(uint64_t uuid) { this->uuid_ = ESPBTUUID::from_uint16(static_cast<uint16_t>(uuid)); }
  void set_service_uuid32(uint64_t uuid) { this->uuid_ = ESPBTUUID::from_uint32(static_cast<uint32_t>(uuid)); }
  void set_service_uuid128(const uint8_t *uuid) { this->uuid_ = ESPBTUUID::from_raw(uuid); }

  void set_address(uint64_t address) {
    this->address_ = address;
    this->has_address_ = true;
  }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->has_address_ && device.address_uint64() != this->address_) {
      return false;
    }
    for (const auto &sd : device.get_service_datas()) {
      if (sd.uuid == this->uuid_) {
        this->trigger(sd.data);
        return true;
      }
    }
    return false;
  }

 protected:
  ESPBTUUID uuid_{};
  uint64_t address_{0};
  bool has_address_{false};
};

// on_ble_manufacturer_data_advertise: fires when an advertisement contains
// manufacturer data for the given ID. Optional single-MAC filter.
class BLEManufacturerDataAdvertiseTrigger final : public Trigger<const adv_data_t &>, public ESPBTDeviceListener {
 public:
  template<typename Hub> explicit BLEManufacturerDataAdvertiseTrigger(Hub *parent) { parent->register_listener(this); }

  void set_manufacturer_uuid16(uint64_t uuid) { this->uuid_ = ESPBTUUID::from_uint16(static_cast<uint16_t>(uuid)); }
  void set_manufacturer_uuid32(uint64_t uuid) { this->uuid_ = ESPBTUUID::from_uint32(static_cast<uint32_t>(uuid)); }
  void set_manufacturer_uuid128(const uint8_t *uuid) { this->uuid_ = ESPBTUUID::from_raw(uuid); }

  void set_address(uint64_t address) {
    this->address_ = address;
    this->has_address_ = true;
  }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->has_address_ && device.address_uint64() != this->address_) {
      return false;
    }
    for (const auto &md : device.get_manufacturer_datas()) {
      if (md.uuid == this->uuid_) {
        this->trigger(md.data);
        return true;
      }
    }
    return false;
  }

 protected:
  ESPBTUUID uuid_{};
  uint64_t address_{0};
  bool has_address_{false};
};

// on_scan_end: fires whenever a scan period ends (duration elapsed or stop
// requested). A listener whose on_scan_end() hook fires the trigger — never
// claims devices (parse_device always returns false).
class BLEEndOfScanTrigger final : public Trigger<>, public ESPBTDeviceListener {
 public:
  template<typename Hub> explicit BLEEndOfScanTrigger(Hub *parent) { parent->register_listener(this); }

  bool parse_device(const ESPBTDevice &device) override { return false; }
  void on_scan_end() override { this->trigger(); }
};

}  // namespace esphome::ble_device_base
