#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TUYA_WEATHER
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "tuya_uart.h"

namespace esphome::tuya {

template<typename... Ts> class TuyaSendWeatherAction final : public Action<Ts...> {
 public:
  TuyaSendWeatherAction(TuyaUART *parent, size_t count) : parent_(parent) {
    this->entries_.init(count);
    this->values_.reserve(count);
  }
  void add_int(const char *key, TemplatableValue<int32_t, Ts...> value) {
    this->entries_.push_back(Entry{key, false, std::move(value), {}});
  }
  void add_string(const char *key, TemplatableValue<std::string, Ts...> value) {
    this->entries_.push_back(Entry{key, true, {}, std::move(value)});
  }
  void add_string(const char *key, const char *constant) {
    this->entries_.push_back(Entry{key, true, {}, {}, constant});
  }
  void play(const Ts &...x) override {
    this->values_.resize(this->entries_.size());
    size_t i = 0;
    for (auto &e : this->entries_) {
      TuyaWeatherValue &value = this->values_[i++];
      value.key.assign(e.key);
      value.is_string = e.is_string;
      value.value_int = e.is_string ? 0 : e.int_value.value(x...);
      if (e.constant != nullptr) {
        value.value_string.assign(e.constant);
      } else if (e.is_string) {
        value.value_string = e.string_value.value(x...);
      }
    }
    this->parent_->send_weather(this->values_);
  }

 protected:
  struct Entry {
    const char *key;  // codegen literal
    bool is_string;
    TemplatableValue<int32_t, Ts...> int_value;
    TemplatableValue<std::string, Ts...> string_value;
    const char *constant{nullptr};  // codegen literal of a string that is no lambda
  };
  TuyaUART *parent_;
  FixedVector<Entry> entries_;
  std::vector<TuyaWeatherValue> values_;
};

}  // namespace esphome::tuya

#endif  // USE_TUYA_WEATHER
