#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"

#include <vector>

namespace esphome {
namespace remote_transmitter {

class RemoteTransmitterComponent : public remote_base::RemoteTransmitterBase,
                                   public Component
#ifdef USE_ESP32
    ,
                                   public remote_base::RemoteRMTChannel
#endif
{
 public:
  explicit RemoteTransmitterComponent(InternalGPIOPin *pin) : remote_base::RemoteTransmitterBase(pin) {}

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_carrier_duty_percent(uint8_t carrier_duty_percent) { this->carrier_duty_percent_ = carrier_duty_percent; }

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override;
#ifdef USE_ESP8266
  void calculate_on_off_time_(uint32_t carrier_frequency, uint32_t *on_time_period, uint32_t *off_time_period);

  void mark_(uint32_t on_time, uint32_t off_time, uint32_t usec);

  void space_(uint32_t usec);

  void await_target_time_();
  uint32_t target_time_;
#endif

#ifdef USE_ESP32
  void configure_rmt_();

  uint32_t current_carrier_frequency_{UINT32_MAX};
  bool initialized_{false};
  std::vector<rmt_item32_t> rmt_temp_;
  esp_err_t error_code_{ESP_OK};
  bool inverted_{false};
#endif
  uint8_t carrier_duty_percent_{50};
};

}  // namespace remote_transmitter
}  // namespace esphome
