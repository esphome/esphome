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

#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  void build_full_duplex_config(i2s_std_config_t &std_cfg) override;
  void full_duplex_rx_enabling(i2s_chan_handle_t rx_handle, uint32_t dma_desc_num) override;
#endif

 protected:
  /// @brief Builds the standard (non-PDM) mode configuration from the configured settings.
  void build_std_config_(i2s_std_config_t &std_cfg) const;

  /// @brief Starts the I2S driver. Updates the ``audio_stream_info_`` member variable with the current setttings.
  /// @return True if succesful, false otherwise
  bool start_driver_();

  /// @brief Stops the I2S driver.
  void stop_driver_();

  /// @brief Registers the DMA callbacks on a disabled RX channel and restarts the capture time byte counts.
  /// @return True if successful, false otherwise
  bool start_capture_count_(i2s_chan_handle_t rx_handle, uint32_t dma_desc_num);

  /// @brief Reads the size of one DMA buffer from a started RX channel.
  /// @return True if successful, false otherwise
  bool read_dma_buffer_size_();

  /// @brief Attempts to correct a microphone DC offset; e.g., a microphones silent level is offset from 0. Applies a
  /// correction offset that is updated using an exponential moving average for all samples away from 0.
  /// @param data
  void fix_dc_offset_(std::vector<uint8_t> &data);

  size_t read_(uint8_t *buf, size_t len, uint32_t timeout_ms);

  /// @brief Sets the Microphone ``audio_stream_info_`` member variable to the configured I2S settings.
  void configure_stream_settings_();

  static void mic_task(void *params);

  /// @brief Records the byte count and time of each DMA buffer the driver queues for reading.
  static bool i2s_on_recv_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
  /// @brief Removes a buffer the driver dropped because the task fell behind; it will never be read.
  static bool i2s_on_recv_q_ovf_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

  SemaphoreHandle_t active_listeners_semaphore_{nullptr};
  EventGroupHandle_t event_group_{nullptr};

  TaskHandle_t task_handle_{nullptr};

  // Written by the DMA interrupt and read by the mic task, under dma_lock_. Bytes queued for reading since the
  // channel was enabled (minus dropped buffers, wrapping), and the time the newest buffer was queued.
  portMUX_TYPE dma_lock_ = portMUX_INITIALIZER_UNLOCKED;
  uint32_t dma_bytes_received_{0};
  int64_t dma_receive_time_us_{0};

  // Bytes the mic task has read since the channel was enabled, wrapping. A full duplex channel can stay enabled
  // across microphone sessions, so this outlives the task.
  uint32_t dma_bytes_read_{0};

  // DMA layout of the RX channel, set when the driver starts
  size_t dma_buffer_bytes_{0};
  uint32_t dma_desc_num_{0};

  gpio_num_t din_pin_{I2S_GPIO_UNUSED};
  i2s_chan_handle_t rx_handle_{nullptr};
#if SOC_I2S_SUPPORTS_PDM_RX
  i2s_pdm_dsr_t pdm_dsr_{I2S_PDM_DSR_8S};
#endif

  bool pdm_{false};
  bool correct_dc_offset_;
  bool locked_driver_{false};
  int32_t dc_offset_prev_input_{0};
  int32_t dc_offset_prev_output_{0};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
