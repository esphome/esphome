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

// Number of DMA events between upstream callbacks (~16ms = 4 events x 4ms each).
// Matches non-SPDIF timing to prevent overwhelming upstream sync algorithms.
static constexpr uint32_t SPDIF_DMA_EVENTS_PER_CALLBACK = 4;

// Brief retry wait used by play() to catch short free-space windows during rapid track transitions.
static constexpr uint32_t SPDIF_PLAY_RETRY_WAIT_MS = 5;

static constexpr size_t SPDIF_I2S_EVENT_QUEUE_COUNT = 2 * SPDIF_DMA_BUFFERS_COUNT;

// Static callback functions for SPDIF encoder (avoids std::function overhead)
static esp_err_t spdif_preload_cb(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait) {
  auto *speaker = static_cast<I2SAudioSpeakerSPDIF *>(user_ctx);
  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_preload_data(speaker->get_tx_handle(), data, size, &bytes_written);
  if (err != ESP_OK || bytes_written != size) {
    ESP_LOGV(TAG, "Preload failed: %s (wrote %zu/%zu bytes)", esp_err_to_name(err), bytes_written, size);
    return (err != ESP_OK) ? err : ESP_ERR_NO_MEM;
  }
  return ESP_OK;
}

static esp_err_t spdif_write_cb(void *user_ctx, uint32_t *data, size_t size, TickType_t ticks_to_wait) {
  auto *speaker = static_cast<I2SAudioSpeakerSPDIF *>(user_ctx);
  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(speaker->get_tx_handle(), data, size, &bytes_written, ticks_to_wait);
  if (err != ESP_OK) {
    ESP_LOGV(TAG, "I2S write failed: %s (wrote %zu/%zu bytes)", esp_err_to_name(err), bytes_written, size);
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

  // Reset lockstep records queue so it starts paired with the (also-reset) i2s_event_queue_.
  xQueueReset(this->write_records_queue_);

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
    // Each preloaded block pushes a 0-real-frame record so that the corresponding
    // on_sent events drain in lockstep without crediting any audio frames.
    this->spdif_encoder_->set_preload_mode(true);
    for (size_t i = 0; i < SPDIF_DMA_BUFFERS_COUNT; i++) {
      esp_err_t preload_err = this->spdif_encoder_->flush_with_silence(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
      if (preload_err != ESP_OK) {
        break;  // DMA preload buffer full or error
      }
      const uint32_t silence_record = 0;
      xQueueSendToBack(this->write_records_queue_, &silence_record, 0);
    }
    this->spdif_encoder_->set_preload_mode(false);
    this->spdif_encoder_->reset();  // Clean encoder state for the main loop

    // Now register the callback and enable the channel
    xQueueReset(this->i2s_event_queue_);
    const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
    i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
    i2s_channel_enable(this->tx_handle_);

    // Always-fill model: each iteration produces exactly one SPDIF block (= one DMA buffer).
    // We drain real PCM up to one block from the ring buffer and silence-pad any remainder.
    // Blocking writes pace the loop at the DMA consumption rate. This mirrors the standard
    // I2S speaker pattern (PR #16317): fill what you can, then silence-pad whatever is still
    // missing to complete the DMA buffer.
    const uint32_t block_duration_us = this->current_stream_info_.frames_to_microseconds(SPDIF_BLOCK_SAMPLES);
    // Sized to absorb the worst case where every DMA buffer is full when we issue the write.
    const TickType_t write_timeout_ticks =
        pdMS_TO_TICKS(((block_duration_us * (SPDIF_DMA_BUFFERS_COUNT + 1)) + 999) / 1000);
    // Brief read budget when the ring buffer is empty (~half a block).
    const TickType_t read_timeout_ticks = pdMS_TO_TICKS(((block_duration_us / 2) + 999) / 1000);

    // SPDIF Callback Decimation: fire every 4th DMA event (~16ms), matching non-SPDIF timing.
    uint32_t spdif_pending_frames = 0;
    int64_t spdif_pending_timestamp = 0;
    uint32_t spdif_dma_event_count = 0;

    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);

    // SPDIF continuous mode: loop runs indefinitely, outputting silence when no audio data
    // to keep the receiver synced. Exits only via break (stream info change, silence timeout,
    // lockstep desync, dropped event, or partial-write failure).
    while (true) {
      uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
        // The ISR pairs COMMAND_STOP with ERR_DROPPED_EVENT when it has to discard a completion
        // event; that desyncs the lockstep queues permanently and the only safe recovery is a full
        // task restart.
        if (event_group_bits & SpeakerEventGroupBits::ERR_DROPPED_EVENT) {
          ESP_LOGV(TAG, "Exiting: ISR dropped event, restarting to recover lockstep");
          break;
        }
        // User-initiated stop. In SPDIF continuous mode, transition to silence output rather
        // than tearing the task down.
        this->spdif_silence_start_ = millis();
        ESP_LOGV(TAG, "COMMAND_STOP received, continuing in silence mode");
      }
      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY) {
        // SPDIF continuous mode never tears the channel down on graceful stop. Clear the flag and
        // let the audio simply drain through the always-fill loop into the silence-timeout path.
        xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
      }

      if (this->audio_stream_info_ != this->current_stream_info_) {
        ESP_LOGV(TAG, "Exiting: stream info changed");
        break;
      }

      // Drain ISR completion events, popping a matching record for each.
      int64_t write_timestamp;
      bool lockstep_broken = false;
      while (xQueueReceive(this->i2s_event_queue_, &write_timestamp, 0)) {
        // Lockstep: pop the matching record (real audio frames packed into this DMA block).
        // Records are pushed by the task right after each successful block commit, so the FIFO
        // order matches DMA completion order. Empty records queue here means lockstep broke.
        uint32_t real_frames = 0;
        if (xQueueReceive(this->write_records_queue_, &real_frames, 0) != pdTRUE) {
          ESP_LOGV(TAG, "Event without matching write record");
          xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_LOCKSTEP_DESYNC);
          lockstep_broken = true;
          break;
        }

        // Per-block timestamp adjustment: shift back by the silence-padding portion of the block
        // so the reported timestamp reflects when the last real sample left the wire.
        uint32_t frames_sent = real_frames;
        if (real_frames < SPDIF_BLOCK_SAMPLES) {
          const uint32_t frames_zeroed = SPDIF_BLOCK_SAMPLES - real_frames;
          write_timestamp -= this->current_stream_info_.frames_to_microseconds(frames_zeroed);
        }

        spdif_dma_event_count++;
        // Accumulate frames; keep the latest timestamp so the callback reports when the last
        // sample left the wire, not the first.
        if (frames_sent > 0) {
          spdif_pending_timestamp = write_timestamp;
          spdif_pending_frames += frames_sent;
        }

        bool decimation_reached = (spdif_dma_event_count >= SPDIF_DMA_EVENTS_PER_CALLBACK);
        // Partial blocks mark an end-of-stream boundary (silence-padded tail). Fire immediately
        // so the back-shifted timestamp isn't overwritten by a later full audio block landing
        // in the same decimation window.
        bool partial_flush = (real_frames > 0 && real_frames < SPDIF_BLOCK_SAMPLES);

        if (decimation_reached || partial_flush) {
          if (spdif_pending_frames > 0) {
            this->audio_output_callback_(spdif_pending_frames, spdif_pending_timestamp);
            spdif_pending_frames = 0;
          }
          spdif_dma_event_count = 0;
        }
      }
      if (lockstep_broken) {
        ESP_LOGV(TAG, "Exiting: lockstep desync, restarting task");
        break;
      }

      // Always-fill: produce exactly one SPDIF block this iteration. The blocking encoder write
      // paces the task at the DMA consumption rate.
      uint32_t real_frames_in_block = 0;
      bool block_committed = false;
      bool partial_write_failure = false;

      if (!this->pause_state_) {
        while (real_frames_in_block < SPDIF_BLOCK_SAMPLES) {
          if (transfer_buffer->available() == 0) {
            size_t bytes_read = transfer_buffer->transfer_data_from_source(read_timeout_ticks);
            if (bytes_read == 0) {
              break;  // No upstream data within the read budget; silence-pad the remainder.
            }
            uint8_t *new_data = transfer_buffer->get_buffer_end() - bytes_read;
            this->apply_software_volume_(new_data, bytes_read);
            this->swap_esp32_mono_samples_(new_data, bytes_read);
          }

          const uint32_t frames_still_needed = SPDIF_BLOCK_SAMPLES - real_frames_in_block;
          const size_t bytes_still_needed = this->current_stream_info_.frames_to_bytes(frames_still_needed);
          const size_t bytes_to_feed = std::min(transfer_buffer->available(), bytes_still_needed);

          uint32_t blocks_sent = 0;
          size_t pcm_consumed = 0;
          esp_err_t err = this->spdif_encoder_->write(transfer_buffer->get_buffer_start(), bytes_to_feed,
                                                      write_timeout_ticks, &blocks_sent, &pcm_consumed);
          if (err != ESP_OK) {
            // A failed (or timed-out) send leaves an unsent block in the encoder's stitch buffer;
            // resuming would credit the next iteration's bytes against an old block. Bail and
            // let loop() restart the task with a clean encoder.
            xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_PARTIAL_WRITE);
            partial_write_failure = true;
            break;
          }

          if (pcm_consumed > 0) {
            transfer_buffer->decrease_buffer_length(pcm_consumed);
            real_frames_in_block += this->current_stream_info_.bytes_to_frames(pcm_consumed);
          }
          if (blocks_sent > 0) {
            block_committed = true;
            break;
          }
        }
      }

      if (partial_write_failure) {
        break;
      }

      if (!block_committed) {
        // Pad whatever real audio we managed to feed (if any) with silence to complete one block,
        // or emit a full silence block if the encoder is empty.
        esp_err_t err = this->spdif_encoder_->flush_with_silence(write_timeout_ticks);
        if (err != ESP_OK) {
          xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_PARTIAL_WRITE);
          break;
        }
      }

      // One block committed to DMA; push exactly one record carrying its real-audio frame count.
      // Failure here means the records queue is full, which violates the lockstep invariant.
      if (xQueueSendToBack(this->write_records_queue_, &real_frames_in_block, 0) != pdTRUE) {
        xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_LOCKSTEP_DESYNC);
        break;
      }

      // Silence-timeout tracking and graceful-stop reset.
      if (real_frames_in_block == 0) {
        if (this->spdif_silence_start_ == 0) {
          this->spdif_silence_start_ = millis();
        }

        if (this->timeout_.has_value()) {
          const uint32_t silence_duration = millis() - this->spdif_silence_start_;
          if (silence_duration >= this->timeout_.value()) {
            ESP_LOGV(TAG, "Silence timeout reached (%" PRIu32 "ms) - stopping speaker", silence_duration);
            break;
          }
        }
      } else if (this->spdif_silence_start_ != 0) {
        uint32_t silence_duration = millis() - this->spdif_silence_start_;
        if (silence_duration > 100) {
          ESP_LOGV(TAG, "Exiting silence mode after %" PRIu32 "ms, have audio data", silence_duration);
        }
        this->spdif_silence_start_ = 0;
      }
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
