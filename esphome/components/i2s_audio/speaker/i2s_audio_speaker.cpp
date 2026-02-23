#include "i2s_audio_speaker.h"

#ifdef USE_ESP32

#ifdef USE_I2S_LEGACY
#include <driver/i2s.h>
#else
#include <driver/i2s_std.h>
#endif  // USE_I2S_LEGACY

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

namespace esphome::i2s_audio {

static constexpr uint32_t DMA_BUFFER_DURATION_MS = 15;
static constexpr size_t DMA_BUFFERS_COUNT = 4;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
// SPDIF mode adds overhead as each sample is encapsulated in a subframe;
// each DMA buffer can hold only 192 samples (~4ms each vs. ~15ms for standard I2S).
// To match the standard I2S buffering duration, we use more buffers to minimize
// the impact of the overhead, such as stuttering or audio/silence oscillation.
// 15 buffers × 4ms = 60ms of DMA buffering (same as 4 × 15ms for standard)
static constexpr size_t SPDIF_DMA_BUFFERS_COUNT = 15;

// Sync offset to compensate for SPDIF preload latency.
// The preload mechanism adds delay compared to standard I2S.
// This value is added to timestamps reported to upstream sync algorithms.
// Adjust this value to fine-tune synchronization with other players.
static constexpr int64_t SPDIF_SYNC_OFFSET_US = 75 * 1000;  // 75ms in microseconds

// Duration to wait after preload before entering "silence mode".
// Allows bursty data delivery to settle without causing audio/silence oscillation.
static constexpr uint32_t SPDIF_GRACE_PERIOD_MS = 500;

// Duration of continuous silence before faking a stop to unblock the upstream pipeline.
// Long enough to avoid false triggers but short enough to be responsive.
static constexpr uint32_t SPDIF_FAKE_STOP_DELAY_MS = 500;

// Duration to wait during preload while buffers fill up.
// Audio data accumulates during this time before playback starts.
static constexpr uint32_t SPDIF_PRELOAD_MS = 100;

// Timeout for flushing pending frames if no callback received.
static constexpr uint32_t SPDIF_FLUSH_TIMEOUT_MS = 20;

// Number of DMA events between upstream callbacks (~16ms = 4 events × 4ms each).
// Matches non-SPDIF timing to prevent overwhelming upstream sync algorithms.
static constexpr uint32_t SPDIF_DMA_EVENTS_PER_CALLBACK = 4;
#endif

static const size_t TASK_STACK_SIZE = 4096;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
// Static silence buffer for SPDIF continuous mode
// 192 samples * 2 channels * 2 bytes per sample = 768 bytes
// Stored in flash (.rodata section) to avoid stack/heap usage
static const int16_t SPDIF_SILENCE_BUFFER[SPDIF_BLOCK_SAMPLES * 2] = {0};
#endif  // USE_I2S_AUDIO_SPDIF_MODE
static const ssize_t TASK_PRIORITY = 19;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
static const size_t SPDIF_I2S_EVENT_QUEUE_COUNT = SPDIF_DMA_BUFFERS_COUNT + 1;
#endif
static const size_t I2S_EVENT_QUEUE_COUNT = DMA_BUFFERS_COUNT + 1;

static const char *const TAG = "i2s_audio.speaker";

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

// Lists the Q15 fixed point scaling factor for volume reduction.
// Has 100 values representing silence and a reduction [49, 48.5, ... 0.5, 0] dB.
// dB to PCM scaling factor formula: floating_point_scale_factor = 2^(-db/6.014)
// float to Q15 fixed point formula: q15_scale_factor = floating_point_scale_factor * 2^(15)
static const std::vector<int16_t> Q15_VOLUME_SCALING_FACTORS = {
    0,     116,   122,   130,   137,   146,   154,   163,   173,   183,   194,   206,   218,   231,   244,
    259,   274,   291,   308,   326,   345,   366,   388,   411,   435,   461,   488,   517,   548,   580,
    615,   651,   690,   731,   774,   820,   868,   920,   974,   1032,  1094,  1158,  1227,  1300,  1377,
    1459,  1545,  1637,  1734,  1837,  1946,  2061,  2184,  2313,  2450,  2596,  2750,  2913,  3085,  3269,
    3462,  3668,  3885,  4116,  4360,  4619,  4893,  5183,  5490,  5816,  6161,  6527,  6914,  7324,  7758,
    8218,  8706,  9222,  9770,  10349, 10963, 11613, 12302, 13032, 13805, 14624, 15491, 16410, 17384, 18415,
    19508, 20665, 21891, 23189, 24565, 26022, 27566, 29201, 30933, 32767};

#ifdef USE_I2S_AUDIO_SPDIF_MODE
// Static callback functions for SPDIF encoder (avoids std::function overhead)
#ifdef USE_I2S_LEGACY
static esp_err_t spdif_write_cb(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait) {
  auto *speaker = static_cast<I2SAudioSpeaker *>(user_ctx);
  size_t bytes_written = 0;
  esp_err_t err = i2s_write(speaker->get_parent()->get_port(), data, size, &bytes_written, ticks_to_wait);
  // Only log errors that aren't expected. ESP_ERR_TIMEOUT with 0 timeout is normal (DMA full).
  if (err != ESP_OK && (err != ESP_ERR_TIMEOUT || ticks_to_wait != 0)) {
    ESP_LOGW(TAG, "SPDIF I2S write failed: %s (wrote %zu/%zu bytes)", esp_err_to_name(err), bytes_written, size);
  }
  return err;
}
#else
static esp_err_t spdif_preload_cb(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait) {
  auto *speaker = static_cast<I2SAudioSpeaker *>(user_ctx);
  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_preload_data(speaker->get_tx_handle(), data, size, &bytes_written);
  if (err != ESP_OK || bytes_written != size) {
    ESP_LOGW(TAG, "SPDIF preload failed: %s (wrote %zu/%zu bytes)", esp_err_to_name(err), bytes_written, size);
    return (err != ESP_OK) ? err : ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

static esp_err_t spdif_write_cb(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait) {
  auto *speaker = static_cast<I2SAudioSpeaker *>(user_ctx);
  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(speaker->get_tx_handle(), data, size, &bytes_written, ticks_to_wait);
  // Only log errors that aren't expected. ESP_ERR_TIMEOUT with 0 timeout is normal (DMA full).
  if (err != ESP_OK && !(err == ESP_ERR_TIMEOUT && ticks_to_wait == 0)) {
    ESP_LOGW(TAG, "SPDIF I2S write failed: %s (wrote %zu/%zu bytes)", esp_err_to_name(err), bytes_written, size);
  }
  return err;
}
#endif  // USE_I2S_LEGACY
#endif  // USE_I2S_AUDIO_SPDIF_MODE

void I2SAudioSpeaker::setup() {
  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  if (this->spdif_mode_) {
    this->spdif_encoder_ = new SPDIFEncoder();
    if (!this->spdif_encoder_->setup()) {
      ESP_LOGE(TAG, "Failed to setup SPDIF encoder");
      this->mark_failed();
      return;
    }

    // Configure channel status block with the sample rate
    this->spdif_encoder_->set_sample_rate(this->sample_rate_);

#ifdef USE_I2S_LEGACY
    // Legacy driver: use a single write callback
    this->spdif_encoder_->set_write_callback(spdif_write_cb, this);
#else
    // New driver: separate callbacks for preload (during underflow recovery) and normal writes
    this->spdif_encoder_->set_preload_callback(spdif_preload_cb, this);
    this->spdif_encoder_->set_write_callback(spdif_write_cb, this);
#endif  // USE_I2S_LEGACY
  }
#endif  // USE_I2S_AUDIO_SPDIF_MODE

  // Initialize volume control. When audio_dac is configured, this sets the DAC volume.
  // When no audio_dac is configured, this initializes software volume control, which is
  // especially important for SPDIF mode.
  this->set_volume(this->volume_);
}

void I2SAudioSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Speaker:\n"
                "  Pin: %d\n"
                "  Buffer duration: %" PRIu32,
                static_cast<int8_t>(this->dout_pin_), this->buffer_duration_ms_);
  if (this->timeout_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 " ms", this->timeout_.value());
  }
#ifdef USE_I2S_AUDIO_SPDIF_MODE
  if (this->spdif_mode_) {
    ESP_LOGCONFIG(TAG,
                  "  SPDIF Mode: %s\n"
                  "  Sample Rate: %" PRIu32 " Hz",
                  YESNO(this->spdif_mode_), this->sample_rate_);
  } else
#endif  // USE_I2S_AUDIO_SPDIF_MODE
  {
#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_DAC
    ESP_LOGCONFIG(TAG, "  Internal DAC mode: %d", static_cast<int8_t>(this->internal_dac_mode_));
#endif  // SOC_I2S_SUPPORTS_DAC
    ESP_LOGCONFIG(TAG, "  Communication format: %d", static_cast<int8_t>(this->i2s_comm_fmt_));
#else
    ESP_LOGCONFIG(TAG, "  Communication format: %s", this->i2s_comm_fmt_.c_str());
#endif  // USE_I2S_LEGACY
  }
}

