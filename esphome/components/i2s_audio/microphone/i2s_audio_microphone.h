#pragma once

#ifdef USE_ESP32

#include "../i2s_audio.h"

#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace esphome {
namespace i2s_audio {

class I2SAudioMicrophone : public I2SAudioIn, public microphone::Microphone, public Component {
 public:
  void setup() override;
  void start() override;
  void stop() override;

  void loop() override;

  void set_correct_dc_offset(bool correct_dc_offset) { this->correct_dc_offset_ = correct_dc_offset; }

#ifdef USE_I2S_LEGACY
  void set_din_pin(int8_t pin) { this->din_pin_ = pin; }
#else
  void set_din_pin(int8_t pin) { this->din_pin_ = (gpio_num_t) pin; }
#endif

  void set_pdm(bool pdm) { this->pdm_ = pdm; }

#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_ADC
  void set_adc_channel(adc1_channel_t channel) {
    this->adc_channel_ = channel;
    this->adc_ = true;
  }
#endif
#endif

 protected:
  /// @brief Starts the I2S driver. Updates the ``audio_stream_info_`` member variable with the current setttings.
  /// @return True if succesful, false otherwise
  bool start_driver_();

  /// @brief Stops the I2S driver.
  void stop_driver_();

  /// @brief Attempts to correct a microphone DC offset; e.g., a microphones silent level is offset from 0. Applies a
  /// correction offset that is updated using an exponential moving average for all samples away from 0.
  /// @param data
  void fix_dc_offset_(std::vector<uint8_t> &data);

  size_t read_(uint8_t *buf, size_t len, TickType_t ticks_to_wait);

  /// @brief Sets the Microphone ``audio_stream_info_`` member variable to the configured I2S settings.
  void configure_stream_settings_();

  static void mic_task(void *params);

  SemaphoreHandle_t active_listeners_semaphore_{nullptr};
  EventGroupHandle_t event_group_{nullptr};

  TaskHandle_t task_handle_{nullptr};

#ifdef USE_I2S_LEGACY
  int8_t din_pin_{I2S_PIN_NO_CHANGE};
#if SOC_I2S_SUPPORTS_ADC
  adc1_channel_t adc_channel_{ADC1_CHANNEL_MAX};
  bool adc_{false};
#endif
#else
  gpio_num_t din_pin_{I2S_GPIO_UNUSED};
  i2s_chan_handle_t rx_handle_;
#endif
  bool pdm_{false};

  bool correct_dc_offset_;
  int32_t dc_offset_{0};
};

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
