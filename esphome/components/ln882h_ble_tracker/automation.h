// Automation triggers and actions for ln882h_ble_tracker: triggers follow the
// esp32_ble_tracker design; only the scan-control actions are
// platform-specific.

#pragma once

#ifdef USE_LIBRETINY

#include "ln882h_ble_tracker.h"

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include <algorithm>
#include <initializer_list>

namespace esphome::ln882h_ble_tracker {

template<typename... Ts> class StartScanAction final : public Action<Ts...>, public Parented<LN882HBLETracker> {
 public:
  TEMPLATABLE_VALUE(bool, continuous)
  void play(const Ts &...x) override {
    // With continuous: set, the action wins. Without it, the configured value
    // is used - stop_scan() clears the runtime flag permanently, so a bare
    // stop_scan/start_scan pair would otherwise never resume continuous mode.
    const bool want =
        this->continuous_.has_value() ? this->continuous_.value(x...) : this->parent_->configured_continuous();
    if (this->parent_->scan_running()) {
      // Same mode on a running scan is a no-op (esp32 parity): re-anchoring
      // the duration window here would let a repeated action keep a one-shot
      // scan alive forever. A real mode switch re-anchors so a change to
      // one-shot runs a full duration from now.
      if (want != this->parent_->scan_continuous()) {
        this->parent_->set_scan_continuous_runtime(want);
        this->parent_->restart_scan_window();
      }
      return;
    }
    this->parent_->set_scan_continuous_runtime(want);
    this->parent_->start_scan();
  }
};

template<typename... Ts> class StopScanAction final : public Action<Ts...>, public Parented<LN882HBLETracker> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_scan(); }
};

// ---------------------------------------------------------------------------
// Automation triggers.
//
// Each trigger is a ble_device_base::ESPBTDeviceListener registered on the hub —
// the same design as esp32_ble_tracker, where the triggers sit in the listener
// list and their parse_device() return feeds the "Found device" suppression.
// ---------------------------------------------------------------------------

// on_ble_advertise: fires on every BLE advertisement, optionally filtered to one or more MACs.
class ESPBTAdvertiseTrigger final : public Trigger<const ble_device_base::ESPBTDevice &>,
                                    public ble_device_base::ESPBTDeviceListener {
 public:
  explicit ESPBTAdvertiseTrigger(LN882HBLETracker *parent) { parent->register_listener(this); }

  void set_addresses(std::initializer_list<uint64_t> addresses) { this->addresses_ = addresses; }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
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
class BLEServiceDataAdvertiseTrigger final : public Trigger<const ble_device_base::adv_data_t &>,
                                             public ble_device_base::ESPBTDeviceListener {
 public:
  explicit BLEServiceDataAdvertiseTrigger(LN882HBLETracker *parent) { parent->register_listener(this); }

  void set_service_uuid16(uint64_t uuid) {
    this->uuid_ = ble_device_base::ESPBTUUID::from_uint16(static_cast<uint16_t>(uuid));
  }
  void set_service_uuid32(uint64_t uuid) {
    this->uuid_ = ble_device_base::ESPBTUUID::from_uint32(static_cast<uint32_t>(uuid));
  }
  void set_service_uuid128(const uint8_t *uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_raw(uuid); }

  void set_address(uint64_t address) {
    this->address_ = address;
    this->has_address_ = true;
  }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
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
  ble_device_base::ESPBTUUID uuid_{};
  uint64_t address_{0};
  bool has_address_{false};
};

// on_ble_manufacturer_data_advertise: fires when an advertisement contains
// manufacturer data for the given ID. Optional single-MAC filter.
class BLEManufacturerDataAdvertiseTrigger final : public Trigger<const ble_device_base::adv_data_t &>,
                                                  public ble_device_base::ESPBTDeviceListener {
 public:
  explicit BLEManufacturerDataAdvertiseTrigger(LN882HBLETracker *parent) { parent->register_listener(this); }

  void set_manufacturer_uuid16(uint64_t uuid) {
    this->uuid_ = ble_device_base::ESPBTUUID::from_uint16(static_cast<uint16_t>(uuid));
  }
  void set_manufacturer_uuid32(uint64_t uuid) {
    this->uuid_ = ble_device_base::ESPBTUUID::from_uint32(static_cast<uint32_t>(uuid));
  }
  void set_manufacturer_uuid128(const uint8_t *uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_raw(uuid); }

  void set_address(uint64_t address) {
    this->address_ = address;
    this->has_address_ = true;
  }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override {
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
  ble_device_base::ESPBTUUID uuid_{};
  uint64_t address_{0};
  bool has_address_{false};
};

// on_scan_end: fires whenever a scan period ends (duration elapsed or stop_scan
// called). A listener whose on_scan_end() hook fires the trigger — never claims
// devices (parse_device always returns false).
class BLEEndOfScanTrigger final : public Trigger<>, public ble_device_base::ESPBTDeviceListener {
 public:
  explicit BLEEndOfScanTrigger(LN882HBLETracker *parent) { parent->register_listener(this); }

  bool parse_device(const ble_device_base::ESPBTDevice &device) override { return false; }
  void on_scan_end() override { this->trigger(); }
};

}  // namespace esphome::ln882h_ble_tracker

#endif  // USE_LIBRETINY