void I2SAudioSpeaker::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if ((event_group_bits & SpeakerEventGroupBits::COMMAND_START) && (this->state_ == speaker::STATE_STOPPED)) {
#ifdef USE_I2S_AUDIO_SPDIF_MODE
    // In SPDIF fake-stopped mode, the task is still running - just restore state
    if (this->spdif_fake_stopped_) {
      ESP_LOGV(TAG, "SPDIF: Restoring from fake stop (speaker was running in background)");
      this->state_ = speaker::STATE_RUNNING;
      // DON'T clear spdif_fake_stopped_ here - let speaker_task handle it with preload
      // Set silence_start to NOW to trigger preload countdown in speaker_task
      this->spdif_silence_start_ = millis();
      xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
    } else
#endif
    {
      this->state_ = speaker::STATE_STARTING;
      xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
    }
  }

  // Handle the task's state
  if (event_group_bits & SpeakerEventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Starting");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_STARTING);
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_RUNNING) {
    ESP_LOGD(TAG, "Started");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);
    this->state_ = speaker::STATE_RUNNING;
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_STOPPING) {
    ESP_LOGD(TAG, "Stopping");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPING);
    this->state_ = speaker::STATE_STOPPING;
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_STOPPED) {
    ESP_LOGD(TAG, "Stopped");

    vTaskDelete(this->speaker_task_handle_);
    this->speaker_task_handle_ = nullptr;

    this->stop_i2s_driver_();
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ALL_BITS);
    this->status_clear_error();

#ifdef USE_I2S_AUDIO_SPDIF_MODE
    // Clear fake-stopped flag since task has actually stopped
    // This prevents "restore from fake stop" when task is truly stopped
    this->spdif_fake_stopped_ = false;
    this->spdif_needs_preload_ = true;
    this->spdif_silence_start_ = 0;
    this->spdif_preload_ended_ = 0;
#endif

    this->state_ = speaker::STATE_STOPPED;
  }

  // Log any errors encounted by the task
  if (event_group_bits & SpeakerEventGroupBits::ERR_ESP_NO_MEM) {
    ESP_LOGE(TAG, "Not enough memory");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  }

  // Warn if any playback timestamp events are dropped, which drastically reduces synced playback accuracy
  if (event_group_bits & SpeakerEventGroupBits::WARN_DROPPED_EVENT) {
    ESP_LOGW(TAG, "Event dropped, synchronized playback accuracy is reduced");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::WARN_DROPPED_EVENT);
  }

  // Handle the speaker's state
  switch (this->state_) {
    case speaker::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }

      if (this->start_i2s_driver_(this->audio_stream_info_) != ESP_OK) {
        ESP_LOGE(TAG, "Driver failed to start; retrying in 1 second");
        this->status_momentary_error("driver-failure", 1000);
        break;
      }

      if (this->speaker_task_handle_ == nullptr) {
        xTaskCreate(I2SAudioSpeaker::speaker_task, "speaker_task", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY,
                    &this->speaker_task_handle_);

        if (this->speaker_task_handle_ == nullptr) {
          ESP_LOGE(TAG, "Task failed to start, retrying in 1 second");
          this->status_momentary_error("task-failure", 1000);
          this->stop_i2s_driver_();  // Stops the driver to return the lock; will be reloaded in next attempt
        }
      }
      break;
    case speaker::STATE_RUNNING:   // Intentional fallthrough
    case speaker::STATE_STOPPING:  // Intentional fallthrough
    case speaker::STATE_STOPPED:
      break;
  }
}

void I2SAudioSpeaker::set_volume(float volume) {
  this->volume_ = volume;
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_ != nullptr) {
    if (volume > 0.0) {
      this->audio_dac_->set_mute_off();
    }
    this->audio_dac_->set_volume(volume);
  } else
#endif  // USE_AUDIO_DAC
  {
    // Fallback to software volume control by using a Q15 fixed point scaling factor.
    // At maximum volume (1.0), set to INT16_MAX to completely bypass volume processing
    // and avoid any floating-point precision issues that could cause slight volume reduction.
    if (volume >= 1.0f) {
      this->q15_volume_factor_ = INT16_MAX;
    } else {
      ssize_t decibel_index = remap<ssize_t, float>(volume, 0.0f, 1.0f, 0, Q15_VOLUME_SCALING_FACTORS.size() - 1);
      this->q15_volume_factor_ = Q15_VOLUME_SCALING_FACTORS[decibel_index];
    }
  }
}

void I2SAudioSpeaker::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_) {
    if (mute_state) {
      this->audio_dac_->set_mute_on();
    } else {
      this->audio_dac_->set_mute_off();
    }
  } else
#endif  // USE_AUDIO_DAC
  {
    if (mute_state) {
      // Fallback to software volume control and scale by 0
      this->q15_volume_factor_ = 0;
    } else {
      // Revert to previous volume when unmuting
      this->set_volume(this->volume_);
    }
  }
}

size_t I2SAudioSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed; cannot play audio");
    return 0;
  }
  if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
    this->start();
  }

  if (this->state_ != speaker::STATE_RUNNING) {
    // Unable to write data to a running speaker, so delay the max amount of time so it can get ready
    vTaskDelay(ticks_to_wait);
    ticks_to_wait = 0;
  }

  size_t bytes_written = 0;
  if (this->state_ == speaker::STATE_RUNNING) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
    if (temp_ring_buffer.use_count() == 2) {
      // Only the speaker task and this temp_ring_buffer own the ring buffer, so its safe to write to
      bytes_written = temp_ring_buffer->write_without_replacement((void *) data, length, ticks_to_wait);
    }
  }

  return bytes_written;
}

bool I2SAudioSpeaker::has_buffered_data() const {
  if (this->audio_ring_buffer_.use_count() > 0) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
    return temp_ring_buffer->available() > 0;
  }
  return false;
}

void I2SAudioSpeaker::speaker_task(void *params) {
  I2SAudioSpeaker *this_speaker = (I2SAudioSpeaker *) params;

  xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::TASK_STARTING);

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  // Reset SPDIF encoder at task start to ensure clean state
  // (previous task may have left stale data in encoder buffer)
  if (this_speaker->spdif_mode_ && this_speaker->spdif_encoder_ != nullptr) {
    this_speaker->spdif_encoder_->reset();
  }
