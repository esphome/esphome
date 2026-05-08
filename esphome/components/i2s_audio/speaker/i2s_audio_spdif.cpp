#include "i2s_audio_spdif.h"

#if defined(USE_ESP32) && defined(USE_I2S_AUDIO_SPDIF_MODE)

#include <driver/i2s_std.h>

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio.spdif";

// SPDIF mode adds overhead as each sample is encapsulated in a subframe;
// each DMA buffer can hold only 192 samples (~4ms each vs. ~15ms for standard I2S).
// To match the standard I2S buffering duration, we use more buffers to minimize
// the impact of the overhead, such as stuttering or audio/silence oscillation.
// 15 buffers x 4ms = 60ms of DMA buffering (same as 4 x 15ms for standard)
static constexpr size_t SPDIF_DMA_BUFFERS_COUNT = 15;

// Timeout for flushing pending frames if no callback received.
static constexpr uint32_t SPDIF_FLUSH_TIMEOUT_MS = 20;

// Number of DMA events between upstream callbacks (~16ms = 4 events x 4ms each).
// Matches non-SPDIF timing to prevent overwhelming upstream sync algorithms.
static constexpr uint32_t SPDIF_DMA_EVENTS_PER_CALLBACK = 4;

// Consider TX stalled only if no DMA callbacks have arrived for this long.
// Zero-block non-blocking writes alone are not sufficient (they can happen when DMA is simply full).
static constexpr uint32_t SPDIF_STALL_NO_DMA_MS = 80;

// Fallback stall detector: force recovery if silence writes make no forward progress for too long,
// even if occasional DMA callbacks are still observed.
static constexpr uint32_t SPDIF_STALL_ZERO_PROGRESS_MS = 1000;

// Minimum spacing between re-prime attempts to avoid churn.
static constexpr uint32_t SPDIF_REPRIME_COOLDOWN_MS = 500;

// Small waits used in SPDIF mode to keep DMA fed during rapid pipeline churn.
static constexpr uint32_t SPDIF_EMPTY_READ_DELAY_MS = 1;
static constexpr uint32_t SPDIF_SILENCE_LOOP_DELAY_MS = 1;
static constexpr uint32_t SPDIF_PLAY_RETRY_WAIT_MS = 5;

static constexpr size_t SPDIF_I2S_EVENT_QUEUE_COUNT = SPDIF_DMA_BUFFERS_COUNT + 1;

// Static silence buffer for SPDIF continuous mode
// 192 samples * 2 channels * 2 bytes per sample = 768 bytes
// Stored in flash (.rodata section) to avoid stack/heap usage
static const int16_t SPDIF_SILENCE_BUFFER[SPDIF_BLOCK_SAMPLES * 2] = {0};

