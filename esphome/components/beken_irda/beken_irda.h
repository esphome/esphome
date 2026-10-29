#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

namespace esphome::beken_irda {

class BekenIrdaComponent final : public Component, public remote_base::RemoteReceiverBase {
 public:
  explicit BekenIrdaComponent(InternalGPIOPin *pin) : remote_base::RemoteReceiverBase(pin) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void set_hardware(bool hardware) { this->hardware_ = hardware; }
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_timer_channel(uint8_t channel) { this->timer_channel_ = channel; }
  void set_sample_period(uint32_t sample_period_us) { this->sample_period_ = sample_period_us; }
  void set_filter_us(uint32_t filter_us) { this->filter_us_ = filter_us; }
  void set_idle_us(uint32_t idle_us) { this->idle_us_ = idle_us; }
  void set_buffer_size(uint16_t buffer_size) { this->buffer_size_ = buffer_size; }

  /// Called from the hardware IRDA interrupt handler.
  void handle_isr();
  /// Called from the sampling timer interrupt handler.
  void handle_timer_isr();

 protected:
  void log_registers_(const LogString *where);
  void setup_hardware_();
  void setup_gpio_timer_();
  void dispatch_full_(uint32_t code);
  void dispatch_repeat_();
  void push_sample_(int32_t value);
  void reset_capture_();

  bool hardware_{false};
  bool debug_{false};
  uint8_t timer_channel_{1};
  uint32_t sample_period_{100};
  uint32_t filter_us_{50};
  uint32_t idle_us_{10000};
  uint16_t buffer_size_{512};

  // Hardware NEC decoder state.
  volatile uint32_t raw_code_{0};
  volatile uint32_t raw_status_{0};
  volatile uint32_t isr_count_{0};
  volatile uint32_t right_count_{0};
  volatile uint32_t end_count_{0};
  volatile uint32_t repeat_count_{0};
  volatile uint32_t error_count_{0};
  volatile bool raw_ready_{false};
  volatile bool repeat_ready_{false};

  // Timer sampling state. The ISR writes samples into the ring between head and tail.
  ISRInternalGPIOPin isr_pin_;
  int32_t *buffer_{nullptr};
  volatile uint32_t buffer_head_{0};
  volatile uint32_t buffer_tail_{0};
  volatile bool frame_ready_{false};
  volatile bool overflow_{false};
  volatile bool capture_active_{false};
  volatile uint32_t timer_ticks_{0};
  volatile uint32_t carry_ticks_{0};
  volatile uint32_t timer_edges_{0};
  volatile uint32_t timer_high_{0};
  volatile uint32_t timer_low_{0};
  volatile uint8_t last_level_{1};
  volatile uint32_t frames_count_{0};
  volatile uint32_t overflow_count_{0};
  volatile uint32_t filtered_count_{0};
  volatile uint32_t buffer_high_water_{0};

  volatile uint32_t last_code_{0};
  volatile uint32_t dropped_repeat_count_{0};

  uint32_t timer_init_ret_{0xFFFFFFFFu};
  uint32_t timer_enable_ret_{0xFFFFFFFFu};
  uint32_t loop_count_{0};
  uint32_t last_heartbeat_{0};
  bool logged_first_loop_{false};
};

}  // namespace esphome::beken_irda
