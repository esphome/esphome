#pragma once
#include "bthome_handler.h"
#include "helpers.h"
#include "esphome/core/optional.h"
#include "esphome/core/defines.h"

#include <array>
#include <cstddef>
#include <span>

#ifdef USE_BTHOME_DECRYPTION
#include <algorithm>
#include <initializer_list>
#endif

namespace esphome {
namespace bthome {

static_assert(sizeof(BTHomeHeader) == 1, "BTHomeHeader must be 1 byte");

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
 public:
  void set_handler(size_t index, BTHomeObjectHandler *handler) override { handlers_[index] = handler; }

 protected:
  std::span<BTHomeObjectHandler *> get_handlers() override { return handlers_; }

 private:
  std::array<BTHomeObjectHandler *, NUM_SENSORS> handlers_{};
};

}  // namespace bthome
}  // namespace esphome