// Static callback functions for SPDIF encoder (avoids std::function overhead)
static esp_err_t spdif_preload_cb(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait) {
  auto *speaker = static_cast<I2SAudioSpeakerSPDIF *>(user_ctx);
  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_preload_data(speaker->get_tx_handle(), data, size, &bytes_written);
  if (err != ESP_OK || bytes_written != size) {
    ESP_LOGW(TAG, "Preload failed: %s (wrote %zu/%zu bytes)", esp_err_to_name(err), bytes_written, size);
    return (err != ESP_OK) ? err : ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

static esp_err_t spdif_write_cb(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait) {
  auto *speaker = static_cast<I2SAudioSpeakerSPDIF *>(user_ctx);
  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(speaker->get_tx_handle(), data, size, &bytes_written, ticks_to_wait);
  // ESP_ERR_TIMEOUT is expected under DMA backpressure in SPDIF mode.
  if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
    ESP_LOGW(TAG, "I2S write failed: %s (wrote %zu/%zu bytes)", esp_err_to_name(err), bytes_written, size);
  }
  return err;
}

void I2SAudioSpeakerSPDIF::setup() {
  I2SAudioSpeakerBase::setup();
  if (this->is_failed()) {
    return;
  }

  this->spdif_encoder_ = new SPDIFEncoder();
  if (!this->spdif_encoder_->setup()) {
    ESP_LOGE(TAG, "Encoder setup failed");
    this->mark_failed();
    return;
  }

  // Configure channel status block with the sample rate
  this->spdif_encoder_->set_sample_rate(this->sample_rate_);

  // Separate callbacks for preload (during underflow recovery) and normal writes
  this->spdif_encoder_->set_preload_callback(spdif_preload_cb, this);
  this->spdif_encoder_->set_write_callback(spdif_write_cb, this);
}

void I2SAudioSpeakerSPDIF::dump_config() {
  I2SAudioSpeakerBase::dump_config();
  ESP_LOGCONFIG(TAG,
                "  SPDIF Mode: YES\n"
                "  Sample Rate: %" PRIu32 " Hz",
                this->sample_rate_);
}

void I2SAudioSpeakerSPDIF::on_task_stopped() { this->spdif_silence_start_ = 0; }

size_t I2SAudioSpeakerSPDIF::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed; cannot play audio");
    return 0;
  }

  // In SPDIF mode, keep accepting upstream audio while the speaker task is active.
  // This avoids transient drops during stop/start transitions.
  const bool task_active = (this->speaker_task_handle_ != nullptr);

  if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
    this->start();
  }

  if (!task_active && this->state_ != speaker::STATE_RUNNING) {
    // Unable to write data to a running speaker, so delay the max amount of time so it can get ready
    vTaskDelay(ticks_to_wait);
    ticks_to_wait = 0;
  }

  size_t bytes_written = 0;
  if (this->state_ == speaker::STATE_RUNNING || task_active) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
    if (temp_ring_buffer != nullptr) {
      // In SPDIF mode, a tiny wait helps avoid transient 0-byte writes during short backpressure windows.
      TickType_t effective_ticks_to_wait = ticks_to_wait;
      if (effective_ticks_to_wait == 0) {
        effective_ticks_to_wait = pdMS_TO_TICKS(1);
      }
      bytes_written = temp_ring_buffer->write_without_replacement((void *) data, length, effective_ticks_to_wait);
      if (bytes_written == 0 && length > 0) {
        // Retry once to catch short free-space windows during rapid seek/track transitions.
        bytes_written =
            temp_ring_buffer->write_without_replacement((void *) data, length, pdMS_TO_TICKS(SPDIF_PLAY_RETRY_WAIT_MS));
      }
    }
  }

  return bytes_written;
}

