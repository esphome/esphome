#include "i2s_audio_speaker_standard.h"

#ifdef USE_ESP32

#include <driver/i2s_std.h>

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio.speaker.std";

static constexpr size_t DMA_BUFFERS_COUNT = 4;
// Sized to comfortably absorb scheduling jitter: at most DMA_BUFFERS_COUNT events can be in flight,
// doubled so that a transient backlog never overruns the queue (which would desync the lockstep
// invariant between i2s_event_queue_ and write_records_queue_).
static constexpr size_t I2S_EVENT_QUEUE_COUNT = DMA_BUFFERS_COUNT * 2;
// Generous timeout for ``i2s_channel_write`` blocking. A buffer frees roughly every
// DMA_BUFFER_DURATION_MS, so a multiple of that gives plenty of slack against scheduling jitter
// without masking real failures.
static constexpr TickType_t WRITE_TIMEOUT_TICKS = pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS * (DMA_BUFFERS_COUNT + 1));

void I2SAudioSpeaker::dump_config() {
  I2SAudioSpeakerBase::dump_config();
  const char *fmt_str;
  switch (this->i2s_comm_fmt_) {
    case I2SCommFmt::PCM:
      fmt_str = "pcm";
      break;
    case I2SCommFmt::MSB:
      fmt_str = "msb";
      break;
    default:
      fmt_str = "std";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Communication format: %s", fmt_str);
}

void I2SAudioSpeaker::run_speaker_task() {
  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STARTING);

  const uint32_t dma_buffers_duration_ms = DMA_BUFFER_DURATION_MS * DMA_BUFFERS_COUNT;
  // Ensure ring buffer duration is at least the duration of all DMA buffers
  const uint32_t ring_buffer_duration = std::max(dma_buffers_duration_ms, this->buffer_duration_ms_);

  // The DMA buffers may have more bits per sample, so calculate buffer sizes based on the input audio stream info
  const size_t bytes_per_frame = this->current_stream_info_.frames_to_bytes(1);
  // Round the ring buffer size down to a multiple of bytes_per_frame so the wrap boundary stays frame-aligned and
  // avoids unnecessary single-frame splices.
  const size_t ring_buffer_size =
      (this->current_stream_info_.ms_to_bytes(ring_buffer_duration) / bytes_per_frame) * bytes_per_frame;
  const uint32_t frames_per_dma_buffer = this->current_stream_info_.ms_to_frames(DMA_BUFFER_DURATION_MS);
  const size_t dma_buffer_bytes = this->current_stream_info_.frames_to_bytes(frames_per_dma_buffer);

  bool successful_setup = false;

  std::unique_ptr<audio::RingBufferAudioSource> audio_source;

  // Pre-zeroed buffer used to silence-pad each DMA descriptor whenever real audio doesn't fully fill it.
  RAMAllocator<uint8_t> silence_allocator;
  uint8_t *silence_buffer = silence_allocator.allocate(dma_buffer_bytes);

  if (silence_buffer != nullptr) {
    memset(silence_buffer, 0, dma_buffer_bytes);

    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = ring_buffer::RingBuffer::create(ring_buffer_size);
    audio_source =
        audio::RingBufferAudioSource::create(temp_ring_buffer, dma_buffer_bytes, static_cast<uint8_t>(bytes_per_frame));

    if (audio_source != nullptr) {
      // audio_source is nullptr if the ring buffer fails to allocate
      this->audio_ring_buffer_ = temp_ring_buffer;
      successful_setup = true;
    }
  }

  if (successful_setup) {
    // Preload every DMA descriptor with silence and push a matching zero-real-frames record per buffer.
    // This guarantees that every on_sent event has a corresponding write record from the start, so
    // ``i2s_event_queue_`` and ``write_records_queue_`` stay in lockstep for the entire task lifetime.
    for (size_t i = 0; i < DMA_BUFFERS_COUNT; i++) {
      size_t bytes_loaded = 0;
      esp_err_t err = i2s_channel_preload_data(this->tx_handle_, silence_buffer, dma_buffer_bytes, &bytes_loaded);
      if (err != ESP_OK || bytes_loaded != dma_buffer_bytes) {
        ESP_LOGV(TAG, "Failed to preload silence into DMA buffer %u (err=%d, loaded=%u)", (unsigned) i, (int) err,
                 (unsigned) bytes_loaded);
        successful_setup = false;
        break;
      }
      uint32_t zero_real_frames = 0;
      if (xQueueSend(this->write_records_queue_, &zero_real_frames, 0) != pdTRUE) {
        // Should never happen: the queue was just reset and is sized for DMA_BUFFERS_COUNT * 2 entries.
        ESP_LOGV(TAG, "Failed to push preload write record");
        successful_setup = false;
        break;
      }
    }
  }

  if (successful_setup) {
    // Register the on_sent callback BEFORE enabling the channel so the very first transmitted buffer
    // generates a queued event that pairs with the first preloaded silence record.
    const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
    i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);

    if (i2s_channel_enable(this->tx_handle_) != ESP_OK) {
      ESP_LOGV(TAG, "Failed to enable I2S channel");
      successful_setup = false;
    }
  }

  if (!successful_setup) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  } else {
    bool stop_gracefully = false;
    // Number of records currently in ``write_records_queue_`` that carry real audio. Used by graceful
    // stop to wait until every real-audio buffer has been confirmed played by an ISR event.
    uint32_t pending_real_buffers = 0;
    uint32_t last_data_received_time = millis();

    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);

    // Main speaker task loop. Continues while:
    // - Paused, OR
    // - No timeout configured, OR
    // - Timeout hasn't elapsed since last data
    //
    // Always-fill model: every iteration writes exactly one DMA buffer's worth, mixing real audio
    // and silence padding as needed. The blocking ``i2s_channel_write`` paces the loop at the DMA
    // consumption rate, and every buffer write is matched 1:1 with a record on ``write_records_queue_``.
    //
    // While paused, the real-audio fill is skipped and the entire DMA buffer is filled with silence;
    // the same blocking ``i2s_channel_write`` provides natural pacing (one buffer per ~DMA_BUFFER_DURATION_MS),
    // so the lockstep invariant is preserved without burning CPU.
    while (this->pause_state_ || !this->timeout_.has_value() ||
           (millis() - last_data_received_time) <= this->timeout_.value()) {
      uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        // COMMAND_STOP is set both by user-initiated stop() and by the ISR when it drops a completion
        // event (paired with ERR_DROPPED_EVENT so loop() can distinguish the two cases).
        xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
        ESP_LOGV(TAG, "Exiting: COMMAND_STOP received");
        break;
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

      // Drain ISR-stamped completion events. Each event corresponds 1:1 with a write_records_queue_
      // entry by construction (preloaded records at startup, plus exactly one record pushed per
      // iteration alongside exactly one DMA-buffer-sized write).
      int64_t write_timestamp;
      bool lockstep_broken = false;
      while (xQueueReceive(this->i2s_event_queue_, &write_timestamp, 0)) {
        uint32_t real_frames = 0;
        if (xQueueReceive(this->write_records_queue_, &real_frames, 0) != pdTRUE) {
          // Should never happen: would indicate the lockstep invariant is broken.
          ESP_LOGV(TAG, "Event without matching write record");
          xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_LOCKSTEP_DESYNC);
          lockstep_broken = true;
          break;
        }
        if (real_frames > 0) {
          pending_real_buffers--;
          // Real audio is packed at the start of each DMA buffer with any silence padding on the
          // tail, so the real audio finished playing earlier than the buffer-completion timestamp
          // by the duration of the trailing zeros.
          const uint32_t silence_frames = frames_per_dma_buffer - real_frames;
          const int64_t adjusted_ts =
              write_timestamp - this->current_stream_info_.frames_to_microseconds(silence_frames);
          this->audio_output_callback_(real_frames, adjusted_ts);
        }
      }
      if (lockstep_broken) {
        break;
      }

      // Graceful stop: exit only after the source's exposed chunk is drained, the underlying ring
      // buffer has nothing left to hand over, and every real-audio buffer we submitted has been
      // confirmed played. ``has_buffered_data()`` returns bytes still sitting in the ring buffer
      // awaiting fill().
      if (stop_gracefully && audio_source->available() == 0 && !this->has_buffered_data() &&
          pending_real_buffers == 0) {
        ESP_LOGV(TAG, "Exiting: graceful stop complete");
        break;
      }

      // Compose exactly one DMA buffer's worth: drain as much real audio as the source currently
      // exposes (may take multiple fill() calls when crossing a ring buffer wrap), then pad any
      // remainder with silence. All writes pack into the next free DMA descriptor in order, so the
      // descriptor ends up holding [real audio][silence padding].
      size_t bytes_written_total = 0;
      size_t real_bytes_total = 0;
      bool partial_write_failure = false;

      if (!this->pause_state_) {
        while (bytes_written_total < dma_buffer_bytes) {
          size_t bytes_read = audio_source->fill(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS) / 2, false);
          if (bytes_read > 0) {
            uint8_t *new_data = audio_source->mutable_data() + audio_source->available() - bytes_read;
            this->apply_software_volume_(new_data, bytes_read);
            this->swap_esp32_mono_samples_(new_data, bytes_read);
          }

          const size_t to_write = std::min(audio_source->available(), dma_buffer_bytes - bytes_written_total);
          if (to_write == 0) {
            // Ring buffer has nothing more to hand over right now; pad the rest of this DMA buffer
            // with silence so the lockstep invariant (one write per iteration) is preserved.
            break;
          }

          size_t bw = 0;
          i2s_channel_write(this->tx_handle_, audio_source->data(), to_write, &bw, WRITE_TIMEOUT_TICKS);
          if (bw != to_write) {
            // A short real-audio write breaks DMA descriptor alignment for every subsequent event;
            // the only safe recovery is to restart the task.
            ESP_LOGV(TAG, "Partial real audio write: %u of %u bytes", (unsigned) bw, (unsigned) to_write);
            xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_PARTIAL_WRITE);
            partial_write_failure = true;
            break;
          }
          audio_source->consume(bw);
          bytes_written_total += bw;
          real_bytes_total += bw;
        }
        if (real_bytes_total > 0) {
          last_data_received_time = millis();
        }
      }

      if (partial_write_failure) {
        break;
      }

      const size_t silence_bytes = dma_buffer_bytes - bytes_written_total;
      if (silence_bytes > 0) {
        size_t bw = 0;
        i2s_channel_write(this->tx_handle_, silence_buffer, silence_bytes, &bw, WRITE_TIMEOUT_TICKS);
        if (bw != silence_bytes) {
          // Same descriptor-alignment hazard as a partial real-audio write.
          ESP_LOGV(TAG, "Partial silence write: %u of %u bytes", (unsigned) bw, (unsigned) silence_bytes);
          xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_PARTIAL_WRITE);
          break;
        }
      }

      const uint32_t real_frames_in_buffer = this->current_stream_info_.bytes_to_frames(real_bytes_total);
      // Push the matching write record. Capacity headroom in I2S_EVENT_QUEUE_COUNT guarantees this
      // succeeds even with a transient backlog of unprocessed events; if it ever fails the lockstep
      // invariant is broken and every subsequent timestamp would be silently wrong, so bail.
      if (xQueueSend(this->write_records_queue_, &real_frames_in_buffer, 0) != pdTRUE) {
        ESP_LOGV(TAG, "Exiting: write records queue full");
        xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_LOCKSTEP_DESYNC);
        break;
      }
      if (real_frames_in_buffer > 0) {
        pending_real_buffers++;
      }
    }
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPING);

  audio_source.reset();

  if (silence_buffer != nullptr) {
    silence_allocator.deallocate(silence_buffer, dma_buffer_bytes);
    silence_buffer = nullptr;
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPED);

  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

