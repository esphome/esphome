#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WIFI_TWT

#include "wifi_twt.h"
#include "esphome/core/automation.h"

namespace esphome::wifi_twt {

template<typename... Ts> class WiFiTWTStartAction : public Action<Ts...> {
 public:
  explicit WiFiTWTStartAction(WiFiTWT *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(uint32_t, wake_interval_ms)
  TEMPLATABLE_VALUE(uint32_t, wake_duration_ms)
  TEMPLATABLE_VALUE(uint8_t, setup_cmd)
  TEMPLATABLE_VALUE(uint8_t, flow_type)

  void play(Ts... x) override {
    if (this->wake_interval_ms_.has_value())
      this->parent_->set_wake_interval_ms(this->wake_interval_ms_.value(x...));
    if (this->wake_duration_ms_.has_value())
      this->parent_->set_wake_duration_ms(this->wake_duration_ms_.value(x...));
    if (this->setup_cmd_.has_value())
      this->parent_->set_setup_cmd(this->setup_cmd_.value(x...));
    if (this->flow_type_.has_value())
      this->parent_->set_flow_type(this->flow_type_.value(x...));
    this->parent_->start_twt();
  }

 protected:
  WiFiTWT *parent_;
};

template<typename... Ts> class WiFiTWTStopAction : public Action<Ts...> {
 public:
  explicit WiFiTWTStopAction(WiFiTWT *parent) : parent_(parent) {}
  void play(Ts... x) override { this->parent_->stop_twt(); }

 protected:
  WiFiTWT *parent_;
};

}  // namespace esphome::wifi_twt

#endif  // USE_WIFI_TWT
