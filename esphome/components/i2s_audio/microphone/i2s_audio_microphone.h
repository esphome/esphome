#pragma once

#ifdef USE_ESP32

#include "../i2s_audio.h"

#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace esphome::i2s_audio {

class I2SAudioMicrophone final : public I2SAudioIn, public microphone::Microphone, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void start() override;
  void stop() override;

  void loop() override;

  void set_correct_dc_offset(bool correct_dc_offset) { this->correct_dc_offset_ = correct_dc_offset; }

  void set_din_pin(int8_t pin) { this->din_pin_ = (gpio_num_t) pin; }

  void set_pdm(bool pdm) { this->pdm_ = pdm; }

#if SOC_I2S_SUPPORTS_PDM_RX
  void set_pdm_dsr(i2s_pdm_dsr_t pdm_dsr) { this->pdm_dsr_ = pdm_dsr; }
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

  size_t read_(uint8_t *buf, size_t len, uint32_t timeout_ms);

  /// @brief Sets the Microphone ``audio_stream_info_`` member variable to the configured I2S settings.
  void configure_stream_settings_();

  static void mic_task(void *params);

  SemaphoreHandle_t active_listeners_semaphore_{nullptr};
  EventGroupHandle_t event_group_{nullptr};

  TaskHandle_t task_handle_{nullptr};

  gpio_num_t din_pin_{I2S_GPIO_UNUSED};
  i2s_chan_handle_t rx_handle_;
  bool pdm_{false};
#if SOC_I2S_SUPPORTS_PDM_RX
  i2s_pdm_dsr_t pdm_dsr_{I2S_PDM_DSR_8S};
#endif

  bool correct_dc_offset_;
  bool locked_driver_{false};
  int32_t dc_offset_prev_input_{0};
  int32_t dc_offset_prev_output_{0};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
