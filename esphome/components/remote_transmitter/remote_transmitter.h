#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

#include <vector>

#if defined(USE_ESP32)
#include <soc/soc_caps.h>
#if SOC_RMT_SUPPORTED
#include <driver/rmt_tx.h>
#endif  // SOC_RMT_SUPPORTED
#endif  // USE_ESP32

namespace esphome::remote_transmitter {

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
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

class RemoteTransmitterComponent final : public remote_base::RemoteTransmitterBase,
                                         public Component
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
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

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  void set_with_dma(bool with_dma) { this->with_dma_ = with_dma; }
  void set_eot_level(bool eot_level) { this->eot_level_ = eot_level; }
#endif
#if (defined(USE_ESP32) && SOC_RMT_SUPPORTED) || defined(USE_LIBRETINY_VARIANT_RTL8720C)
  void set_non_blocking(bool non_blocking) { this->non_blocking_ = non_blocking; }
#endif
#ifdef USE_LIBRETINY_VARIANT_RTL8720C
  void loop() override;
  // called from the envelope timer ISR trampoline; not part of the public API
  void advance_envelope_isr();
#endif

  Trigger<> *get_transmit_trigger() { return &this->transmit_trigger_; }
  Trigger<> *get_complete_trigger() { return &this->complete_trigger_; }

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override;
#if defined(USE_ESP8266) || (defined(USE_LIBRETINY) && !defined(USE_LIBRETINY_VARIANT_RTL8720C)) || \
    defined(USE_RP2) || (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)
  void await_target_time_();
  uint32_t target_time_{0};
#endif
#if defined(USE_ESP8266) || (defined(USE_LIBRETINY) && !defined(USE_RTL87XX)) || defined(USE_RP2) || \
    (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)
  void calculate_on_off_time_(uint32_t carrier_frequency, uint32_t *on_time_period, uint32_t *off_time_period);

  void mark_(uint32_t on_time, uint32_t off_time, uint32_t usec);

  void space_(uint32_t usec);
#endif
#ifdef USE_RTL87XX
  // Carrier frequency the PWM is currently configured for; 0 = not yet configured
  uint32_t current_carrier_frequency_{0};
  void *pwm_{nullptr};  // pwmout_t*, opaque here to keep the SDK header out of this shared header
#endif
#ifdef USE_LIBRETINY_VARIANT_RTL8720C
  void start_isr_item_(size_t index);
  void arm_envelope_timer_(uint32_t duration_us);
  void abort_stalled_chain_();
  void deliver_completion_();
  void wait_until_idle_();
  void arm_chain_(uint32_t send_times, uint32_t send_wait);
  void update_carrier_(uint32_t carrier_frequency);
  std::vector<int32_t> isr_data_;  // owned copy of the frame; temp_ may be re-encoded mid-flight
  float isr_mark_duty_{0.0f};
  float isr_space_duty_{0.0f};
  volatile size_t isr_index_{0};
  volatile uint32_t isr_repeats_left_{0};
  uint32_t isr_send_wait_{0};
  volatile uint32_t isr_wait_remaining_{0};  // remainder of a duration chained across one-shots
  volatile bool isr_in_gap_{false};
  volatile bool transmitting_{false};
  bool non_blocking_{false};
  bool complete_pending_{false};
  bool stall_aborted_{false};  // this transmission ended via abort; blocks warning clear
#endif

#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
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
  std::string error_string_;
  bool inverted_{false};
  bool non_blocking_{false};
#endif
  uint8_t carrier_duty_percent_{50};

  Trigger<> transmit_trigger_;
  Trigger<> complete_trigger_;
};

}  // namespace esphome::remote_transmitter
