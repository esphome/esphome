#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

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
#ifdef USE_ESP32
  RemoteTransmitterComponent(InternalGPIOPin *pin, uint8_t mem_block_num = 1)
      : remote_base::RemoteTransmitterBase(pin), remote_base::RemoteRMTChannel(mem_block_num) {}

  RemoteTransmitterComponent(InternalGPIOPin *pin, rmt_channel_t channel, uint8_t mem_block_num = 1)
      : remote_base::RemoteTransmitterBase(pin), remote_base::RemoteRMTChannel(channel, mem_block_num) {}
#else
  explicit RemoteTransmitterComponent(InternalGPIOPin *pin) : remote_base::RemoteTransmitterBase(pin) {}
#endif
  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_carrier_duty_percent(uint8_t carrier_duty_percent) { this->carrier_duty_percent_ = carrier_duty_percent; }

  Trigger<> *get_transmit_trigger() const { return this->transmit_trigger_; };
  Trigger<> *get_complete_trigger() const { return this->complete_trigger_; };

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override;
#if defined(USE_ESP8266) || defined(USE_LIBRETINY)
  void calculate_on_off_time_(uint32_t carrier_frequency, uint32_t *on_time_period, uint32_t *off_time_period);

  void mark_(uint32_t on_time, uint32_t off_time, uint32_t usec);

  void space_(uint32_t usec);

  void await_target_time_();
  uint32_t target_time_;
#endif

#ifdef USE_ESP32
  void configure_rmt_();

  uint32_t current_carrier_frequency_{38000};
  bool initialized_{false};
  std::vector<rmt_item32_t> rmt_temp_;
  esp_err_t error_code_{ESP_OK};
  std::string error_string_{""};
  bool inverted_{false};
#endif
  uint8_t carrier_duty_percent_;

  Trigger<> *transmit_trigger_{new Trigger<>()};
  Trigger<> *complete_trigger_{new Trigger<>()};
};

}  // namespace remote_transmitter
}  // namespace esphome
