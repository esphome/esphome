#pragma once
#include "remote_object.h"
#include "bthome.h"
#include "encryption.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/core/optional.h"
#include "esphome/core/defines.h"

#include <array>
#include <cstddef>
#include <span>

#ifdef USE_BTHOME_DECRYPTION
#include <algorithm>
#include <initializer_list>
#endif

namespace esphome::bthome::client {

class RemoteDeviceBase {
 public:
  void set_address(uint64_t address) { this->address_ = address; }
#ifdef USE_BTHOME_DECRYPTION
  void set_encryption_key(std::initializer_list<uint8_t> key) {
    if (key.size() != EncryptionKey{}.size())
      return;
    EncryptionKey k{};
    std::copy(key.begin(), key.end(), k.begin());
    this->encryption_key_ = k;
  }
#endif
  virtual void set_handler(size_t index, BTHomeRemoteObject *handler) = 0;
  bool parse_data(const ble_device_base::ESPBTDevice &device, const uint8_t *data, size_t data_size);
  void dump_config();

 protected:
  virtual std::span<BTHomeRemoteObject *> get_handlers() = 0;

  uint64_t address_{0};
  // Payloads seen under the current packet ID, as hashes, for duplicate filtering.
  static constexpr size_t SEEN_PAYLOAD_SLOTS = 4;
  optional<uint8_t> last_packet_id_{};
  std::array<uint32_t, SEEN_PAYLOAD_SLOTS> seen_payload_hashes_{};
  uint8_t seen_payload_count_{0};
  uint8_t next_seen_payload_{0};
  // Set once a frame with an unexpected encryption state has been logged, so it is not logged every time.
  bool encryption_mismatch_logged_{false};
#ifdef USE_BTHOME_DECRYPTION
  optional<EncryptionKey> encryption_key_;
#endif

 private:
  bool is_duplicate_(uint8_t packet_id, const uint8_t *payload, size_t payload_size);
  bool matches_(const ble_device_base::ESPBTDevice &device, const uint8_t *data, size_t data_size) const;
  size_t process_object_(std::span<BTHomeRemoteObject *> handlers, size_t index, const BTHomeObject &object);
};

// Logs the configuration of every remote device; called from the listener's dump_config.
void dump_remote_devices(std::span<RemoteDeviceBase *const> devices);

template<size_t NUM_SENSORS> class RemoteDevice : public RemoteDeviceBase {
 public:
  void set_handler(size_t index, BTHomeRemoteObject *handler) override { handlers_[index] = handler; }

 protected:
  std::span<BTHomeRemoteObject *> get_handlers() override { return handlers_; }

 private:
  std::array<BTHomeRemoteObject *, NUM_SENSORS> handlers_{};
};
}  // namespace esphome::bthome::client