#endif  // USE_I2S_AUDIO_SPDIF_MODE

  const uint32_t dma_buffers_duration_ms = DMA_BUFFER_DURATION_MS * DMA_BUFFERS_COUNT;
  // Ensure ring buffer duration is at least the duration of all DMA buffers
  const uint32_t ring_buffer_duration = std::max(dma_buffers_duration_ms, this_speaker->buffer_duration_ms_);

  // The DMA buffers may have more bits per sample, so calculate buffer sizes based in the input audio stream info
  const size_t ring_buffer_size = this_speaker->current_stream_info_.ms_to_bytes(ring_buffer_duration);

  // For SPDIF mode, one DMA buffer = one SPDIF block = 192 PCM frames
  // For standard I2S mode, calculate based on duration
#ifdef USE_I2S_AUDIO_SPDIF_MODE
  const uint32_t frames_to_fill_single_dma_buffer =
      this_speaker->spdif_mode_ ? SPDIF_BLOCK_SAMPLES
                                : this_speaker->current_stream_info_.ms_to_frames(DMA_BUFFER_DURATION_MS);
#else
  const uint32_t frames_to_fill_single_dma_buffer =
      this_speaker->current_stream_info_.ms_to_frames(DMA_BUFFER_DURATION_MS);
#endif  // USE_I2S_AUDIO_SPDIF_MODE
  const size_t bytes_to_fill_single_dma_buffer =
      this_speaker->current_stream_info_.frames_to_bytes(frames_to_fill_single_dma_buffer);

  bool successful_setup = false;
  std::unique_ptr<audio::AudioSourceTransferBuffer> transfer_buffer =
      audio::AudioSourceTransferBuffer::create(bytes_to_fill_single_dma_buffer);

  if (transfer_buffer != nullptr) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = RingBuffer::create(ring_buffer_size);
    if (temp_ring_buffer.use_count() == 1) {
      transfer_buffer->set_source(temp_ring_buffer);
      this_speaker->audio_ring_buffer_ = temp_ring_buffer;
      successful_setup = true;
    }
  }

  if (!successful_setup) {
    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  } else {
    bool stop_gracefully = false;
    bool tx_dma_underflow = true;

    uint32_t frames_written = 0;
    uint32_t last_data_received_time = millis();

#if !defined(USE_I2S_LEGACY) && defined(USE_I2S_AUDIO_SPDIF_MODE)
    // SPDIF Continuous Silence Mode + Callback Decimation
    //
    // Key principles:
    // 1. NEVER stop the I2S channel - always output a valid SPDIF stream
    // 2. When no audio data, output silence-encoded SPDIF blocks (not zeros!)
    // 3. Fire callbacks every 4 DMA events (~16ms), matching non-SPDIF timing
    //
    // This eliminates gaps that cause SPDIF receivers to re-sync, and reduces
    // callback rate to prevent overwhelming upstream sync algorithms.
    const uint32_t spdif_callback_threshold =
        this_speaker->spdif_mode_ ? this_speaker->current_stream_info_.ms_to_frames(DMA_BUFFER_DURATION_MS) : 0;
    uint32_t spdif_pending_frames = 0;
    int64_t spdif_pending_timestamp = 0;
    uint32_t spdif_last_callback_time = millis();
    // Count DMA events for decimation
    uint32_t spdif_dma_event_count = 0;
#endif  // !USE_I2S_LEGACY && USE_I2S_AUDIO_SPDIF_MODE

    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::TASK_RUNNING);

    // Main speaker task loop. Continues while:
    // - Paused, OR
    // - No timeout configured, OR
    // - SPDIF continuous mode (never timeout, always output valid stream), OR
    // - Timeout hasn't elapsed since last data
#ifdef USE_I2S_AUDIO_SPDIF_MODE
    // SPDIF continuous mode outputs silence when no audio data to keep receiver synced.
    // The timeout_ controls how long to output silence before stopping.
    const bool spdif_continuous_mode = this_speaker->spdif_mode_;
#else
    const bool spdif_continuous_mode = false;
