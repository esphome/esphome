#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

#include <vector>

#if defined(USE_ESP32)
#include <driver/rmt_tx.h>
#endif

namespace esphome {
namespace remote_transmitter {

#ifdef USE_ESP32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
// IDF version 5.5.1 and above is required because of a bug in
// the RMT encoder: https://github.com/espressif/esp-idf/issues/17244
typedef union {  // NOLINT(modernize-use-using)
  struct {
    uint16_t duration : 15;
    uint16_t level : 1;
  };
  uint16_t val;
} rmt_symbol_half_t;

struct RemoteTransmitterComponentStore {
  uint32_t times{0};
  uint32_t index{0};
};
#endif
#endif

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

  // transmitter setup must run after receiver setup to allow the same GPIO to be used by both
  float get_setup_priority() const override { return setup_priority::DATA - 1; }

  void set_carrier_duty_percent(uint8_t carrier_duty_percent) { this->carrier_duty_percent_ = carrier_duty_percent; }

  void digital_write(bool value);

#if defined(USE_ESP32)
  void set_with_dma(bool with_dma) { this->with_dma_ = with_dma; }
  void set_eot_level(bool eot_level) { this->eot_level_ = eot_level; }
  void set_non_blocking(bool non_blocking) { this->non_blocking_ = non_blocking; }
#endif

  Trigger<> *get_transmit_trigger() const { return this->transmit_trigger_; };
  Trigger<> *get_complete_trigger() const { return this->complete_trigger_; };

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override;
#if defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2040)
  void calculate_on_off_time_(uint32_t carrier_frequency, uint32_t *on_time_period, uint32_t *off_time_period);

  void mark_(uint32_t on_time, uint32_t off_time, uint32_t usec);

  void space_(uint32_t usec);

  void await_target_time_();
  uint32_t target_time_;
#endif

#ifdef USE_ESP32
  void configure_rmt_();
  void wait_for_rmt_();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
  RemoteTransmitterComponentStore store_{};
  std::vector<rmt_symbol_half_t> rmt_temp_;
#else
  std::vector<rmt_symbol_word_t> rmt_temp_;
#endif
  uint32_t current_carrier_frequency_{38000};
  bool initialized_{false};
  bool with_dma_{false};
  bool eot_level_{false};
  rmt_channel_handle_t channel_{NULL};
  rmt_encoder_handle_t encoder_{NULL};
  esp_err_t error_code_{ESP_OK};
  std::string error_string_{""};
  bool inverted_{false};
  bool non_blocking_{false};
#endif
  uint8_t carrier_duty_percent_;

  Trigger<> *transmit_trigger_{new Trigger<>()};
  Trigger<> *complete_trigger_{new Trigger<>()};
};

}  // namespace remote_transmitter
}  // namespace esphome
