#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"
#include <array>
#include <cinttypes>
#include <utility>
#include <vector>

namespace esphome {
namespace remote_base {

static const uint8_t MAX_DATA_LENGTH = 15;
static const uint8_t DATA_LENGTH_MASK = 0x3f;

/*
Message Format:
  2 bytes:   Sync (0x55FF)
  1 bit:     Retransmission flag (High means retransmission)
  1 bit:     Address length flag (Low means 2 bytes, High means 3 bytes)
  2 bits:    Unknown
  4 bits:    Data length (in bytes)
  1 bit:     Reply flag (High means this is a reply to a previous message with the same message type)
  7 bits:    Message type
  2-3 bytes: Destination address
  2-3 bytes: Source address
  1 byte:    Message ID (randomized, does not change for retransmissions)
  0-? bytes: Data
  1 byte:    Checksum
*/

class ABBWelcomeData {
 public:
  // Make default
  ABBWelcomeData() {
    std::fill(std::begin(this->data_), std::end(this->data_), 0);
    this->data_[0] = 0x55;
    this->data_[1] = 0xff;
  }
  // Make from initializer_list
  ABBWelcomeData(std::initializer_list<uint8_t> data) {
    std::fill(std::begin(this->data_), std::end(this->data_), 0);
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  // Make from vector
  ABBWelcomeData(const std::vector<uint8_t> &data) {
    std::fill(std::begin(this->data_), std::end(this->data_), 0);
    std::copy_n(data.begin(), std::min(data.size(), this->data_.size()), this->data_.begin());
  }
  // Default copy constructor
  ABBWelcomeData(const ABBWelcomeData &) = default;

  bool auto_message_id{false};

  uint8_t *data() { return this->data_.data(); }
  const uint8_t *data() const { return this->data_.data(); }
  uint8_t size() const {
    return std::min(static_cast<uint8_t>(6 + (2 * this->get_address_length()) + (this->data_[2] & DATA_LENGTH_MASK)),
                    static_cast<uint8_t>(this->data_.size()));
  }
  bool is_valid() const {
    return this->data_[0] == 0x55 && this->data_[1] == 0xff &&
           ((this->data_[2] & DATA_LENGTH_MASK) <= MAX_DATA_LENGTH) &&
           (this->data_[this->size() - 1] == this->calc_cs_());
  }
  void set_retransmission(bool retransmission) {
    if (retransmission) {
      this->data_[2] |= 0x80;
    } else {
      this->data_[2] &= 0x7f;
    }
  }
  bool get_retransmission() const { return this->data_[2] & 0x80; }
  // set_three_byte_address must be called before set_source_address, set_destination_address, set_message_id and
  // set_data!
  void set_three_byte_address(bool three_byte_address) {
    if (three_byte_address) {
      this->data_[2] |= 0x40;
    } else {
      this->data_[2] &= 0xbf;
    }
  }
  uint8_t get_three_byte_address() const { return (this->data_[2] & 0x40); }
  uint8_t get_address_length() const { return this->get_three_byte_address() ? 3 : 2; }
  void set_message_type(uint8_t message_type) { this->data_[3] = message_type; }
  uint8_t get_message_type() const { return this->data_[3]; }
  void set_destination_address(uint32_t address) {
    if (this->get_address_length() == 2) {
      this->data_[4] = (address >> 8) & 0xff;
      this->data_[5] = address & 0xff;
    } else {
      this->data_[4] = (address >> 16) & 0xff;
      this->data_[5] = (address >> 8) & 0xff;
      this->data_[6] = address & 0xff;
    }
  }
  uint32_t get_destination_address() const {
    if (this->get_address_length() == 2) {
      return (this->data_[4] << 8) + this->data_[5];
    }
    return (this->data_[4] << 16) + (this->data_[5] << 8) + this->data_[6];
  }
  void set_source_address(uint32_t address) {
    if (this->get_address_length() == 2) {
      this->data_[6] = (address >> 8) & 0xff;
      this->data_[7] = address & 0xff;
    } else {
      this->data_[7] = (address >> 16) & 0xff;
      this->data_[8] = (address >> 8) & 0xff;
      this->data_[9] = address & 0xff;
    }
  }
  uint32_t get_source_address() const {
    if (this->get_address_length() == 2) {
      return (this->data_[6] << 8) + this->data_[7];
    }
    return (this->data_[7] << 16) + (this->data_[8] << 8) + this->data_[9];
  }
  void set_message_id(uint8_t message_id) { this->data_[4 + 2 * this->get_address_length()] = message_id; }
  uint8_t get_message_id() const { return this->data_[4 + 2 * this->get_address_length()]; }
  void set_data(std::vector<uint8_t> data) {
    uint8_t size = std::min(MAX_DATA_LENGTH, static_cast<uint8_t>(data.size()));
    this->data_[2] &= (0xff ^ DATA_LENGTH_MASK);
    this->data_[2] |= (size & DATA_LENGTH_MASK);
    if (size)
      std::copy_n(data.begin(), size, this->data_.begin() + 5 + 2 * this->get_address_length());
  }
  std::vector<uint8_t> get_data() const {
    std::vector<uint8_t> data(this->data_.begin() + 5 + 2 * this->get_address_length(),
                              this->data_.begin() + 5 + 2 * this->get_address_length() + this->get_data_size());
    return data;
  }
  uint8_t get_data_size() const {
    return std::min(MAX_DATA_LENGTH, static_cast<uint8_t>(this->data_[2] & DATA_LENGTH_MASK));
  }
  void finalize() {
    if (this->auto_message_id && !this->get_retransmission() && !(this->data_[3] & 0x80)) {
      this->set_message_id(static_cast<uint8_t>(random_uint32()));
    }
    this->data_[0] = 0x55;
    this->data_[1] = 0xff;
    this->data_[this->size() - 1] = this->calc_cs_();
  }
  std::string to_string(uint8_t max_print_bytes = 255) const {
    std::string info;
    if (this->is_valid()) {
      info = str_sprintf(this->get_three_byte_address() ? "[%06" PRIX32 " %s %06" PRIX32 "] Type: %02X"
                                                        : "[%04" PRIX32 " %s %04" PRIX32 "] Type: %02X",
                         this->get_source_address(), this->get_retransmission() ? "»" : ">",
                         this->get_destination_address(), this->get_message_type());
      if (this->get_data_size())
        info += str_sprintf(", Data: %s", format_hex_pretty(this->get_data()).c_str());
    } else {
      info = "[Invalid]";
    }
    uint8_t print_bytes = std::min(this->size(), max_print_bytes);
    if (print_bytes)
      info = str_sprintf("%s %s", format_hex_pretty(this->data_.data(), print_bytes).c_str(), info.c_str());
    return info;
  }
  bool operator==(const ABBWelcomeData &rhs) const {
    if (std::equal(this->data_.begin(), this->data_.begin() + this->size(), rhs.data_.begin()))
      return true;
    return (this->auto_message_id || rhs.auto_message_id) && this->is_valid() && rhs.is_valid() &&
           (this->get_message_type() == rhs.get_message_type()) &&
           (this->get_source_address() == rhs.get_source_address()) &&
           (this->get_destination_address() == rhs.get_destination_address()) && (this->get_data() == rhs.get_data());
  }
  uint8_t &operator[](size_t idx) { return this->data_[idx]; }
  const uint8_t &operator[](size_t idx) const { return this->data_[idx]; }

 protected:
  std::array<uint8_t, 12 + MAX_DATA_LENGTH> data_;
  // Calculate checksum
  uint8_t calc_cs_() const;
};

class ABBWelcomeProtocol : public RemoteProtocol<ABBWelcomeData> {
 public:
  void encode(RemoteTransmitData *dst, const ABBWelcomeData &src) override;
  optional<ABBWelcomeData> decode(RemoteReceiveData src) override;
  void dump(const ABBWelcomeData &data) override;

