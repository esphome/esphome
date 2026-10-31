#pragma once
#include "remote_object.h"
#include "bthome.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/core/optional.h"
#include "esphome/core/defines.h"

#include <array>
#include <cstddef>
#include <span>

namespace esphome::bthome::client {

class RemoteDeviceBase {
 public:
  void set_address(uint64_t address) { this->address_ = address; }
  virtual void set_handler(size_t index, BTHomeRemoteObject *handler) = 0;
  bool parse_data(const ble_device_base::ESPBTDevice &device, const uint8_t *data, size_t data_size);
  void dump_config();

 protected:
  virtual std::span<BTHomeRemoteObject *> get_handlers() = 0;

  // Before address_, so on 32-bit targets it fills the padding after the vtable pointer.
  uint32_t last_frame_ms_{0};
  uint64_t address_{0};
  // Payloads seen under the current packet ID, as hashes, for duplicate filtering.
  static constexpr size_t SEEN_PAYLOAD_SLOTS = 4;
  // After this much silence from a device a seen payload counts as new again.
  static constexpr uint32_t PACKET_ID_RESET_MS = 4000;
  // Without a packet ID, only a copy of the previous frame this close to it is a repeat.
  static constexpr uint32_t NO_PACKET_ID_REPEAT_MS = 1200;
  optional<uint8_t> last_packet_id_{};
  std::array<uint32_t, SEEN_PAYLOAD_SLOTS> seen_payload_hashes_{};
  uint8_t seen_payload_count_{0};
  uint8_t next_seen_payload_{0};
  // Set once a frame with an unexpected encryption state has been logged, so it is not logged every time.
  bool encryption_mismatch_logged_{false};

 private:
  bool is_duplicate_(optional<uint8_t> packet_id, const uint8_t *payload, size_t payload_size, uint32_t now);
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
