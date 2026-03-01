#pragma once
#include <cstddef>
#include <cstdint>
#include "helpers.h"

namespace esphome {
namespace bthome {

static constexpr size_t BLE_FLAGS_SIZE = 3;       // [02 01 06]
static constexpr size_t BLE_SVC_HEADER_SIZE = 4;  // [LL 16 D2 FC]
static constexpr size_t BLE_ADV_MAX_SIZE = 31;

static constexpr uint8_t BLE_AD_TYPE_FLAGS = 0x01;        // AD type: Flags
static constexpr uint8_t BLE_AD_FLAGS_VALUE = 0x06;       // General Discoverable + BR/EDR Not Supported
static constexpr uint8_t BLE_AD_TYPE_SVC_DATA_16 = 0x16;  // AD type: Service Data – 16-bit UUID

// Handler interface for advertisement state changes (e.g. to trigger a new advertisement)
class IBLEAdvHandler {
 public:
  virtual ~IBLEAdvHandler() = default;
  virtual void on_advertise(bool active) = 0;
};

// Abstract adapter interface for all Advertising BLE operations — platform agnostic
class IBLEAdvertiser {
 public:
  virtual ~IBLEAdvertiser() = default;

  virtual void setup(IBLEAdvHandler *) = 0;

  // Returns local BLE MAC address (6 bytes), may be nullptr
  virtual MacAddressPtr get_local_mac() = 0;

  // Pushes raw advertisement data to the BLE controller.
  virtual void config_adv_data_raw(const uint8_t *data, size_t len) = 0;
};

// Handler interface for incoming BTHome data — implemented by DeviceListener
class IBTHomeListener {
 public:
  virtual ~IBTHomeListener() = default;
  // Called with already-validated BTHome data (header byte + payload). Returns true if handled.
  virtual bool on_bthome_data(MacAddressPtr source, const uint8_t *data, size_t size) = 0;
};

// Abstract adapter interface for scanning BLE operations — platform agnostic
class IBLEListener {
 public:
  virtual ~IBLEListener() = default;
  virtual void setup(IBTHomeListener *) = 0;
};

}  // namespace bthome
}  // namespace esphome