 protected:
  void encode_byte_(RemoteTransmitData *dst, uint8_t data) const;
  bool decode_byte_(RemoteReceiveData &src, bool &done, uint8_t &data);
};

class ABBWelcomeBinarySensor : public RemoteReceiverBinarySensorBase {
 public:
  bool matches(RemoteReceiveData src) override {
    auto data = ABBWelcomeProtocol().decode(src);
    return data.has_value() && data.value() == this->data_;
  }
  void set_source_address(const uint32_t source_address) { this->data_.set_source_address(source_address); }
  void set_destination_address(const uint32_t destination_address) {
    this->data_.set_destination_address(destination_address);
  }
  void set_retransmission(const bool retransmission) { this->data_.set_retransmission(retransmission); }
  void set_three_byte_address(const bool three_byte_address) { this->data_.set_three_byte_address(three_byte_address); }
  void set_message_type(const uint8_t message_type) { this->data_.set_message_type(message_type); }
  void set_message_id(const uint8_t message_id) { this->data_.set_message_id(message_id); }
  void set_auto_message_id(const bool auto_message_id) { this->data_.auto_message_id = auto_message_id; }
  void set_data(const std::vector<uint8_t> &data) { this->data_.set_data(data); }
  void finalize() { this->data_.finalize(); }

 protected:
  ABBWelcomeData data_;
};

using ABBWelcomeTrigger = RemoteReceiverTrigger<ABBWelcomeProtocol>;
using ABBWelcomeDumper = RemoteReceiverDumper<ABBWelcomeProtocol>;

template<typename... Ts> class ABBWelcomeAction : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(uint32_t, source_address)
  TEMPLATABLE_VALUE(uint32_t, destination_address)
  TEMPLATABLE_VALUE(bool, retransmission)
  TEMPLATABLE_VALUE(bool, three_byte_address)
  TEMPLATABLE_VALUE(uint8_t, message_type)
  TEMPLATABLE_VALUE(uint8_t, message_id)
  TEMPLATABLE_VALUE(bool, auto_message_id)
  void set_data_static(std::vector<uint8_t> data) { data_static_ = std::move(data); }
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    has_data_func_ = true;
  }
  void encode(RemoteTransmitData *dst, Ts... x) override {
    ABBWelcomeData data;
    data.set_three_byte_address(this->three_byte_address_.value(x...));
    data.set_source_address(this->source_address_.value(x...));
    data.set_destination_address(this->destination_address_.value(x...));
    data.set_retransmission(this->retransmission_.value(x...));
    data.set_message_type(this->message_type_.value(x...));
    data.set_message_id(this->message_id_.value(x...));
    data.auto_message_id = this->auto_message_id_.value(x...);
    if (has_data_func_) {
      data.set_data(this->data_func_(x...));
    } else {
      data.set_data(this->data_static_);
    }
    data.finalize();
    ABBWelcomeProtocol().encode(dst, data);
  }

 protected:
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
  bool has_data_func_{false};
};

}  // namespace remote_base
}  // namespace esphome
