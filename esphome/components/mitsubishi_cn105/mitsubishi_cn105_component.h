#pragma once

#include "mitsubishi_cn105.h"

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <utility>

namespace esphome::mitsubishi_cn105 {

class MitsubishiCN105Component : public Component, public uart::UARTDevice {
 public:
  explicit MitsubishiCN105Component() : hp_(*this) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_update_interval(uint32_t ms) { this->hp_.set_update_interval(ms); }
  void set_telemetry_request_min_interval(uint32_t ms) { this->hp_.set_telemetry_request_min_interval(ms); }

  void set_remote_temperature(float temperature) { this->hp_.set_remote_temperature(temperature); }
  void clear_remote_temperature() { this->hp_.clear_remote_temperature(); }

  void set_power(bool power_on) { this->hp_.set_power(power_on); }
  void set_target_temperature(float target_temperature) { this->hp_.set_target_temperature(target_temperature); }
  void set_mode(MitsubishiCN105::Mode mode) { this->hp_.set_mode(mode); }
  void set_fan_mode(MitsubishiCN105::FanMode fan_mode) { this->hp_.set_fan_mode(fan_mode); }
  void set_vane_mode(MitsubishiCN105::VaneMode vane_mode) { this->hp_.set_vane_mode(vane_mode); }
  void set_wide_vane_mode(MitsubishiCN105::WideVaneMode mode) { this->hp_.set_wide_vane_mode(mode); }

  const MitsubishiCN105::Status &status() const { return this->hp_.status(); }
  bool is_status_initialized() const { return this->hp_.is_status_initialized(); }
  bool is_telemetry_polling_enabled() const { return this->hp_.is_telemetry_polling_enabled(); }

  template<typename F> void add_on_status_callback(F &&callback) {
    this->status_callback_.add(std::forward<F>(callback));
  }

  void publish_status() {
    if (this->is_status_initialized()) {
      this->status_callback_.call();
    }
  }

 protected:
  MitsubishiCN105 hp_;
  CallbackManager<void()> status_callback_;
};

}  // namespace esphome::mitsubishi_cn105