#endif

    while (this_speaker->pause_state_ || !this_speaker->timeout_.has_value() || spdif_continuous_mode ||
           (millis() - last_data_received_time) <= this_speaker->timeout_.value()) {
      uint32_t event_group_bits = xEventGroupGetBits(this_speaker->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        xEventGroupClearBits(this_speaker->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
#ifdef USE_I2S_AUDIO_SPDIF_MODE
        // In SPDIF continuous mode, NEVER break on COMMAND_STOP.
        // We fake the stop to unblock the external pipeline, but keep outputting.
        // The buffer will drain naturally, then we enter silence mode.
        if (spdif_continuous_mode) {
          if (!this_speaker->spdif_fake_stopped_) {
            ESP_LOGV(TAG, "SPDIF: Faking stop to unblock pipeline (speaker continues in background)");
            // Set state to STOPPED to unblock external pipeline waiting for us to stop
            this_speaker->state_ = speaker::STATE_STOPPED;
            this_speaker->spdif_fake_stopped_ = true;
            this_speaker->spdif_needs_preload_ = true;  // Next audio will need preload
            this_speaker->spdif_preload_ended_ = 0;     // Reset grace period
          }
          // Don't break - keep the speaker running
          // Buffer will drain, then we output silence until new audio arrives
        } else
#endif
        {
          ESP_LOGV(TAG, "Exiting: COMMAND_STOP received");
          break;
        }
      }
      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY) {
        xEventGroupClearBits(this_speaker->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
        stop_gracefully = true;
      }

      if (this_speaker->audio_stream_info_ != this_speaker->current_stream_info_) {
        // Audio stream info changed, stop the speaker task so it will restart with the proper settings.
        ESP_LOGV(TAG, "Exiting: stream info changed");
        break;
      }
#ifdef USE_I2S_LEGACY
      i2s_event_t i2s_event;
      while (xQueueReceive(this_speaker->i2s_event_queue_, &i2s_event, 0)) {
        if (i2s_event.type == I2S_EVENT_TX_Q_OVF) {
          tx_dma_underflow = true;
#ifdef USE_I2S_AUDIO_SPDIF_MODE
          // Fill with silence when buffer underflows to prevent receiver from detecting source change
          if (spdif_continuous_mode) {
            this_speaker->spdif_encoder_->reset();
            this_speaker->spdif_encoder_->write(reinterpret_cast<const uint8_t *>(SPDIF_SILENCE_BUFFER),
                                                sizeof(SPDIF_SILENCE_BUFFER), 0);
          }
#endif  // USE_I2S_AUDIO_SPDIF_MODE
        }
      }
#else
      int64_t write_timestamp;
      while (xQueueReceive(this_speaker->i2s_event_queue_, &write_timestamp, 0)) {
        // Receives timing events from the I2S on_sent callback. If actual audio data was sent in this event, it passes
        // on the timing info via the audio_output_callback.
        uint32_t frames_sent = frames_to_fill_single_dma_buffer;
        if (frames_to_fill_single_dma_buffer > frames_written) {
          tx_dma_underflow = true;
          frames_sent = frames_written;
          const uint32_t frames_zeroed = frames_to_fill_single_dma_buffer - frames_written;
          write_timestamp -= this_speaker->current_stream_info_.frames_to_microseconds(frames_zeroed);
        } else {
          tx_dma_underflow = false;
        }
        frames_written -= frames_sent;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
        if (this_speaker->spdif_mode_ && spdif_callback_threshold > 0) {
          // SPDIF Callback Decimation: fire every 4th DMA event (~16ms)
          // This matches non-SPDIF timing and prevents overwhelming upstream.
          spdif_dma_event_count++;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          // Verbose: log first DMA event
          static bool first_dma_event_logged = false;
          if (!first_dma_event_logged) {
            ESP_LOGV(TAG, "SPDIF: First DMA event, frames_sent=%" PRIu32, frames_sent);
            first_dma_event_logged = true;
          }
#endif

          // Accumulate frames and timestamp
          if (frames_sent > 0) {
            if (spdif_pending_frames == 0) {
              spdif_pending_timestamp = write_timestamp;
            }
            spdif_pending_frames += frames_sent;
          }

          // Fire callback every 4 DMA events, or on timeout if we have pending frames
          bool decimation_reached = (spdif_dma_event_count >= SPDIF_DMA_EVENTS_PER_CALLBACK);
          bool timeout_flush =
              (spdif_pending_frames > 0) && ((millis() - spdif_last_callback_time) >= SPDIF_FLUSH_TIMEOUT_MS);

          if (decimation_reached || timeout_flush) {
            if (spdif_pending_frames > 0) {
              // Apply SPDIF sync offset (defined at top of file)
              int64_t adjusted_timestamp = spdif_pending_timestamp + SPDIF_SYNC_OFFSET_US;
              this_speaker->audio_output_callback_(spdif_pending_frames, adjusted_timestamp);
              spdif_pending_frames = 0;
              spdif_last_callback_time = millis();
            }
            spdif_dma_event_count = 0;  // Reset decimation counter
          }
        } else
#endif  // USE_I2S_AUDIO_SPDIF_MODE
        {
          // Standard I2S mode: fire callback immediately for each event
          if (frames_sent > 0) {
            this_speaker->audio_output_callback_(frames_sent, write_timestamp);
          }
        }
      }
#endif  // USE_I2S_LEGACY

      if (this_speaker->pause_state_) {
        // Pause state is accessed atomically, so thread safe
        // Delay so the task yields, then skip transferring audio data
        vTaskDelay(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
        continue;
      }

      // Wait half the duration of the data already written to the DMA buffers for new audio data
      // The millisecond helper modifies the frames_written variable, so use the microsecond helper and divide by 1000
      uint32_t read_delay = (this_speaker->current_stream_info_.frames_to_microseconds(frames_written) / 1000) / 2;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
      // In SPDIF mode, if transfer buffer is empty (we're pumping silence), use a very short timeout.
      // This ensures we can pump silence fast enough to keep the DMA fed (~250 blocks/sec needed).
      // Otherwise the long timeout based on frames_written causes DMA to run dry.
      if (this_speaker->spdif_mode_ && transfer_buffer->available() == 0) {
        read_delay = 1;  // 1ms - just check for new data, don't wait long
      }
#endif

      size_t bytes_read = transfer_buffer->transfer_data_from_source(pdMS_TO_TICKS(read_delay));
      uint8_t *new_data = transfer_buffer->get_buffer_end() - bytes_read;

      if (bytes_read > 0) {
        if (this_speaker->q15_volume_factor_ < INT16_MAX) {
          // Apply the software volume adjustment by unpacking the sample into a Q31 fixed-point number, shifting it,
          // multiplying by the volume factor, and packing the sample back into the original bytes per sample.

          const size_t bytes_per_sample = this_speaker->current_stream_info_.samples_to_bytes(1);
          const uint32_t len = bytes_read / bytes_per_sample;

          // Use Q16 for samples with 1 or 2 bytes: shifted_sample * gain_factor is Q16 * Q15 -> Q31
          int32_t shift = 15;                                      // Q31 -> Q16
          int32_t gain_factor = this_speaker->q15_volume_factor_;  // Q15

          if (bytes_per_sample >= 3) {
            // Use Q23 for samples with 3 or 4 bytes: shifted_sample * gain_factor is Q23 * Q8 -> Q31

            shift = 8;          // Q31 -> Q23
            gain_factor >>= 7;  // Q15 -> Q8
          }

          for (uint32_t i = 0; i < len; ++i) {
            int32_t sample =
                audio::unpack_audio_sample_to_q31(&new_data[i * bytes_per_sample], bytes_per_sample);  // Q31
            sample >>= shift;
            sample *= gain_factor;  // Q31
            audio::pack_q31_as_audio_sample(sample, &new_data[i * bytes_per_sample], bytes_per_sample);
          }
        }

#ifdef USE_ESP32_VARIANT_ESP32
        // For ESP32 8/16 bit mono mode samples need to be switched.
        if (this_speaker->current_stream_info_.get_channels() == 1 &&
            this_speaker->current_stream_info_.get_bits_per_sample() <= 16) {
          size_t len = bytes_read / sizeof(int16_t);
          int16_t *tmp_buf = (int16_t *) new_data;
          for (size_t i = 0; i < len; i += 2) {
            int16_t tmp = tmp_buf[i];
            tmp_buf[i] = tmp_buf[i + 1];
            tmp_buf[i + 1] = tmp;
          }
        }
#endif  // USE_ESP32_VARIANT_ESP32
      }

      if (transfer_buffer->available() == 0) {
#ifdef USE_I2S_AUDIO_SPDIF_MODE
        // SPDIF Continuous Silence Mode: always output valid SPDIF stream
        // When no audio data, write silence-encoded blocks to keep receiver happy
        if (spdif_continuous_mode && this_speaker->spdif_encoder_ != nullptr) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          // Verbose: Track silence path entry (rate limited)
          static uint32_t silence_path_log_time = 0;
          static uint32_t silence_path_iterations = 0;
          static uint32_t silence_blocks_written = 0;
          static uint32_t silence_write_attempts = 0;
          static bool first_silence_entry = true;
          silence_path_iterations++;

          if (first_silence_entry) {
            ESP_LOGV(TAG, "SPDIF: Entered silence path (no audio data), stop_gracefully=%d", stop_gracefully);
            first_silence_entry = false;
          }
#endif

          // CRITICAL: Update last_data_received_time unconditionally in SPDIF silence mode
          // This prevents the speaker task timeout from firing. Outputting silence is active
          // operation, not idleness - we're keeping the SPDIF receiver synced.
          last_data_received_time = millis();

          // Grace period: After preload completes, don't enter "silence mode" for a while.
          // This allows bursty data delivery to settle without causing audio/silence oscillation.
          // We still write silence to DMA, but we don't track it as a prolonged silence event.
          bool in_grace_period = (this_speaker->spdif_preload_ended_ != 0) &&
                                 (millis() - this_speaker->spdif_preload_ended_ < SPDIF_GRACE_PERIOD_MS);

          if (!in_grace_period) {
            // Track when we entered silence mode (only after cooldowns)
            if (this_speaker->spdif_silence_start_ == 0) {
              this_speaker->spdif_silence_start_ = millis();
              // Note: We do NOT set spdif_needs_preload_ here because brief gaps during
              // normal playback don't need preload - the DMA is already primed.
              // Preload is only needed for: initial startup, or after fake-stop (500ms+ silence)
            }

            // Only fake the stop after being in silence mode for a minimum duration.
            // This prevents oscillation during normal playback when buffer is briefly empty.
            if (!this_speaker->spdif_fake_stopped_ &&
                (millis() - this_speaker->spdif_silence_start_ >= SPDIF_FAKE_STOP_DELAY_MS)) {
              ESP_LOGV(TAG, "SPDIF: Silence mode for %" PRIu32 "ms - faking stop to unblock pipeline",
                       millis() - this_speaker->spdif_silence_start_);
              this_speaker->state_ = speaker::STATE_STOPPED;
              this_speaker->spdif_fake_stopped_ = true;
              this_speaker->spdif_needs_preload_ = true;  // Next audio will need preload
              this_speaker->spdif_preload_ended_ = 0;     // Reset grace period
            }

            // Check if timeout has been exceeded (if not set to "never")
            // timeout_.has_value() == false means "never" timeout (keep filling silence forever)
            // timeout_.has_value() == true means stop after that duration of silence
            if (this_speaker->timeout_.has_value()) {
              uint32_t silence_duration = millis() - this_speaker->spdif_silence_start_;
              if (silence_duration >= this_speaker->timeout_.value()) {
                ESP_LOGV(TAG, "SPDIF: Silence timeout reached (%" PRIu32 "ms) - stopping speaker", silence_duration);
                // Clear fake-stopped since we're actually stopping
                this_speaker->spdif_fake_stopped_ = false;
                break;  // Exit the loop and actually stop the speaker
              }
            }
          }

          // First flush any partial block with silence padding (non-blocking to avoid getting stuck)
          if (this_speaker->spdif_encoder_->has_pending_data()) {
            this_speaker->spdif_encoder_->flush_with_silence(0);  // Non-blocking
          }

          // CRITICAL: In SPDIF continuous mode, ALWAYS write silence when no audio data.
          // We don't check tx_dma_underflow because:
          // 1. When DMA runs empty, callbacks stop, so tx_dma_underflow doesn't update
          // 2. The non-blocking write handles "DMA full" gracefully (just doesn't write)
          // 3. We need continuous output to prevent receiver from losing sync
          if (!stop_gracefully) {
            uint32_t silence_blocks = 0;
            esp_err_t err = this_speaker->spdif_encoder_->write(reinterpret_cast<const uint8_t *>(SPDIF_SILENCE_BUFFER),
                                                                sizeof(SPDIF_SILENCE_BUFFER), 0,
                                                                &silence_blocks);  // Non-blocking!
            // Don't count silence as frames_written - it's not real audio

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
            silence_write_attempts++;
            silence_blocks_written += silence_blocks;

            // Verbose: log silence write result on first attempt
            static bool first_write_logged = false;
            if (!first_write_logged) {
              ESP_LOGV(TAG, "SPDIF: First silence write - err=%s, blocks=%lu", esp_err_to_name(err),
                       (unsigned long) silence_blocks);
              first_write_logged = true;
            }
#else
            (void) err;  // Suppress unused variable warning
#endif
          }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          // Verbose: Log silence path stats every second
          if (millis() - silence_path_log_time >= 1000) {
            ESP_LOGV(TAG,
                     "SPDIF: Silence stats - iterations=%" PRIu32 "/sec, write_attempts=%" PRIu32
                     ", blocks_written=%" PRIu32 ", grace=%d, stop_gracefully=%d",
                     silence_path_iterations, silence_write_attempts, silence_blocks_written, in_grace_period,
                     stop_gracefully);
            silence_path_iterations = 0;
            silence_write_attempts = 0;
            silence_blocks_written = 0;
            silence_path_log_time = millis();
          }
#endif
        } else {
          // Debug: Log if we're NOT in SPDIF silence mode when we should be
          static bool non_spdif_logged = false;
          if (!non_spdif_logged) {
            ESP_LOGW(TAG, "SPDIF: Silence path but spdif_mode_=%d, encoder=%p", this_speaker->spdif_mode_,
                     (void *) this_speaker->spdif_encoder_);
            non_spdif_logged = true;
          }
        }
#endif  // USE_I2S_AUDIO_SPDIF_MODE
        if (stop_gracefully && tx_dma_underflow) {
#ifdef USE_I2S_AUDIO_SPDIF_MODE
          // In SPDIF continuous mode, don't break on graceful stop during silence
          // Keep outputting silence until new audio arrives or explicit COMMAND_STOP
          if (!spdif_continuous_mode)
#endif
          {
            break;
          }
        }
#ifdef USE_I2S_AUDIO_SPDIF_MODE
        // In SPDIF mode, use a shorter delay to pump silence faster
        // We need ~250 blocks/sec to keep DMA fed, so max 4ms per iteration
        if (spdif_continuous_mode) {
          vTaskDelay(pdMS_TO_TICKS(1));  // Minimal yield, then pump more silence
        } else
#endif
        {
          vTaskDelay(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS / 2));
        }
      } else {
        size_t bytes_written = 0;

#ifdef USE_I2S_LEGACY
        // Legacy driver path
#ifdef USE_I2S_AUDIO_SPDIF_MODE
        if (this_speaker->spdif_mode_) {
          // SPDIF mode: encode PCM to BMC and write to I2S
          uint32_t blocks_sent = 0;
          esp_err_t err = this_speaker->spdif_encoder_->write(
              transfer_buffer->get_buffer_start(), transfer_buffer->available(), portMAX_DELAY, &blocks_sent);
          if (err == ESP_OK) {
            // All input was consumed by encoder (buffered internally)
            bytes_written = transfer_buffer->available();
          } else {
            ESP_LOGW(TAG, "SPDIF: Write failed with error %s", esp_err_to_name(err));
          }

          if (bytes_written > 0) {
            last_data_received_time = millis();
            // Track frames based on complete blocks sent
            frames_written += blocks_sent * SPDIF_BLOCK_SAMPLES;
            transfer_buffer->decrease_buffer_length(bytes_written);
            // The legacy driver doesn't easily support the callback approach for timestamps
            if (blocks_sent > 0) {
              // Apply SPDIF sync offset (defined at top of file)
              int64_t adjusted_timestamp = esp_timer_get_time() + dma_buffers_duration_ms * 1000 + SPDIF_SYNC_OFFSET_US;
              this_speaker->audio_output_callback_(blocks_sent * SPDIF_BLOCK_SAMPLES, adjusted_timestamp);
            }
          }
        } else
#endif  // USE_I2S_AUDIO_SPDIF_MODE
        {
          // Standard I2S mode
          if (this_speaker->current_stream_info_.get_bits_per_sample() == (uint8_t) this_speaker->bits_per_sample_) {
            i2s_write(this_speaker->parent_->get_port(), transfer_buffer->get_buffer_start(),
                      transfer_buffer->available(), &bytes_written, pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
          } else if (this_speaker->current_stream_info_.get_bits_per_sample() <
                     (uint8_t) this_speaker->bits_per_sample_) {
            i2s_write_expand(this_speaker->parent_->get_port(), transfer_buffer->get_buffer_start(),
                             transfer_buffer->available(), this_speaker->current_stream_info_.get_bits_per_sample(),
                             this_speaker->bits_per_sample_, &bytes_written, pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
          }

          if (bytes_written > 0) {
            last_data_received_time = millis();
            frames_written += this_speaker->current_stream_info_.bytes_to_frames(bytes_written);
            transfer_buffer->decrease_buffer_length(bytes_written);
            // The legacy driver doesn't easily support the callback approach for timestamps, so fall back to a direct
            // but less accurate approach.
            this_speaker->audio_output_callback_(this_speaker->current_stream_info_.bytes_to_frames(bytes_written),
                                                 esp_timer_get_time() + dma_buffers_duration_ms * 1000);
          }
        }
#else
        // New I2S driver path
#ifdef USE_I2S_AUDIO_SPDIF_MODE
        if (this_speaker->spdif_mode_) {
          // SPDIF Continuous Mode: channel is ALWAYS running, never disabled
          // We just write audio or silence - the stream never stops

          // Check if we need preload (only on startup or after fake-stop, NOT brief gaps)
          const bool was_fake_stopped = this_speaker->spdif_fake_stopped_;
          const bool needs_preload = this_speaker->spdif_needs_preload_;

          // SPDIF Preload: Wait for buffers to stabilize ONLY when needed.
          // This happens on initial startup or after fake-stop (seek/track change).
          // Brief gaps during normal playback should NOT trigger preload.
          if (needs_preload && this_speaker->spdif_silence_start_ != 0) {
            uint32_t silence_duration = millis() - this_speaker->spdif_silence_start_;
            if (silence_duration < SPDIF_PRELOAD_MS) {
              // Still preloading - write silence to keep DMA fed while we wait
              // We're in the audio block (have data), but we're not ready to play yet
              if (tx_dma_underflow && !stop_gracefully) {
                uint32_t silence_blocks = 0;
                this_speaker->spdif_encoder_->write(reinterpret_cast<const uint8_t *>(SPDIF_SILENCE_BUFFER),
                                                    sizeof(SPDIF_SILENCE_BUFFER), 0, &silence_blocks);
              }
              vTaskDelay(pdMS_TO_TICKS(5));  // Small delay to avoid busy loop
              continue;
            }
            // Preload complete - now transition to playing
            ESP_LOGV(TAG, "SPDIF: Preload complete after %" PRIu32 "ms, starting playback", silence_duration);
            this_speaker->spdif_needs_preload_ = false;     // Clear preload flag
            this_speaker->spdif_preload_ended_ = millis();  // Start grace period
          }

          // Restore from fake-stopped if applicable
          if (was_fake_stopped) {
            this_speaker->state_ = speaker::STATE_RUNNING;
            this_speaker->spdif_fake_stopped_ = false;
          }

          // Clear silence timer since we have audio data now
          if (this_speaker->spdif_silence_start_ != 0) {
            uint32_t silence_duration = millis() - this_speaker->spdif_silence_start_;
            // Only log if we were in silence for >100ms to reduce log spam during oscillation
            if (silence_duration > 100) {
              ESP_LOGV(TAG, "SPDIF: Exiting silence mode after %" PRIu32 "ms, have audio data", silence_duration);
            }
            this_speaker->spdif_silence_start_ = 0;
          }

          {
            uint32_t blocks_sent = 0;

            // Debug: log first write attempt
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
            static bool first_write_logged = false;
            if (!first_write_logged) {
              ESP_LOGV(TAG, "SPDIF: First write, available=%zu bytes", transfer_buffer->available());
              first_write_logged = true;
            }
#endif

            // Write audio data to encoder (which writes to DMA)
            esp_err_t err =
                this_speaker->spdif_encoder_->write(transfer_buffer->get_buffer_start(), transfer_buffer->available(),
                                                    pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS), &blocks_sent);
            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
              ESP_LOGW(TAG, "SPDIF write failed: %s", esp_err_to_name(err));
            }

            // All input data was consumed by the encoder (it buffers partial blocks internally)
            bytes_written = transfer_buffer->available();

            // Update frame accounting based on complete blocks sent (192 frames per block)
            if (bytes_written > 0) {
              last_data_received_time = millis();
              frames_written += blocks_sent * SPDIF_BLOCK_SAMPLES;
              transfer_buffer->decrease_buffer_length(bytes_written);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
              // Verbose: log periodic progress (rate limited)
              static uint32_t total_blocks_sent = 0;
              static uint32_t last_progress_log = 0;
              total_blocks_sent += blocks_sent;
              if (millis() - last_progress_log >= 1000) {
                ESP_LOGV(TAG, "SPDIF: blocks_sent=%" PRIu32 " total, frames_written=%" PRIu32, total_blocks_sent,
                         frames_written);
                last_progress_log = millis();
              }
#endif
            }
          }
        } else
#endif  // USE_I2S_AUDIO_SPDIF_MODE
        {
          // Standard I2S mode
          if (tx_dma_underflow) {
            // Temporarily disable channel and callback to reset the I2S driver's internal DMA buffer queue
            i2s_channel_disable(this_speaker->tx_handle_);
            const i2s_event_callbacks_t null_callbacks = {.on_sent = nullptr};
            i2s_channel_register_event_callback(this_speaker->tx_handle_, &null_callbacks, this_speaker);
            i2s_channel_preload_data(this_speaker->tx_handle_, transfer_buffer->get_buffer_start(),
                                     transfer_buffer->available(), &bytes_written);
          } else {
            // Audio is already playing, use regular write to add to the DMA buffers
            i2s_channel_write(this_speaker->tx_handle_, transfer_buffer->get_buffer_start(),
                              transfer_buffer->available(), &bytes_written, DMA_BUFFER_DURATION_MS);
          }

          if (bytes_written > 0) {
            last_data_received_time = millis();
            frames_written += this_speaker->current_stream_info_.bytes_to_frames(bytes_written);
            transfer_buffer->decrease_buffer_length(bytes_written);

            if (tx_dma_underflow) {
              tx_dma_underflow = false;
              // Enable the on_sent callback and channel after preload
              xQueueReset(this_speaker->i2s_event_queue_);
              const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
              i2s_channel_register_event_callback(this_speaker->tx_handle_, &callbacks, this_speaker);
              i2s_channel_enable(this_speaker->tx_handle_);
            }
          }
        }
#endif  // USE_I2S_LEGACY
      }
    }
    // If we reach here, the while loop exited - either via break or condition became false
