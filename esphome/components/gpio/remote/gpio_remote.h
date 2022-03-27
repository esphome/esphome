#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/remote/remote.h"
#include "protocol_helper.h"

#ifdef USE_ESP32
#include <driver/rmt.h>
#endif

namespace esphome {
namespace gpio {

#ifdef USE_ESP32
class GPIORemoteRMTChannel {
 public:
  explicit GPIORemoteRMTChannel(uint8_t mem_block_num = 1);

  void config_rmt(rmt_config_t &rmt);
  void set_clock_divider(uint8_t clock_divider) { this->clock_divider_ = clock_divider; }

 protected:
  uint32_t from_microseconds_(uint32_t us) {
    const uint32_t ticks_per_ten_us = 80000000u / this->clock_divider_ / 100000u;
    return us * ticks_per_ten_us / 10;
  }
  uint32_t to_microseconds_(uint32_t ticks) {
    const uint32_t ticks_per_ten_us = 80000000u / this->clock_divider_ / 100000u;
    return (ticks * 10) / ticks_per_ten_us;
  }
  // RemoteComponentBase *remote_base_;
  rmt_channel_t channel_{RMT_CHANNEL_0};
  uint8_t mem_block_num_;
  uint8_t clock_divider_{80};
};
#endif

class GPIORemote : public remote::Remote,
                   public Component
#ifdef USE_ESP32
    ,
                   public GPIORemoteRMTChannel
#endif

{
 public:
  explicit GPIORemote(InternalGPIOPin *transmit_pin) : transmit_pin_(transmit_pin) {}

  void setup() override;

  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_carrier_duty_percent(uint8_t carrier_duty_percent) { carrier_duty_percent_ = carrier_duty_percent; }

  void add_protocol(RemoteProtocolCodec *protocol) {
    protocols_.push_back(protocol);
    capabilities_.supports_transmit.push_back(protocol->get_name());
  };

  RemoteProtocolCodec *get_protocol(const std::string &name) {
    for (auto *proto : protocols_) {
      if (proto->get_name() == name)
        return proto;
    }
    return nullptr;
  }

 protected:
  // Transmitter members
  void transmit(int repeat, int wait, const std::string &name, const std::vector<remote::arg_t> &args) override;
  void send_internal_(uint32_t send_times, uint32_t send_wait);

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
  // properties
  RemoteSignalData temp_;
  std::vector<RemoteProtocolCodec *> protocols_;
  InternalGPIOPin *transmit_pin_;
  uint8_t carrier_duty_percent_{50};
};
}  // namespace gpio
}  // namespace esphome
