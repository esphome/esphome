#pragma once

#ifdef USE_ESP32
#include <cstddef>
#include <cstdint>
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/components/speaker/speaker.h"

#include "driver/sdm.h"
#include "driver/gptimer.h"

namespace esphome::sigma_delta {

class SigmaDeltaSpeaker final : public Component, public speaker::Speaker {
 public:
  ~SigmaDeltaSpeaker();
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_pin(InternalGPIOPin *pin) { this->pin_ = pin; }
  void set_sample_rate(uint32_t rate) { this->sample_rate_ = rate; }
  void set_oversample_rate(uint32_t rate) { this->oversample_rate_ = rate; }
  void set_bits_per_sample(uint8_t bits) { this->bits_per_sample_ = bits; }
  void set_num_channels(uint8_t ch) { this->num_channels_ = ch; }

  void start() override;
  void stop() override;
  size_t play(const uint8_t *data, size_t length) override;
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override {
    return this->play(data, length);
  }
  bool has_buffered_data() const override;

 protected:
  static bool IRAM_ATTR timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

  InternalGPIOPin *pin_{nullptr};
  uint32_t sample_rate_{44100};
  uint32_t oversample_rate_{1'000'000};
  uint8_t bits_per_sample_{16};
  uint8_t num_channels_{2};

  sdm_channel_handle_t sdm_handle_{nullptr};
  gptimer_handle_t timer_handle_{nullptr};

  // Ring buffer for ISR-safe audio queue (SPSC)
  static constexpr size_t RING_SIZE = 8192;
  int8_t *ring_buf_{nullptr};
  volatile size_t ring_read_{0};
  volatile size_t ring_write_{0};

  bool running_{false};
};

}  // namespace esphome::sigma_delta

#endif  // USE_ESP32
