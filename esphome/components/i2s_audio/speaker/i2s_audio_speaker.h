#pragma once

#ifdef USE_ESP32

#include "../i2s_audio.h"

#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace i2s_audio {

static const size_t BUFFER_SIZE = 1024;

enum class TaskEventType : uint8_t {
  PLAYING,
  PAUSING,
  WARNING = 255,
};

struct TaskEvent {
  TaskEventType type;
  esp_err_t err;
  int8_t data{0};
  bool stopped{false};
};

class I2SAudioSpeaker : public Component, public speaker::Speaker, public I2SAudioOut {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::LATE; }

  void setup() override;
  void loop() override;

  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
#if SOC_I2S_SUPPORTS_DAC
  void set_internal_dac_mode(i2s_dac_mode_t mode) { this->internal_dac_mode_ = mode; }
#endif
  void set_external_dac_channels(uint8_t channels) { this->external_dac_channels_ = channels; }
  void set_16bit_mode(bool mode) { this->use_16bit_mode_ = mode; }

  void start() override;
  void stop() override;
  void finish() override;
  void flush() override;

  size_t play(const uint8_t *data, size_t length) override;

  bool has_buffered_data() const override;
  size_t available_space() const override;

 protected:
  void start_();
  void stop_();
  void watch_();
  void set_state_(speaker::State state);
  uint8_t wordsize_() { return this->use_16bit_mode_ ? 2 : 4; }
  static void player_task(void *params);

  TaskHandle_t player_task_handle_{nullptr};
  StreamBufferHandle_t buffer_queue_{nullptr};
  QueueHandle_t event_queue_{nullptr};

  uint8_t dout_pin_{0};
  bool use_16bit_mode_{false};

#if SOC_I2S_SUPPORTS_DAC
  i2s_dac_mode_t internal_dac_mode_{I2S_DAC_CHANNEL_DISABLE};
#endif
  uint8_t external_dac_channels_;
};

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
