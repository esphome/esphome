#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TUYA_WEATHER
#include <string>
#include <vector>

#include "esphome/core/automation.h"
#include "tuya.h"

namespace esphome::tuya {

template<typename... Ts> class TuyaSendWeatherAction final : public Action<Ts...> {
 public:
  explicit TuyaSendWeatherAction(Tuya *parent) : parent_(parent) {}
  void add_int(const std::string &key, TemplatableValue<int32_t, Ts...> value) {
    this->entries_.push_back(Entry{key, false, std::move(value), {}});
  }
  void add_string(const std::string &key, TemplatableValue<std::string, Ts...> value) {
    this->entries_.push_back(Entry{key, true, {}, std::move(value)});
  }
  void play(const Ts &...x) override {
    std::vector<TuyaWeatherValue> values;
    values.reserve(this->entries_.size());
    for (auto &e : this->entries_) {
      if (e.is_string) {
        values.push_back(TuyaWeatherValue{e.key, true, 0, e.string_value.value(x...)});
      } else {
        values.push_back(TuyaWeatherValue{e.key, false, e.int_value.value(x...), {}});
      }
    }
    this->parent_->send_weather(values);
  }

 protected:
  struct Entry {
    std::string key;
    bool is_string;
    TemplatableValue<int32_t, Ts...> int_value;
    TemplatableValue<std::string, Ts...> string_value;
  };
  Tuya *parent_;
  std::vector<Entry> entries_;
};

}  // namespace esphome::tuya

#endif  // USE_TUYA_WEATHER