#ifdef USE_I2S_AUDIO_SPDIF_MODE
    // In SPDIF mode, loop exit is expected when:
    // 1. Timeout reached (user configured timeout)
    // 2. COMMAND_STOP received (non-continuous mode)
    // 3. Stream info changed
    // Only warn if timeout is "never" since that should never exit
    if (this_speaker->spdif_mode_ && !this_speaker->timeout_.has_value()) {
      ESP_LOGW(TAG, "SPDIF: Unexpected loop exit; set 'timeout: never' to prevent this");
    }
#endif
  }

  xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::TASK_STOPPING);

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  // Reset SPDIF encoder state to prevent stale state on next start
  if (this_speaker->spdif_mode_ && this_speaker->spdif_encoder_ != nullptr) {
    this_speaker->spdif_encoder_->set_preload_mode(false);
    this_speaker->spdif_encoder_->reset();
  }
#endif  // USE_I2S_AUDIO_SPDIF_MODE

  if (transfer_buffer != nullptr) {
    transfer_buffer.reset();
  }

  xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::TASK_STOPPED);

  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void I2SAudioSpeaker::start() {
  if (!this->is_ready() || this->is_failed() || this->status_has_error())
    return;
  if ((this->state_ == speaker::STATE_STARTING) || (this->state_ == speaker::STATE_RUNNING))
    return;

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
}

