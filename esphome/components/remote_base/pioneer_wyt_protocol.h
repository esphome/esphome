#pragma once

#include <array>
#include <vector>

#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome::remote_base {

class PioneerWytData {
 public:
  static constexpr uint8_t SIZE = 14;
  enum PioneerWytDataType : uint8_t {
    PIONEER_WYT_TYPE_GENERAL = 0x01,
    PIONEER_WYT_TYPE_FAN = 0x02,
  };

  // Make default
  PioneerWytData() {}
  // Make from initializer_list
  PioneerWytData(std::initializer_list<uint8_t> data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  // Make from vector
  PioneerWytData(const std::vector<uint8_t> &data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  PioneerWytData(const uint8_t *data, size_t len) {
    progmem_memcpy(this->data_.data(), data, std::min(len, this->data_.size()));
  }

  uint8_t *data() { return this->data_.data(); }
  const uint8_t *data() const { return this->data_.data(); }
  uint8_t size() const { return this->data_.size(); }
  // FAN frames add 0x0F to the byte sum
  void finalize() {
    this->data_[OFFSET_CS] =
        sum_bytes(this->data_.data(), OFFSET_CS, this->type() == PIONEER_WYT_TYPE_FAN ? 0x0F : 0x00);
  }
  PioneerWytDataType type() const { return static_cast<PioneerWytDataType>(this->data_[3]); }
  uint8_t &operator[](size_t idx) { return this->data_[idx]; }
  const uint8_t &operator[](size_t idx) const { return this->data_[idx]; }

 protected:
  static constexpr uint8_t OFFSET_CS = SIZE - 1;
  std::array<uint8_t, SIZE> data_{};
};

class PioneerWytProtocol : public RemoteProtocol<PioneerWytData> {
 public:
  void encode(RemoteTransmitData *dst, const PioneerWytData &data);
};

}  // namespace esphome::remote_base
