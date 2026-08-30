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

// The BK7231N-style PWM block (hardware shadow-load duty updates) enables the ISR-driven
// transmitter on these families; family-level proxy for the SDK's CFG_SOC_NAME gate.
// See remote_transmitter_bk72xx.cpp.
#if defined(USE_LIBRETINY_VARIANT_BK7231N) || defined(USE_LIBRETINY_VARIANT_BK7238)
#define REMOTE_TRANSMITTER_BK_PWM
#endif

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
#if (defined(USE_ESP32) && SOC_RMT_SUPPORTED) || defined(USE_LIBRETINY_VARIANT_RTL8720C) || \
    defined(REMOTE_TRANSMITTER_BK_PWM)
  void set_non_blocking(bool non_blocking) { this->non_blocking_ = non_blocking; }
#endif
#if defined(USE_LIBRETINY_VARIANT_RTL8720C) || defined(REMOTE_TRANSMITTER_BK_PWM)
  void loop() override;
  // called from the envelope timer ISR trampoline; not part of the public API
  void advance_envelope_isr();
  // same, for trampolines whose SDK callback carries no user argument
  static void advance_active_isr();
#endif

  Trigger<> *get_transmit_trigger() { return &this->transmit_trigger_; }
  Trigger<> *get_complete_trigger() { return &this->complete_trigger_; }

 protected:
  void send_internal(uint32_t send_times, uint32_t send_wait) override;
#if defined(USE_ESP8266) || \
    (defined(USE_LIBRETINY) && !defined(USE_LIBRETINY_VARIANT_RTL8720C) && !defined(REMOTE_TRANSMITTER_BK_PWM)) || \
    defined(USE_RP2) || (defined(USE_ESP32) && !SOC_RMT_SUPPORTED)
  void await_target_time_();
  uint32_t target_time_{0};
#endif
#if defined(USE_ESP8266) || \
    (defined(USE_LIBRETINY) && !defined(USE_RTL87XX) && !defined(REMOTE_TRANSMITTER_BK_PWM)) || defined(USE_RP2) || \
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
#if defined(USE_LIBRETINY_VARIANT_RTL8720C) || defined(REMOTE_TRANSMITTER_BK_PWM)
  // Envelope chain, shared by every family that paces transmission from a hardware timer
  // (remote_transmitter_libretiny_isr.cpp)
  void start_isr_item_(size_t index);
  void arm_envelope_timer_(uint32_t duration_us);
  void abort_stalled_chain_();
  void deliver_completion_();
  void wait_until_idle_();
  void arm_chain_(uint32_t send_times, uint32_t send_wait);
  // Hooks implemented per family: everything the chain needs from the hardware
  bool envelope_ready_() const;                       // PWM claimed successfully in setup()
  void prepare_carrier_(uint32_t carrier_frequency);  // retune period, stage mark/space levels
  void write_envelope_level_(bool mark);              // drive carrier (mark) or idle (space)
  void arm_one_shot_(uint32_t duration_us);           // fire advance_envelope_isr after duration_us
  void stop_envelope_timer_();
  std::vector<int32_t> isr_data_;  // owned copy of the frame; temp_ may be re-encoded mid-flight
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
#ifdef USE_LIBRETINY_VARIANT_RTL8720C
  float isr_mark_duty_{0.0f};
  float isr_space_duty_{0.0f};
#endif
#ifdef REMOTE_TRANSMITTER_BK_PWM
  void write_pwm_t1_(uint32_t t1_counts);
  uint32_t isr_mark_t1_{0};
  uint32_t isr_space_t1_{0};
  uint32_t isr_period_t4_{684};  // 26MHz counts; ~38kHz default until a send sets the real carrier
  int8_t pwm_channel_{-1};
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