void I2SAudioSpeaker::stop() { this->stop_(false); }

void I2SAudioSpeaker::finish() { this->stop_(true); }

void I2SAudioSpeaker::stop_(bool wait_on_empty) {
  if (this->is_failed())
    return;
  if (this->state_ == speaker::STATE_STOPPED)
    return;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  // In SPDIF continuous mode, immediately fake the stop to prevent blocking.
  // The speaker_task will continue running in the background, outputting silence.
  // This prevents the calling code from blocking while waiting for STATE_STOPPED.
  if (this->spdif_mode_ && this->state_ == speaker::STATE_RUNNING) {
    ESP_LOGV(TAG, "SPDIF: stop() called - immediately faking stop");
    this->state_ = speaker::STATE_STOPPED;
    this->spdif_fake_stopped_ = true;
    this->spdif_needs_preload_ = true;  // Next audio will need preload
    this->spdif_preload_ended_ = 0;     // Reset grace period
    this->spdif_silence_start_ = 0;     // Reset silence timer
    // Don't set COMMAND_STOP - let the task keep running in silence mode
    return;
  }
#endif

  if (wait_on_empty) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
  } else {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
  }
}

esp_err_t I2SAudioSpeaker::start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info) {
  this->current_stream_info_ = audio_stream_info;  // store the stream info settings the driver will use

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  if (this->spdif_mode_) {
    // SPDIF mode validation
    if (this->sample_rate_ != audio_stream_info.get_sample_rate()) {
      ESP_LOGE(TAG, "SPDIF only supports a single sample rate (configured: %" PRIu32 " Hz, stream: %" PRIu32 " Hz)",
               this->sample_rate_, audio_stream_info.get_sample_rate());
      return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio_stream_info.get_bits_per_sample() != 16) {
      ESP_LOGE(TAG, "SPDIF only supports 16 bits per sample");
      return ESP_ERR_NOT_SUPPORTED;
    }
    if (audio_stream_info.get_channels() != 2) {
      ESP_LOGE(TAG, "SPDIF only supports stereo (2 channels)");
      return ESP_ERR_NOT_SUPPORTED;
    }
  } else
#endif  // USE_I2S_AUDIO_SPDIF_MODE
  {
#ifdef USE_I2S_LEGACY
    if ((this->i2s_mode_ & I2S_MODE_SLAVE) && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {  // NOLINT
#else
    if ((this->i2s_role_ & I2S_ROLE_SLAVE) && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {  // NOLINT
#endif  // USE_I2S_LEGACY
      // Can't reconfigure I2S bus, so the sample rate must match the configured value
      ESP_LOGE(TAG, "Incompatible stream settings");
      return ESP_ERR_NOT_SUPPORTED;
    }
  }

#ifdef USE_I2S_LEGACY
  if ((i2s_bits_per_sample_t) audio_stream_info.get_bits_per_sample() > this->bits_per_sample_) {
#else
  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO &&
      (i2s_slot_bit_width_t) audio_stream_info.get_bits_per_sample() > this->slot_bit_width_) {
#endif  // USE_I2S_LEGACY
    // Currently can't handle the case when the incoming audio has more bits per sample than the configured value
    ESP_LOGE(TAG, "Stream bits per sample must be less than or equal to the speaker's configuration");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (!this->parent_->try_lock()) {
    ESP_LOGE(TAG, "Parent bus is busy");
    return ESP_ERR_INVALID_STATE;
  }

  uint32_t dma_buffer_length = audio_stream_info.ms_to_frames(DMA_BUFFER_DURATION_MS);

#ifdef USE_I2S_LEGACY
#ifdef USE_I2S_AUDIO_SPDIF_MODE
  if (this->spdif_mode_) {
    // SPDIF mode: use fixed configuration
    dma_buffer_length = SPDIF_BLOCK_SIZE_U32;  // One SPDIF block per DMA buffer

    // Log DMA configuration for debugging (legacy driver)
    ESP_LOGV(TAG, "SPDIF Legacy DMA config: %zu buffers × %lu words", SPDIF_DMA_BUFFERS_COUNT,
             (unsigned long) dma_buffer_length);

    i2s_driver_config_t config = {
      .mode = (i2s_mode_t) (this->i2s_mode_ | I2S_MODE_TX),
      .sample_rate = this->sample_rate_ * 2,  // Double rate for BMC encoding
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = this->i2s_comm_fmt_,
      .intr_alloc_flags = 0,
      .dma_buf_count = SPDIF_DMA_BUFFERS_COUNT,  // Use increased buffer count for SPDIF
      .dma_buf_len = (int) dma_buffer_length,
      .use_apll = this->use_apll_,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
      .mclk_multiple = this->mclk_multiple_,
      .bits_per_chan = this->bits_per_channel_,
#if SOC_I2S_SUPPORTS_TDM
      .chan_mask = (i2s_channel_t) (I2S_TDM_ACTIVE_CH0 | I2S_TDM_ACTIVE_CH1),
      .total_chan = 2,
      .left_align = false,
      .big_edin = false,
      .bit_order_msb = false,
      .skip_msk = false,
#endif  // SOC_I2S_SUPPORTS_TDM
    };

    esp_err_t err =
        i2s_driver_install(this->parent_->get_port(), &config, SPDIF_I2S_EVENT_QUEUE_COUNT, &this->i2s_event_queue_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to install driver for SPDIF");
      this->parent_->unlock();
      return err;
    }

    // SPDIF only needs data pin, no clock pins
    i2s_pin_config_t pin_config = {
        .mck_io_num = -1,
        .bck_io_num = -1,
        .ws_io_num = -1,
        .data_out_num = this->dout_pin_,
        .data_in_num = -1,
    };

    err = i2s_set_pin(this->parent_->get_port(), &pin_config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to set the data out pin for SPDIF");
      i2s_driver_uninstall(this->parent_->get_port());
      this->parent_->unlock();
    }
    return err;
  }
#endif  // USE_I2S_AUDIO_SPDIF_MODE

  // Standard I2S mode
  i2s_channel_fmt_t channel = this->channel_;

  if (audio_stream_info.get_channels() == 1) {
    if (this->channel_ == I2S_CHANNEL_FMT_ONLY_LEFT) {
      channel = I2S_CHANNEL_FMT_ONLY_LEFT;
    } else {
      channel = I2S_CHANNEL_FMT_ONLY_RIGHT;
    }
  } else if (audio_stream_info.get_channels() == 2) {
    channel = I2S_CHANNEL_FMT_RIGHT_LEFT;
  }

  i2s_driver_config_t config = {
    .mode = (i2s_mode_t) (this->i2s_mode_ | I2S_MODE_TX),
    .sample_rate = audio_stream_info.get_sample_rate(),
    .bits_per_sample = this->bits_per_sample_,
    .channel_format = channel,
    .communication_format = this->i2s_comm_fmt_,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUFFERS_COUNT,
    .dma_buf_len = (int) dma_buffer_length,
    .use_apll = this->use_apll_,
    .tx_desc_auto_clear = true,
    .fixed_mclk = I2S_PIN_NO_CHANGE,
    .mclk_multiple = this->mclk_multiple_,
    .bits_per_chan = this->bits_per_channel_,
#if SOC_I2S_SUPPORTS_TDM
    .chan_mask = (i2s_channel_t) (I2S_TDM_ACTIVE_CH0 | I2S_TDM_ACTIVE_CH1),
    .total_chan = 2,
    .left_align = false,
    .big_edin = false,
    .bit_order_msb = false,
    .skip_msk = false,
#endif  // SOC_I2S_SUPPORTS_TDM
  };
#if SOC_I2S_SUPPORTS_DAC
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    config.mode = (i2s_mode_t) (config.mode | I2S_MODE_DAC_BUILT_IN);
  }
#endif  // SOC_I2S_SUPPORTS_DAC

  esp_err_t err =
      i2s_driver_install(this->parent_->get_port(), &config, I2S_EVENT_QUEUE_COUNT, &this->i2s_event_queue_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to install I2S legacy driver");
    // Failed to install the driver, so unlock the I2S port
    this->parent_->unlock();
    return err;
  }

#if SOC_I2S_SUPPORTS_DAC
  if (this->internal_dac_mode_ == I2S_DAC_CHANNEL_DISABLE) {
#endif  // SOC_I2S_SUPPORTS_DAC
    i2s_pin_config_t pin_config = this->parent_->get_pin_config();
    pin_config.data_out_num = this->dout_pin_;

    err = i2s_set_pin(this->parent_->get_port(), &pin_config);
#if SOC_I2S_SUPPORTS_DAC
  } else {
    i2s_set_dac_mode(this->internal_dac_mode_);
  }
#endif  // SOC_I2S_SUPPORTS_DAC

  if (err != ESP_OK) {
    // Failed to set the data out pin, so uninstall the driver and unlock the I2S port
    ESP_LOGE(TAG, "Failed to set the data out pin");
    i2s_driver_uninstall(this->parent_->get_port());
    this->parent_->unlock();
  }
#else
  // Determine mode-specific parameters
  i2s_role_t i2s_role = this->i2s_role_;
  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  if (this->spdif_mode_) {
    // SPDIF mode: fixed configuration for BMC encoding
    // For new driver, dma_frame_num is in I2S frames (8 bytes each for 32-bit stereo)
    dma_buffer_length = SPDIF_BLOCK_I2S_FRAMES;  // One SPDIF block = 384 I2S frames = 3072 bytes
  }
#endif  // USE_I2S_AUDIO_SPDIF_MODE

#if SOC_CLK_APLL_SUPPORTED
  if (this->use_apll_) {
    clk_src = i2s_clock_src_t::I2S_CLK_SRC_APLL;
  }
#endif  // SOC_CLK_APLL_SUPPORTED

  // Allocate I2S channel (shared between SPDIF and standard modes)
  // SPDIF uses more DMA buffers to compensate for smaller buffer size (~4ms vs ~15ms)
#ifdef USE_I2S_AUDIO_SPDIF_MODE
  const size_t dma_buffer_count = this->spdif_mode_ ? SPDIF_DMA_BUFFERS_COUNT : DMA_BUFFERS_COUNT;
#else
  const size_t dma_buffer_count = DMA_BUFFERS_COUNT;
#endif

  // Log DMA configuration for debugging
  ESP_LOGV(TAG, "I2S DMA config: %zu buffers × %lu frames = %lu bytes total", dma_buffer_count,
           (unsigned long) dma_buffer_length,
           (unsigned long) (dma_buffer_count * dma_buffer_length * 8));  // 8 bytes per frame for 32-bit stereo

  i2s_chan_config_t chan_cfg = {
      .id = this->parent_->get_port(),
      .role = i2s_role,
      .dma_desc_num = dma_buffer_count,
      .dma_frame_num = dma_buffer_length,
      .auto_clear = true,
      .intr_priority = 3,
  };

  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to allocate new I2S channel: %s", esp_err_to_name(err));
    this->parent_->unlock();
    return err;
  }

  // Build mode-specific configuration
  i2s_std_clk_config_t clk_cfg;
  i2s_std_slot_config_t slot_cfg;
  i2s_std_gpio_config_t gpio_cfg;

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  if (this->spdif_mode_) {
    // SPDIF: double sample rate for BMC, 32-bit stereo, only data pin needed
    clk_cfg = {
        .sample_rate_hz = this->sample_rate_ * 2,
        .clk_src = clk_src,
        .mclk_multiple = this->mclk_multiple_,
    };

    slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);

    gpio_cfg = {
        .mclk = GPIO_NUM_NC,
        .bclk = GPIO_NUM_NC,
        .ws = GPIO_NUM_NC,
        .dout = this->dout_pin_,
        .din = GPIO_NUM_NC,
        .invert_flags =
            {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
    };
  } else
#endif  // USE_I2S_AUDIO_SPDIF_MODE
  {
    // Standard I2S mode
    clk_cfg = {
        .sample_rate_hz = audio_stream_info.get_sample_rate(),
        .clk_src = clk_src,
        .mclk_multiple = this->mclk_multiple_,
    };

    i2s_slot_mode_t slot_mode = this->slot_mode_;
    i2s_std_slot_mask_t slot_mask = this->std_slot_mask_;
    if (audio_stream_info.get_channels() == 1) {
      slot_mode = I2S_SLOT_MODE_MONO;
    } else if (audio_stream_info.get_channels() == 2) {
      slot_mode = I2S_SLOT_MODE_STEREO;
      slot_mask = I2S_STD_SLOT_BOTH;
    }

    if (this->i2s_comm_fmt_ == "std") {
      slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(),
                                                     slot_mode);
    } else if (this->i2s_comm_fmt_ == "pcm") {
      slot_cfg =
          I2S_STD_PCM_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(), slot_mode);
    } else {
      slot_cfg =
          I2S_STD_MSB_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(), slot_mode);
    }
#ifdef USE_ESP32_VARIANT_ESP32
    // There seems to be a bug on the ESP32 (non-variant) platform where setting the slot bit width higher than the
    // bits per sample causes the audio to play too fast. Setting the ws_width to the configured slot bit width seems
    // to make it play at the correct speed while sending more bits per slot.
    if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO) {
      uint32_t configured_bit_width = static_cast<uint32_t>(this->slot_bit_width_);
      slot_cfg.ws_width = configured_bit_width;
      if (configured_bit_width > 16) {
        slot_cfg.msb_right = false;
      }
    }
