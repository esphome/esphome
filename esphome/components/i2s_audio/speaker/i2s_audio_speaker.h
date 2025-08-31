#pragma once

#ifdef USE_ESP32

#include "../i2s_audio.h"

#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/FreeRTOS.h>

#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace i2s_audio {

class I2SAudioSpeaker : public I2SAudioOut, public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }
  void set_timeout(uint32_t ms) { this->timeout_ = ms; }
#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_DAC
  void set_internal_dac_mode(i2s_dac_mode_t mode) { this->internal_dac_mode_ = mode; }
#endif
  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
  void set_i2s_comm_fmt(i2s_comm_format_t mode) { this->i2s_comm_fmt_ = mode; }
#else
  void set_dout_pin(uint8_t pin) { this->dout_pin_ = (gpio_num_t) pin; }
  void set_i2s_comm_fmt(std::string mode) { this->i2s_comm_fmt_ = std::move(mode); }
#endif

  void start() override;
  void stop() override;
  void finish() override;

  void set_pause_state(bool pause_state) override { this->pause_state_ = pause_state; }
  bool get_pause_state() const override { return this->pause_state_; }

  /// @brief Plays the provided audio data.
  /// Starts the speaker task, if necessary. Writes the audio data to the ring buffer.
  /// @param data Audio data in the format set by the parent speaker classes ``set_audio_stream_info`` method.
  /// @param length The length of the audio data in bytes.
  /// @param ticks_to_wait The FreeRTOS ticks to wait before writing as much data as possible to the ring buffer.
  /// @return The number of bytes that were actually written to the ring buffer.
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override { return play(data, length, 0); }

  bool has_buffered_data() const override;

  /// @brief Sets the volume of the speaker. Uses the speaker's configured audio dac component. If unavailble, it is
  /// implemented as a software volume control. Overrides the default setter to convert the floating point volume to a
  /// Q15 fixed-point factor.
  /// @param volume between 0.0 and 1.0
  void set_volume(float volume) override;

  /// @brief Mutes or unmute the speaker. Uses the speaker's configured audio dac component. If unavailble, it is
  /// implemented as a software volume control. Overrides the default setter to convert the floating point volume to a
  /// Q15 fixed-point factor.
  /// @param mute_state true for muting, false for unmuting
  void set_mute_state(bool mute_state) override;

 protected:
  /// @brief Function for the FreeRTOS task handling audio output.
  /// Allocates space for the buffers, reads audio from the ring buffer and writes audio to the I2S port. Stops
  /// immmiately after receiving the COMMAND_STOP signal and stops only after the ring buffer is empty after receiving
  /// the COMMAND_STOP_GRACEFULLY signal. Stops if the ring buffer hasn't read data for more than timeout_ milliseconds.
  /// When stopping, it deallocates the buffers. It communicates its state and any errors via ``event_group_``.
  /// @param params I2SAudioSpeaker component
  static void speaker_task(void *params);

  /// @brief Sends a stop command to the speaker task via ``event_group_``.
  /// @param wait_on_empty If false, sends the COMMAND_STOP signal. If true, sends the COMMAND_STOP_GRACEFULLY signal.
  void stop_(bool wait_on_empty);

#ifndef USE_I2S_LEGACY
  /// @brief Callback function used to send playback timestamps the to the speaker task.
  /// @param handle (i2s_chan_handle_t)
  /// @param event (i2s_event_data_t)
  /// @param user_ctx (void*) User context pointer that the callback accesses
  /// @return True if a higher priority task was interrupted
  static bool i2s_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);
#endif

  /// @brief Starts the ESP32 I2S driver.
  /// Attempts to lock the I2S port, starts the I2S driver using the passed in stream information, and sets the data out
  /// pin. If it fails, it will unlock the I2S port and uninstalls the driver, if necessary.
  /// @param audio_stream_info Stream information for the I2S driver.
  /// @return ESP_ERR_NOT_ALLOWED if the I2S port can't play the incoming audio stream.
  ///         ESP_ERR_INVALID_STATE if the I2S port is already locked.
  ///         ESP_ERR_INVALID_ARG if installing the driver or setting the data outpin fails due to a parameter error.
  ///         ESP_ERR_NO_MEM if the driver fails to install due to a memory allocation error.
  ///         ESP_FAIL if setting the data out pin fails due to an IO error
  ///         ESP_OK if successful
  esp_err_t start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info);

  /// @brief Stops the I2S driver and unlocks the I2S port
  void stop_i2s_driver_();

  TaskHandle_t speaker_task_handle_{nullptr};
  EventGroupHandle_t event_group_{nullptr};

  QueueHandle_t i2s_event_queue_;

  std::weak_ptr<RingBuffer> audio_ring_buffer_;

  uint32_t buffer_duration_ms_;

  optional<uint32_t> timeout_;

  bool pause_state_{false};

  int16_t q15_volume_factor_{INT16_MAX};

  audio::AudioStreamInfo current_stream_info_;  // The currently loaded driver's stream info

#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_DAC
  i2s_dac_mode_t internal_dac_mode_{I2S_DAC_CHANNEL_DISABLE};
#endif
  uint8_t dout_pin_;
  i2s_comm_format_t i2s_comm_fmt_;
#else
  gpio_num_t dout_pin_;
  std::string i2s_comm_fmt_;
  i2s_chan_handle_t tx_handle_;
#endif
};

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
