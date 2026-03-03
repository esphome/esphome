#pragma once
#include <array>
#include <cstddef>
#include <functional>
#include <span>

#include "decoder.h"
#include "encoder.h"
#include "local_sensor.h"
#include "helpers.h"
#include "ble.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"

#ifdef USE_BTHOME_ENCRYPTION
#include "encryption.h"
#endif

namespace esphome {
namespace bthome {
namespace server {

// Base class with most implementation (non-template)
class BTHomeServerBase : public Component, public IBLEAdvHandler {
 public:
  explicit BTHomeServerBase(IBLEAdvertiser *ble_adapter);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

#ifdef USE_BTHOME_ENCRYPTION
  void set_encryption_key(std::initializer_list<uint8_t> key) {
    EncryptionKey k{};
    std::copy(key.begin(), key.end(), k.begin());
    this->encryption_key_ = k;
  }
#endif

  // Virtual methods for derived class to manage sensors
  virtual void set_local_sensor(size_t index, BTHomeLocalBase *sensor) = 0;
  virtual std::span<BTHomeLocalBase *> get_local_sensors() = 0;

  void on_advertise(bool active) override;

 protected:
  void send_frame_();
  void advertise_immediate_(BTHomeObjectType type);

  IBLEAdvertiser *ble_adapter_{nullptr};
  size_t next_sensor_index_{0};
  BTHomeEncoder encoder_;
  bool advertising_{false};
  MacAddress local_mac_;
  uint8_t adv_buffer_[BLE_ADV_MAX_SIZE]{};

#ifdef USE_BTHOME_ENCRYPTION
  optional<EncryptionKey> encryption_key_;
  uint32_t encryption_counter_{0};
#endif
};

// Template class — only holds the sensor array
template<size_t N> class BTHomeServer : public BTHomeServerBase {
 public:
  explicit BTHomeServer(IBLEAdvertiser *ble_adapter = nullptr) : BTHomeServerBase(ble_adapter) {}

  void set_local_sensor(size_t index, BTHomeLocalBase *sensor) override { this->local_sensors_[index] = sensor; }

  std::span<BTHomeLocalBase *> get_local_sensors() override {
    return std::span<BTHomeLocalBase *>(this->local_sensors_.data(), N);
  }

 private:
  std::array<BTHomeLocalBase *, N> local_sensors_{};
};

}  // namespace server
}  // namespace bthome
}  // namespace esphome