#else
    slot_cfg.slot_bit_width = this->slot_bit_width_;
#endif  // USE_ESP32_VARIANT_ESP32
    slot_cfg.slot_mask = slot_mask;

    gpio_cfg = this->parent_->get_pin_config();
    gpio_cfg.dout = this->dout_pin_;
  }

  // Initialize channel with mode-specific configuration (shared)
  i2s_std_config_t std_cfg = {
      .clk_cfg = clk_cfg,
      .slot_cfg = slot_cfg,
      .gpio_cfg = gpio_cfg,
  };

  err = i2s_channel_init_std_mode(this->tx_handle_, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize I2S channel");
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
    this->parent_->unlock();
    return err;
  }

  if (this->i2s_event_queue_ == nullptr) {
#ifdef USE_I2S_AUDIO_SPDIF_MODE
    const size_t event_queue_size = this->spdif_mode_ ? SPDIF_I2S_EVENT_QUEUE_COUNT : I2S_EVENT_QUEUE_COUNT;
#else
    const size_t event_queue_size = I2S_EVENT_QUEUE_COUNT;
#endif
    this->i2s_event_queue_ = xQueueCreate(event_queue_size, sizeof(int64_t));
  } else {
    // Reset queue to clear any stale events from previous task
    xQueueReset(this->i2s_event_queue_);
  }

