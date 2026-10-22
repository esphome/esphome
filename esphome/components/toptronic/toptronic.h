#pragma once

#ifdef USE_ESP32

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <unordered_map>
#include <vector>

namespace esphome::toptronic {

class TopTronic;

uint32_t build_can_id(uint16_t sender_id, uint16_t receiver_mask);
std::vector<uint8_t> build_get_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint);
std::vector<uint8_t> build_set_request(uint8_t function_group, uint8_t function_number, uint32_t datapoint,
                                       const std::vector<uint8_t> &value);

class TopTronic : public Component {
 public:
  explicit TopTronic(canbus::Canbus *canbus);

  void set_device_type(uint16_t device_type) { this->device_type_ = device_type; }
  void set_device_addr(uint8_t device_addr) { this->device_addr_ = device_addr; }
  uint16_t get_device_id() const { return this->device_type_ | this->device_addr_; }

  void setup() override;
  void dump_config() override;
  void parse_frame(const std::vector<uint8_t> &data, uint32_t can_id, bool remote_transmission_request);

 protected:
  struct PendingMessage {
    std::vector<uint8_t> data;
    uint8_t remaining_frames{0};
    uint32_t last_update_ms{0};
  };

  void interpret_message_(const uint8_t *data, size_t len, uint32_t can_id);
  // Evicts reassembly buffers whose continuation frames never arrived.
  void expire_pending_();

  canbus::Canbus *canbus_;
  std::unordered_map<uint32_t, PendingMessage> pending_messages_;
  uint16_t device_type_{0};
  uint8_t device_addr_{0};
};

}  // namespace esphome::toptronic

#endif  // USE_ESP32
