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
  /// `count` is the number of add_int() and add_string() calls that follow.
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
  void play(const Ts &...x) override {
    // values_ keeps its storage from one run to the next
    this->values_.clear();
    for (auto &e : this->entries_) {
      if (e.is_string) {
        this->values_.push_back(TuyaWeatherValue{e.key, true, 0, e.string_value.value(x...)});
      } else {
        this->values_.push_back(TuyaWeatherValue{e.key, false, e.int_value.value(x...), {}});
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
  };
  TuyaUART *parent_;
  FixedVector<Entry> entries_;
  std::vector<TuyaWeatherValue> values_;
};

}  // namespace esphome::tuya

#endif  // USE_TUYA_WEATHER