#ifdef USE_I2S_AUDIO_SPDIF_MODE
  // For SPDIF continuous mode, register callback at startup since we never disable the channel
  if (this->spdif_mode_) {
    const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
    i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
  }
#endif  // USE_I2S_AUDIO_SPDIF_MODE

  i2s_channel_enable(this->tx_handle_);
#endif  // USE_I2S_LEGACY

  return err;
}

#ifndef USE_I2S_LEGACY
bool IRAM_ATTR I2SAudioSpeaker::i2s_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
  int64_t now = esp_timer_get_time();

  BaseType_t need_yield1 = pdFALSE;
  BaseType_t need_yield2 = pdFALSE;
  BaseType_t need_yield3 = pdFALSE;

  I2SAudioSpeaker *this_speaker = (I2SAudioSpeaker *) user_ctx;

  if (xQueueIsQueueFullFromISR(this_speaker->i2s_event_queue_)) {
    // Queue is full, so discard the oldest event and set the warning flag to inform the user
    int64_t dummy;
    xQueueReceiveFromISR(this_speaker->i2s_event_queue_, &dummy, &need_yield1);
    xEventGroupSetBitsFromISR(this_speaker->event_group_, SpeakerEventGroupBits::WARN_DROPPED_EVENT, &need_yield2);
  }

  xQueueSendToBackFromISR(this_speaker->i2s_event_queue_, &now, &need_yield3);

  return need_yield1 | need_yield2 | need_yield3;
}
#endif  // USE_I2S_LEGACY

void I2SAudioSpeaker::stop_i2s_driver_() {
#ifdef USE_I2S_LEGACY
  i2s_driver_uninstall(this->parent_->get_port());
#else
  i2s_channel_disable(this->tx_handle_);
  i2s_del_channel(this->tx_handle_);
  this->tx_handle_ = nullptr;
#endif  // USE_I2S_LEGACY
  this->parent_->unlock();
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
