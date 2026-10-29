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
  void set_sample_period(uint32_t sample_period_us) { this->sample_period_ = sample_period_us; }
  void set_filter_us(uint32_t filter_us) { this->filter_us_ = filter_us; }
  void set_idle_us(uint32_t idle_us) { this->idle_us_ = idle_us; }
  void set_buffer_size(uint16_t buffer_size) { this->buffer_size_ = buffer_size; }

  /// Called from the hardware IRDA interrupt handler.
  void handle_isr();
  /// Called from the sampling timer interrupt handler.
  void handle_timer_isr();

 protected:
  void setup_hardware_();
  void setup_gpio_timer_();
  void dispatch_full_(uint32_t code);
  void dispatch_repeat_();
  void push_sample_(int32_t value);
  void finish_run_(uint8_t level, uint32_t ticks);
  void push_run_();
  void reset_capture_();

  bool hardware_{false};
  uint32_t sample_period_{100};
  uint32_t filter_us_{50};
  uint32_t idle_us_{10000};
  uint16_t buffer_size_{512};

  // Hardware NEC decoder state.
  volatile uint32_t last_code_{0};
  volatile bool raw_ready_{false};
  volatile bool repeat_ready_{false};
  volatile uint32_t reject_count_{0};
  volatile uint32_t error_count_{0};
  uint32_t last_reject_{0};
  uint32_t last_error_{0};

  // Timer sampling state. A run shorter than the filter is folded into the run before it, so
  // the emitted samples keep alternating between marks and spaces.
  ISRInternalGPIOPin isr_pin_;
  int32_t *buffer_{nullptr};
  volatile uint32_t buffer_count_{0};
  volatile bool frame_ready_{false};
  volatile bool overflow_{false};
  volatile bool capture_active_{false};
  volatile uint32_t timer_ticks_{0};
  volatile bool run_active_{false};
  volatile uint8_t run_level_{1};
  volatile uint32_t run_ticks_{0};
  volatile uint8_t last_level_{0};
  volatile uint32_t overflow_count_{0};
};

}  // namespace esphome::beken_irda