esp_err_t I2SAudioSpeaker::start_i2s_driver(audio::AudioStreamInfo &audio_stream_info) {
  this->current_stream_info_ = audio_stream_info;

  if ((this->i2s_role_ & I2S_ROLE_SLAVE) && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {  // NOLINT
    // Can't reconfigure I2S bus, so the sample rate must match the configured value
    ESP_LOGE(TAG, "Incompatible stream settings");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO &&
      (i2s_slot_bit_width_t) audio_stream_info.get_bits_per_sample() > this->slot_bit_width_) {
    // Currently can't handle the case when the incoming audio has more bits per sample than the configured value
    ESP_LOGE(TAG, "Stream bits per sample must be less than or equal to the speaker's configuration");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (!this->parent_->try_lock()) {
    ESP_LOGE(TAG, "Parent bus is busy");
    return ESP_ERR_INVALID_STATE;
  }

  uint32_t dma_buffer_length = audio_stream_info.ms_to_frames(DMA_BUFFER_DURATION_MS);

  i2s_role_t i2s_role = this->i2s_role_;
  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;

#if SOC_CLK_APLL_SUPPORTED
  if (this->use_apll_) {
    clk_src = i2s_clock_src_t::I2S_CLK_SRC_APLL;
  }
#endif  // SOC_CLK_APLL_SUPPORTED

  // Log DMA configuration for debugging
  ESP_LOGV(TAG, "I2S DMA config: %zu buffers x %lu frames", (size_t) DMA_BUFFERS_COUNT,
           (unsigned long) dma_buffer_length);

  i2s_chan_config_t chan_cfg = {
      .id = this->parent_->get_port(),
      .role = i2s_role,
      .dma_desc_num = DMA_BUFFERS_COUNT,
      .dma_frame_num = dma_buffer_length,
      .auto_clear = true,
      .intr_priority = 3,
  };

  // Build standard I2S clock/slot/gpio configuration
  i2s_std_clk_config_t clk_cfg = {
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

  i2s_std_slot_config_t slot_cfg;
  switch (this->i2s_comm_fmt_) {
    case I2SCommFmt::PCM:
      slot_cfg =
          I2S_STD_PCM_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(), slot_mode);
      break;
    case I2SCommFmt::MSB:
      slot_cfg =
          I2S_STD_MSB_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(), slot_mode);
      break;
    default:
      slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(),
                                                     slot_mode);
      break;
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
  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO) {
    slot_cfg.ws_width = static_cast<uint32_t>(this->slot_bit_width_);
  }
#endif  // USE_ESP32_VARIANT_ESP32
  slot_cfg.slot_mask = slot_mask;

  i2s_std_gpio_config_t gpio_cfg = this->parent_->get_pin_config();
  gpio_cfg.dout = this->dout_pin_;

  i2s_std_config_t std_cfg = {
      .clk_cfg = clk_cfg,
      .slot_cfg = slot_cfg,
      .gpio_cfg = gpio_cfg,
  };

  esp_err_t err = this->init_i2s_channel_(chan_cfg, std_cfg, I2S_EVENT_QUEUE_COUNT);
  if (err != ESP_OK) {
    return err;
  }

  // The speaker task will enable the channel after preloading.

  return ESP_OK;
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