void I2SAudioSpeakerSPDIF::run_speaker_task() {
  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STARTING);

  // Reset SPDIF encoder at task start to ensure clean state
  // (previous task may have left stale data in encoder buffer)
  if (this->spdif_encoder_ != nullptr) {
    this->spdif_encoder_->reset();
  }

  const uint32_t dma_buffers_duration_ms = DMA_BUFFER_DURATION_MS * SPDIF_DMA_BUFFERS_COUNT;
  // Ensure ring buffer duration is at least the duration of all DMA buffers
  const uint32_t ring_buffer_duration = std::max(dma_buffers_duration_ms, this->buffer_duration_ms_);

  // The DMA buffers may have more bits per sample, so calculate buffer sizes based on the input audio stream info
  const size_t ring_buffer_size = this->current_stream_info_.ms_to_bytes(ring_buffer_duration);

  // For SPDIF mode, one DMA buffer = one SPDIF block = 192 PCM frames
  const uint32_t frames_to_fill_single_dma_buffer = SPDIF_BLOCK_SAMPLES;
  const size_t bytes_to_fill_single_dma_buffer =
      this->current_stream_info_.frames_to_bytes(frames_to_fill_single_dma_buffer);

  bool successful_setup = false;
  std::unique_ptr<audio::AudioSourceTransferBuffer> transfer_buffer =
      audio::AudioSourceTransferBuffer::create(bytes_to_fill_single_dma_buffer);

  if (transfer_buffer != nullptr) {
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = ring_buffer::RingBuffer::create(ring_buffer_size);
    if (temp_ring_buffer.use_count() == 1) {
      transfer_buffer->set_source(temp_ring_buffer);
      this->audio_ring_buffer_ = temp_ring_buffer;
      successful_setup = true;
    }
  }

  if (!successful_setup) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  } else {
    // Preload DMA buffers with SPDIF-encoded silence before enabling the channel.
    // This ensures the first data transmitted is valid SPDIF (not raw zeros from
    // auto_clear) and prevents phantom DMA events before real audio is available.
    // Track how many buffers were preloaded so the DMA event loop can skip
    // frame accounting until the preloaded silence has fully drained.
    uint32_t preload_buffers_remaining = 0;
    this->spdif_encoder_->set_preload_mode(true);
    for (size_t i = 0; i < SPDIF_DMA_BUFFERS_COUNT; i++) {
      uint32_t preload_blocks = 0;
      esp_err_t preload_err = this->spdif_encoder_->write(reinterpret_cast<const uint8_t *>(SPDIF_SILENCE_BUFFER),
                                                          sizeof(SPDIF_SILENCE_BUFFER),
                                                          pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS), &preload_blocks);
      if (preload_err != ESP_OK || preload_blocks == 0) {
        break;  // DMA buffers full or error
      }
      preload_buffers_remaining += preload_blocks;
    }
    this->spdif_encoder_->set_preload_mode(false);
    this->spdif_encoder_->reset();  // Clean encoder state for the main loop

    // Now register the callback and enable the channel
    xQueueReset(this->i2s_event_queue_);
    const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
    i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
    i2s_channel_enable(this->tx_handle_);

    bool stop_gracefully = false;
    bool tx_dma_underflow = true;

    uint32_t frames_written = 0;

    // SPDIF Continuous Silence Mode + Callback Decimation
    //
    // Key principles:
    // 1. NEVER stop the I2S channel - always output a valid SPDIF stream
    // 2. When no audio data, output silence-encoded SPDIF blocks (not zeros!)
    // 3. Fire callbacks every 4 DMA events (~16ms), matching non-SPDIF timing
    //
    // This eliminates gaps that cause SPDIF receivers to re-sync, and reduces
    // callback rate to prevent overwhelming upstream sync algorithms.
    const uint32_t spdif_callback_threshold = this->current_stream_info_.ms_to_frames(DMA_BUFFER_DURATION_MS);
    uint32_t spdif_pending_frames = 0;
    int64_t spdif_pending_timestamp = 0;
    uint32_t spdif_last_callback_time = millis();
    // Count DMA events for decimation
    uint32_t spdif_dma_event_count = 0;
    uint32_t spdif_last_dma_event_time = millis();
    // Detect a stalled DMA path (many silence write attempts with zero accepted blocks).
    uint32_t spdif_zero_block_streak = 0;
    uint32_t spdif_last_block_progress_time = millis();
    uint32_t spdif_last_reprime_time = 0;

    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);

    // SPDIF continuous mode: loop runs indefinitely, outputting silence when no audio data
    // to keep the receiver synced. Exits only via break (stream info change or silence timeout).
    while (true) {
      uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
        // In SPDIF continuous mode, don't tear down or expose STOPPED here.
        // Keep the task alive and transition to silence output.
        this->spdif_silence_start_ = millis();
        ESP_LOGV(TAG, "COMMAND_STOP received, continuing in silence mode");
      }
      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY) {
        xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
        stop_gracefully = true;
      }

      if (this->audio_stream_info_ != this->current_stream_info_) {
        // Audio stream info changed, stop the speaker task so it will restart with the proper settings.
        ESP_LOGV(TAG, "Exiting: stream info changed");
        break;
      }

      int64_t write_timestamp;
      while (xQueueReceive(this->i2s_event_queue_, &write_timestamp, 0)) {
        spdif_last_dma_event_time = millis();

        // Skip frame accounting for preloaded silence buffers still draining.
        // These DMA events correspond to silence that was preloaded before the
        // channel was enabled, not real audio written by the task.
        if (preload_buffers_remaining > 0) {
          preload_buffers_remaining--;
          continue;
        }

        // Receives timing events from the I2S on_sent callback. If actual audio data was sent in this event, it passes
        // on the timing info via the audio_output_callback.
        uint32_t frames_sent = frames_to_fill_single_dma_buffer;
        if (frames_to_fill_single_dma_buffer > frames_written) {
          tx_dma_underflow = true;
          frames_sent = frames_written;
          const uint32_t frames_zeroed = frames_to_fill_single_dma_buffer - frames_written;
          write_timestamp -= this->current_stream_info_.frames_to_microseconds(frames_zeroed);
        } else {
          tx_dma_underflow = false;
        }
        frames_written -= frames_sent;

        // SPDIF Callback Decimation: fire every 4th DMA event (~16ms)
        // This matches non-SPDIF timing and prevents overwhelming upstream.
        if (spdif_callback_threshold > 0) {
          spdif_dma_event_count++;

          // Accumulate frames; always keep the latest timestamp so the
          // callback reports when the last sample left the wire, not the first.
          if (frames_sent > 0) {
            spdif_pending_timestamp = write_timestamp;
            spdif_pending_frames += frames_sent;
          }

          // Fire callback every 4 DMA events, or on timeout if we have pending frames
          bool decimation_reached = (spdif_dma_event_count >= SPDIF_DMA_EVENTS_PER_CALLBACK);
          bool timeout_flush =
              (spdif_pending_frames > 0) && ((millis() - spdif_last_callback_time) >= SPDIF_FLUSH_TIMEOUT_MS);

          if (decimation_reached || timeout_flush) {
            if (spdif_pending_frames > 0) {
              this->audio_output_callback_(spdif_pending_frames, spdif_pending_timestamp);
              spdif_pending_frames = 0;
              spdif_last_callback_time = millis();
            }
            spdif_dma_event_count = 0;  // Reset decimation counter
          }
        }
      }

      if (this->pause_state_) {
        // Pause state is accessed atomically, so thread safe
        // Delay so the task yields, then skip transferring audio data
        vTaskDelay(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
        continue;
      }

      // Wait half the duration of the data already written to the DMA buffers for new audio data
      // The millisecond helper modifies the frames_written variable, so use the microsecond helper and divide by 1000
      uint32_t read_delay = (this->current_stream_info_.frames_to_microseconds(frames_written) / 1000) / 2;

      // In SPDIF mode, if transfer buffer is empty (we're pumping silence), use a very short timeout.
      // This ensures we can pump silence fast enough to keep the DMA fed (~250 blocks/sec needed).
      // Otherwise the long timeout based on frames_written causes DMA to run dry.
      if (transfer_buffer->available() == 0) {
        read_delay = SPDIF_EMPTY_READ_DELAY_MS;
      }

      size_t bytes_read = transfer_buffer->transfer_data_from_source(pdMS_TO_TICKS(read_delay));
      uint8_t *new_data = transfer_buffer->get_buffer_end() - bytes_read;

      if (bytes_read > 0) {
        this->apply_software_volume_(new_data, bytes_read);
        this->swap_esp32_mono_samples_(new_data, bytes_read);
      }

      if (transfer_buffer->available() == 0) {
        // SPDIF Continuous Silence Mode: always output valid SPDIF stream
        // When no audio data, write silence-encoded blocks to keep receiver happy
        if (this->spdif_encoder_ != nullptr) {
          // "Graceful stop" means "drain buffered audio, then stop." In SPDIF
          // continuous mode we never actually stop, so once audio is drained
          // (we're here), reset the flag to re-enable silence writing and stall
          // recovery. Without this, stop_gracefully stays true forever and
          // blocks silence output, causing DMA to degrade on auto_clear zeros.
          stop_gracefully = false;

          // Track when we entered silence mode
          if (this->spdif_silence_start_ == 0) {
            this->spdif_silence_start_ = millis();
          }

          // If silence persists past the configured timeout, stop the task
          // so components expecting timeout semantics can recover.
          if (this->timeout_.has_value()) {
            const uint32_t silence_duration = millis() - this->spdif_silence_start_;
            if (silence_duration >= this->timeout_.value()) {
              ESP_LOGV(TAG, "Silence timeout reached (%" PRIu32 "ms) - stopping speaker", silence_duration);
              break;
            }
          }

          // First flush any partial block with silence padding (non-blocking to avoid getting stuck).
          // IMPORTANT: Credit any partial block frames to frames_written so the audio_output_callback_
          // fires for them. Without this, pending_playback_frames_ in the mixer's SourceSpeaker never
          // reaches 0 when a stream ends on a non-192-frame boundary, permanently blocking teardown.
          if (this->spdif_encoder_->has_pending_data()) {
            uint32_t partial_frames = this->spdif_encoder_->get_pending_frames();
            // Use a tiny timeout to allow DMA queue progress without stalling the task.
            esp_err_t flush_err = this->spdif_encoder_->flush_with_silence(pdMS_TO_TICKS(1));
            if (flush_err == ESP_OK && partial_frames > 0) {
              frames_written += partial_frames;
            }
          }

          // CRITICAL: In SPDIF continuous mode, ALWAYS write silence when no audio data.
          // We don't check tx_dma_underflow because:
          // 1. When DMA runs empty, callbacks stop, so tx_dma_underflow doesn't update
          // 2. The non-blocking write handles "DMA full" gracefully (just doesn't write)
          // 3. We need continuous output to prevent receiver from losing sync
          if (!stop_gracefully) {
            uint32_t silence_blocks = 0;
            esp_err_t write_err = this->spdif_encoder_->write(
                reinterpret_cast<const uint8_t *>(SPDIF_SILENCE_BUFFER), sizeof(SPDIF_SILENCE_BUFFER), pdMS_TO_TICKS(1),
                &silence_blocks);  // Non-blocking
                                   // Don't count silence as frames_written - it's not real audio

            // Recovery path for a stalled SPDIF TX channel:
            // if silence writes repeatedly produce zero blocks AND DMA callbacks have stopped,
            // re-prime DMA using preload mode.
            const uint32_t ms_since_dma = millis() - spdif_last_dma_event_time;
            const bool dma_events_stalled = ms_since_dma >= SPDIF_STALL_NO_DMA_MS;
            if (silence_blocks > 0) {
              spdif_last_block_progress_time = millis();
            }
            const bool long_zero_progress = (millis() - spdif_last_block_progress_time) >= SPDIF_STALL_ZERO_PROGRESS_MS;
            if (dma_events_stalled && silence_blocks == 0 && (write_err == ESP_OK || write_err == ESP_ERR_TIMEOUT)) {
              spdif_zero_block_streak++;
            } else {
              spdif_zero_block_streak = 0;
            }

            const uint32_t now_ms = millis();
            const bool reprime_cooldown_elapsed =
                (spdif_last_reprime_time == 0) || ((now_ms - spdif_last_reprime_time) >= SPDIF_REPRIME_COOLDOWN_MS);

            if ((spdif_zero_block_streak >= 100 || long_zero_progress) && reprime_cooldown_elapsed) {
              ESP_LOGV(TAG, "TX appears stalled, attempting DMA re-prime");

              i2s_channel_disable(this->tx_handle_);

              const i2s_event_callbacks_t null_callbacks = {.on_sent = nullptr};
              i2s_channel_register_event_callback(this->tx_handle_, &null_callbacks, this);

              this->spdif_encoder_->set_preload_mode(true);
              uint32_t preload_blocks = 0;
              esp_err_t preload_err = this->spdif_encoder_->write(
                  reinterpret_cast<const uint8_t *>(SPDIF_SILENCE_BUFFER), sizeof(SPDIF_SILENCE_BUFFER),
                  pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS), &preload_blocks);
              this->spdif_encoder_->set_preload_mode(false);

              xQueueReset(this->i2s_event_queue_);
              const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
              i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
              i2s_channel_enable(this->tx_handle_);

              if (preload_err == ESP_OK && preload_blocks > 0) {
                tx_dma_underflow = false;
                preload_buffers_remaining = preload_blocks;
                frames_written = 0;  // Stale after channel disable/enable cycle
                ESP_LOGV(TAG, "DMA re-prime successful (%" PRIu32 " preload blocks)", preload_blocks);
                spdif_last_block_progress_time = now_ms;
              } else {
                ESP_LOGW(TAG, "DMA re-prime failed (%s, blocks=%" PRIu32 ")", esp_err_to_name(preload_err),
                         preload_blocks);
              }
              spdif_last_reprime_time = now_ms;
              spdif_zero_block_streak = 0;
            }
          }
        }

        if (stop_gracefully && tx_dma_underflow) {
          // In SPDIF continuous mode, don't break on graceful stop during silence
          // Keep outputting silence until new audio arrives or explicit COMMAND_STOP
          // (handled above which transitions to silence mode rather than breaking)
        }

        // In SPDIF mode, use a shorter delay to pump silence faster
        // We need ~250 blocks/sec to keep DMA fed, so max 4ms per iteration
        vTaskDelay(pdMS_TO_TICKS(SPDIF_SILENCE_LOOP_DELAY_MS));
      } else {
        // Have audio data to write
        size_t bytes_written = 0;

        // Clear silence timer since we have audio data now
        if (this->spdif_silence_start_ != 0) {
          uint32_t silence_duration = millis() - this->spdif_silence_start_;
          if (silence_duration > 100) {
            ESP_LOGV(TAG, "Exiting silence mode after %" PRIu32 "ms, have audio data", silence_duration);
          }
          this->spdif_silence_start_ = 0;
        }

        {
          uint32_t blocks_sent = 0;
          size_t pcm_bytes_consumed = 0;

          // Write audio data to encoder (which writes to DMA)
          esp_err_t err =
              this->spdif_encoder_->write(transfer_buffer->get_buffer_start(), transfer_buffer->available(),
                                          pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS), &blocks_sent, &pcm_bytes_consumed);
          if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Write failed: %s", esp_err_to_name(err));
          }

          // Only consume source bytes that were actually accepted by the encoder.
          bytes_written = pcm_bytes_consumed;

          // Update frame accounting based on complete blocks sent (192 frames per block)
          if (bytes_written > 0) {
            frames_written += blocks_sent * SPDIF_BLOCK_SAMPLES;
            transfer_buffer->decrease_buffer_length(bytes_written);
            // Audio blocks count as DMA progress for the stall detector.
            // Without this, a long uninterrupted audio stream makes the
            // progress timer stale, triggering a spurious re-prime the
            // instant we transition to silence.
            spdif_last_block_progress_time = millis();
          }
        }
      }
    }
    // If we reach here, the while loop exited - either via break or condition became false
    // In SPDIF mode, loop exit is expected when:
    // 1. Timeout reached (user configured timeout)
    // 2. Stream info changed
    // Only warn if timeout is "never" since that should never exit
    if (!this->timeout_.has_value()) {
      ESP_LOGW(TAG, "Unexpected loop exit; set 'timeout: never' to prevent this");
    }
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPING);

  // Reset SPDIF encoder state to prevent stale state on next start
  if (this->spdif_encoder_ != nullptr) {
    this->spdif_encoder_->set_preload_mode(false);
    this->spdif_encoder_->reset();
  }

  if (transfer_buffer != nullptr) {
    transfer_buffer.reset();
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPED);

  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

