#pragma once

#include <array>
#include <vector>

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome::remote_base {

class PDPioneerData {
 public:
  static constexpr uint8_t FRAME_SIZE = 14;
  static constexpr uint8_t OFFSET_CS = 13;

  PDPioneerData() = default;
  PDPioneerData(std::initializer_list<uint8_t> data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  PDPioneerData(const std::vector<uint8_t> &data) {
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  PDPioneerData(const uint8_t *data, size_t len) {
    progmem_memcpy(this->data_.data(), data, std::min(len, this->data_.size()));
  }

  uint8_t *data() { return this->data_.data(); }
  const uint8_t *data() const { return this->data_.data(); }

  // Position 3 tells the two bursts of a command apart; the odd burst's checksum carries an extra 15
  bool is_odd_burst() const { return this->data_[3] == 0x02; }
  bool is_even_burst() const { return this->data_[3] == 0x01; }

  bool is_valid() const {
    return (this->is_odd_burst() || this->is_even_burst()) && this->data_[OFFSET_CS] == this->checksum_();
  }

  void finalize() { this->data_[OFFSET_CS] = this->checksum_(); }

  static constexpr size_t TO_STR_BUFFER_SIZE = format_hex_pretty_size(FRAME_SIZE);
  const char *to_str(char *buffer) const {
    format_hex_pretty_to(buffer, TO_STR_BUFFER_SIZE, this->data_.data(), this->data_.size(), '.');
    return buffer;
  }

  bool operator==(const PDPioneerData &rhs) const { return this->data_ == rhs.data_; }

  uint8_t &operator[](size_t idx) { return this->data_[idx]; }
  const uint8_t &operator[](size_t idx) const { return this->data_[idx]; }

 protected:
  uint8_t checksum_() const { return sum_bytes(this->data_.data(), OFFSET_CS, this->is_odd_burst() ? 15 : 0); }

  std::array<uint8_t, FRAME_SIZE> data_{};
};

class PDPioneerProtocol : public RemoteProtocol<PDPioneerData> {
 public:
  void encode(RemoteTransmitData *dst, const PDPioneerData &src);
  optional<PDPioneerData> decode(RemoteReceiveData src);
  void dump(const PDPioneerData &data);
};

DECLARE_REMOTE_PROTOCOL(PDPioneer)

}  // namespace esphome::remote_base
