#pragma once

#include "binary_storage.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome::binary_storage {

//========================================================================
// Actions for Automations
//========================================================================

template<typename... Ts> class ReadAction : public Action<Ts...> {
 public:
  explicit ReadAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint32_t, length)

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);
    uint32_t len = this->length_.value(x...);

    if (len > 0 && len <= 4096) {
      std::vector<uint8_t> buffer(len);
      this->storage_->read(addr, buffer.data(), len, nullptr);
    }
  }

 protected:
  BinaryStorage *storage_;
};

template<typename... Ts> class WriteAction : public Action<Ts...> {
 public:
  explicit WriteAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)

  void set_data(const std::vector<uint8_t> &data) { this->data_ = data; }
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) { this->data_func_ = func; }

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);

    std::vector<uint8_t> data;
    if (this->data_func_.has_value()) {
      data = this->data_func_.value()(x...);
    } else {
      data = this->data_;
    }

    if (!data.empty()) {
      this->storage_->write(addr, data.data(), data.size(), nullptr);
    }
  }

 protected:
  BinaryStorage *storage_;
  std::vector<uint8_t> data_;
  optional<std::function<std::vector<uint8_t>(Ts...)>> data_func_;
};

template<typename... Ts> class FillAction : public Action<Ts...> {
 public:
  explicit FillAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint8_t, value)

  void play(Ts... x) override {
    uint8_t val = this->value_.value(x...);
    this->storage_->fill(val);
  }

 protected:
  BinaryStorage *storage_;
};

template<typename... Ts> class WriteByteAction : public Action<Ts...> {
 public:
  explicit WriteByteAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(uint8_t, value)

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);
    uint8_t val = this->value_.value(x...);
    this->storage_->write(addr, &val, 1, nullptr);
  }

 protected:
  BinaryStorage *storage_;
};

template<typename... Ts> class WriteStringAction : public Action<Ts...> {
 public:
  explicit WriteStringAction(BinaryStorage *storage) : storage_(storage) {}

  TEMPLATABLE_VALUE(uint32_t, address)
  TEMPLATABLE_VALUE(std::string, value)

  void play(Ts... x) override {
    uint32_t addr = this->address_.value(x...);
    std::string str = this->value_.value(x...);
    this->storage_->write(addr, reinterpret_cast<const uint8_t *>(str.c_str()), str.length() + 1, nullptr);
  }

 protected:
  BinaryStorage *storage_;
};

//========================================================================
// Conditions for Automations
//========================================================================

template<typename... Ts> class IsReadyCondition : public Condition<Ts...> {
 public:
  explicit IsReadyCondition(BinaryStorage *storage) : storage_(storage) {}

  bool check(Ts... x) override { return this->storage_->is_ready(); }

 protected:
  BinaryStorage *storage_;
};

}  // namespace esphome::binary_storage
