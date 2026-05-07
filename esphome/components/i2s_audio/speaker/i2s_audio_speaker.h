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

namespace esphome::i2s_audio {

// Shared constants for I2S audio speaker implementations
static constexpr uint32_t DMA_BUFFER_DURATION_MS = 15;
static constexpr size_t TASK_STACK_SIZE = 4096;
static constexpr ssize_t TASK_PRIORITY = 19;

enum SpeakerEventGroupBits : uint32_t {
  COMMAND_START = (1 << 0),            // indicates loop should start speaker task
  COMMAND_STOP = (1 << 1),             // stops the speaker task
  COMMAND_STOP_GRACEFULLY = (1 << 2),  // Stops the speaker task once all data has been written

  TASK_STARTING = (1 << 10),
  TASK_RUNNING = (1 << 11),
  TASK_STOPPING = (1 << 12),
  TASK_STOPPED = (1 << 13),

  ERR_ESP_NO_MEM = (1 << 19),

  WARN_DROPPED_EVENT = (1 << 20),

  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

/// @brief Abstract base class for I2S audio speaker implementations.
/// Provides shared infrastructure (event groups, ring buffer, volume control, task lifecycle)
/// for derived I2S speaker classes.
class I2SAudioSpeakerBase : public I2SAudioOut, public speaker::Speaker, public Component {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::PROCESSOR; }

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }
  void set_timeout(uint32_t ms) { this->timeout_ = ms; }
  void set_dout_pin(uint8_t pin) { this->dout_pin_ = (gpio_num_t) pin; }

  /// @brief Get the I2S TX channel handle
  i2s_chan_handle_t get_tx_handle() const { return this->tx_handle_; }

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
  /// @brief FreeRTOS task entry point. Casts params to I2SAudioSpeakerBase and calls run_speaker_task_().
  /// @param params I2SAudioSpeakerBase component pointer
  static void speaker_task(void *params);

  /// @brief The main speaker task loop. Implemented by derived classes for mode-specific behavior.
  virtual void run_speaker_task() = 0;

  /// @brief Sends a stop command to the speaker task via ``event_group_``.
  /// @param wait_on_empty If false, sends the COMMAND_STOP signal. If true, sends the COMMAND_STOP_GRACEFULLY signal.
  void stop_(bool wait_on_empty);

  /// @brief Callback function used to send playback timestamps to the speaker task.
  /// @param handle (i2s_chan_handle_t)
  /// @param event (i2s_event_data_t)
  /// @param user_ctx (void*) User context pointer that the callback accesses
  /// @return True if a higher priority task was interrupted
  static bool i2s_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx);

  /// @brief Starts the ESP32 I2S driver. Implemented by derived classes for mode-specific configuration.
  /// @param audio_stream_info Stream information for the I2S driver.
  /// @return ESP_OK if successful, or an error code
  virtual esp_err_t start_i2s_driver(audio::AudioStreamInfo &audio_stream_info) = 0;

  /// @brief Shared I2S channel allocation, initialization, and event queue setup.
  /// Called by derived start_i2s_driver_() implementations after building mode-specific configs.
  /// @param chan_cfg I2S channel configuration
  /// @param std_cfg I2S standard mode configuration (clock, slot, GPIO)
  /// @param event_queue_size Size of the event queue
  /// @return ESP_OK if successful, or an error code. On failure, cleans up channel and unlocks parent.
  esp_err_t init_i2s_channel_(const i2s_chan_config_t &chan_cfg, const i2s_std_config_t &std_cfg,
                              size_t event_queue_size);

  /// @brief Stops the I2S driver and unlocks the I2S port
  void stop_i2s_driver_();

  /// @brief Called in loop() when the task has stopped. Override for mode-specific cleanup.
  virtual void on_task_stopped() {}

  /// @brief Apply software volume control using Q15 fixed-point scaling.
  /// @param data Pointer to audio sample data (modified in place)
  /// @param bytes_read Number of bytes of audio data
  void apply_software_volume_(uint8_t *data, size_t bytes_read);

  /// @brief Swap adjacent 16-bit mono samples for ESP32 (non-variant) hardware quirk.
  /// Only applies when running on original ESP32 with 16-bit mono audio.
  /// @param data Pointer to audio sample data (modified in place)
  /// @param bytes_read Number of bytes of audio data
  void swap_esp32_mono_samples_(uint8_t *data, size_t bytes_read);

  TaskHandle_t speaker_task_handle_{nullptr};
  EventGroupHandle_t event_group_{nullptr};

  QueueHandle_t i2s_event_queue_{nullptr};

  std::weak_ptr<RingBuffer> audio_ring_buffer_;

  uint32_t buffer_duration_ms_;

  optional<uint32_t> timeout_;

  bool pause_state_{false};

  int32_t q31_volume_factor_{INT32_MAX};

  audio::AudioStreamInfo current_stream_info_;  // The currently loaded driver's stream info

  gpio_num_t dout_pin_;
  i2s_chan_handle_t tx_handle_{nullptr};
};

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
