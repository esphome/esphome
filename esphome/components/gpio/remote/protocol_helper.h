#pragma once

#include <utility>

#include "esphome/components/remote/remote.h"

namespace esphome {
namespace gpio {

class RemoteSignalData {
 public:
  void mark(uint32_t length) { this->data_.push_back(length); }

  void space(uint32_t length) { this->data_.push_back(-length); }

  void item(uint32_t mark, uint32_t space) {
    this->mark(mark);
    this->space(space);
  }

  void reserve(uint32_t len) { this->data_.reserve(len); }

  void set_carrier_frequency(uint32_t carrier_frequency) { this->carrier_frequency_ = carrier_frequency; }

  uint32_t get_carrier_frequency() const { return this->carrier_frequency_; }

  const std::vector<int32_t> &get_data() const { return this->data_; }

  template<typename T> void set_data(const std::vector<T> &data, size_t size = 0) {
    this->data_.clear();
    this->data_.reserve(data.size());
    size_t i = 0;

    for (auto it = data.begin(); it != data.end() && (i < size || !size); it++, i++) {
      this->data_.push_back((int32_t) *it);
    }
  }

  void reset() {
    this->data_.clear();
    this->carrier_frequency_ = 0;
  }

  std::vector<int32_t>::iterator begin() { return this->data_.begin(); }

  std::vector<int32_t>::iterator end() { return this->data_.end(); }

 protected:
  std::vector<int32_t> data_{};
  uint32_t carrier_frequency_{0};
};

class RemoteProtocolCodec {
 public:
  RemoteProtocolCodec(std::string name) : name_(std::move(name)) {}

  virtual void encode(RemoteSignalData *dst, const std::vector<remote::arg_t> &command) = 0;

  // TODO: make pure virtual and implement receiver function
  virtual std::vector<remote::arg_t> decode(const RemoteSignalData &src) {
    std::vector<remote::arg_t> empty;
    return empty;
  }

  virtual void dump(const std::vector<remote::arg_t> &command) = 0;

  const std::string &get_name() { return name_; }

 private:
  std::string name_;
};

}  // namespace gpio
}  // namespace esphome