esp_err_t I2SAudioSpeakerSPDIF::start_i2s_driver(audio::AudioStreamInfo &audio_stream_info) {
  this->current_stream_info_ = audio_stream_info;

  // SPDIF mode validation
  if (this->sample_rate_ != audio_stream_info.get_sample_rate()) {
    ESP_LOGE(TAG, "Only supports a single sample rate (configured: %" PRIu32 " Hz, stream: %" PRIu32 " Hz)",
             this->sample_rate_, audio_stream_info.get_sample_rate());
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (audio_stream_info.get_bits_per_sample() != 16) {
    ESP_LOGE(TAG, "Only supports 16 bits per sample");
    return ESP_ERR_NOT_SUPPORTED;
  }
  if (audio_stream_info.get_channels() != 2) {
    ESP_LOGE(TAG, "Only supports stereo (2 channels)");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO &&
      (i2s_slot_bit_width_t) audio_stream_info.get_bits_per_sample() > this->slot_bit_width_) {
    ESP_LOGE(TAG, "Stream bits per sample must be less than or equal to the speaker's configuration");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (!this->parent_->try_lock()) {
    ESP_LOGE(TAG, "Parent bus is busy");
    return ESP_ERR_INVALID_STATE;
  }

  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;

#if SOC_CLK_APLL_SUPPORTED
  if (this->use_apll_) {
    clk_src = i2s_clock_src_t::I2S_CLK_SRC_APLL;
  }
#endif  // SOC_CLK_APLL_SUPPORTED

  // SPDIF mode: fixed configuration for BMC encoding
  // For new driver, dma_frame_num is in I2S frames (8 bytes each for 32-bit stereo)
  uint32_t dma_buffer_length = SPDIF_BLOCK_I2S_FRAMES;  // One SPDIF block = 384 I2S frames = 3072 bytes

  // Log DMA configuration for debugging
  ESP_LOGV(TAG, "I2S DMA config: %zu buffers x %lu frames = %lu bytes total", (size_t) SPDIF_DMA_BUFFERS_COUNT,
           (unsigned long) dma_buffer_length,
           (unsigned long) (SPDIF_DMA_BUFFERS_COUNT * dma_buffer_length * 8));  // 8 bytes per frame for 32-bit stereo

  i2s_chan_config_t chan_cfg = {
      .id = this->parent_->get_port(),
      .role = this->i2s_role_,
      .dma_desc_num = SPDIF_DMA_BUFFERS_COUNT,
      .dma_frame_num = dma_buffer_length,
      .auto_clear = true,
      .intr_priority = 3,
  };

  // SPDIF: double sample rate for BMC, 32-bit stereo, only data pin needed
  i2s_std_clk_config_t clk_cfg = {
      .sample_rate_hz = this->sample_rate_ * 2,
      .clk_src = clk_src,
      .mclk_multiple = this->mclk_multiple_,
  };

  i2s_std_slot_config_t slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);

  i2s_std_gpio_config_t gpio_cfg = {
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

  i2s_std_config_t std_cfg = {
      .clk_cfg = clk_cfg,
      .slot_cfg = slot_cfg,
      .gpio_cfg = gpio_cfg,
  };

  esp_err_t err = this->init_i2s_channel_(chan_cfg, std_cfg, SPDIF_I2S_EVENT_QUEUE_COUNT);
  if (err != ESP_OK) {
    return err;
  }

  // Channel is NOT enabled here. The speaker task will preload DMA buffers
  // with SPDIF-encoded silence before enabling, ensuring the first data on
  // the wire is valid SPDIF (not raw zeros from auto_clear) and preventing
  // phantom DMA events before real audio data is available.

  return ESP_OK;
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32 && USE_I2S_AUDIO_SPDIF_MODE
