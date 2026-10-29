// Platform-neutral BLE advertisement triggers: ESPBTDeviceListener subclasses
// registered on a BLEHub, exposed by each tracker under its own automation
// names. parse_device()'s return feeds the "Found device" suppression.
// Constructors are templated on the hub type so this header also builds with
// no tracker present (host unit tests).

#pragma once

#include "ble_device.h"

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome::ble_device_base {

/// True if `address` is in `table`, a list of MACs ended by 0.
inline bool mac_in_table(const uint64_t *table, uint64_t address) {
  for (; *table != 0; table++) {
    if (*table == address)
      return true;
  }
  return false;
}

// on_ble_advertise: fires on every BLE advertisement, optionally filtered to one or more MACs.
class ESPBTAdvertiseTrigger final : public Trigger<const ESPBTDevice &>, public ESPBTDeviceListener {
 public:
  template<typename Hub> explicit ESPBTAdvertiseTrigger(Hub *parent) { parent->register_listener(this); }

  /// Table of MACs ended by 0; must outlive the trigger.
  void set_addresses(const uint64_t *addresses) { this->addresses_ = addresses; }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->addresses_ != nullptr && !mac_in_table(this->addresses_, device.address_uint64()))
      return false;
    this->trigger(device);
    return true;
  }

 protected:
  const uint64_t *addresses_{nullptr};
};

/// Filter of one service or manufacturer data trigger; codegen emits one constexpr per distinct filter.
struct AdvDataMatch {
  uint64_t address;  // MAC, checked only when has_address is set
  ESPBTUUID uuid;
  bool manufacturer;  // match manufacturer data instead of service data
  bool has_address;
};

// on_ble_service_data_advertise / on_ble_manufacturer_data_advertise: fires with the data of the
// first entry whose UUID matches, optionally only for one MAC.
class BLEAdvertiseDataTrigger final : public Trigger<const adv_data_t &>, public ESPBTDeviceListener {
 public:
  template<typename Hub> BLEAdvertiseDataTrigger(Hub *parent, const AdvDataMatch *match) : match_(match) {
    parent->register_listener(this);
  }

  bool parse_device(const ESPBTDevice &device) override {
    const AdvDataMatch &match = *this->match_;
    if (match.has_address && device.address_uint64() != match.address)
      return false;
    const adv_data_t *data =
        find_adv_data(match.manufacturer ? device.get_manufacturer_datas() : device.get_service_datas(), match.uuid);
    if (data == nullptr)
      return false;
    this->trigger(*data);
    return true;
  }

 protected:
  const AdvDataMatch *match_;
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
