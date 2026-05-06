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
static constexpr size_t I2S_EVENT_QUEUE_COUNT = DMA_BUFFERS_COUNT + 1;

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
  const size_t ring_buffer_size = this->current_stream_info_.ms_to_bytes(ring_buffer_duration);
  const uint32_t frames_to_fill_single_dma_buffer = this->current_stream_info_.ms_to_frames(DMA_BUFFER_DURATION_MS);
  const size_t bytes_to_fill_single_dma_buffer =
      this->current_stream_info_.frames_to_bytes(frames_to_fill_single_dma_buffer);

  bool successful_setup = false;
  std::unique_ptr<audio::AudioSourceTransferBuffer> transfer_buffer =
      audio::AudioSourceTransferBuffer::create(bytes_to_fill_single_dma_buffer);

  if (transfer_buffer != nullptr) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = RingBuffer::create(ring_buffer_size);
    if (temp_ring_buffer.use_count() == 1) {
      transfer_buffer->set_source(temp_ring_buffer);
      this->audio_ring_buffer_ = temp_ring_buffer;
      successful_setup = true;
    }
  }

  if (!successful_setup) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  } else {
    bool stop_gracefully = false;
    bool tx_dma_underflow = true;

    uint32_t frames_written = 0;
    uint32_t last_data_received_time = millis();

    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);

    // Main speaker task loop. Continues while:
    // - Paused, OR
    // - No timeout configured, OR
    // - Timeout hasn't elapsed since last data
    while (this->pause_state_ || !this->timeout_.has_value() ||
           (millis() - last_data_received_time) <= this->timeout_.value()) {
      uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
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

      int64_t write_timestamp;
      while (xQueueReceive(this->i2s_event_queue_, &write_timestamp, 0)) {
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

        // Standard I2S mode: fire callback immediately for each event
        if (frames_sent > 0) {
          this->audio_output_callback_(frames_sent, write_timestamp);
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

      size_t bytes_read = transfer_buffer->transfer_data_from_source(pdMS_TO_TICKS(read_delay));
      uint8_t *new_data = transfer_buffer->get_buffer_end() - bytes_read;

      if (bytes_read > 0) {
        this->apply_software_volume_(new_data, bytes_read);
        this->swap_esp32_mono_samples_(new_data, bytes_read);
      }

      if (transfer_buffer->available() == 0) {
        if (stop_gracefully && tx_dma_underflow) {
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS / 2));
      } else {
        size_t bytes_written = 0;

        if (tx_dma_underflow) {
          // Temporarily disable channel and callback to reset the I2S driver's internal DMA buffer queue
          i2s_channel_disable(this->tx_handle_);
          const i2s_event_callbacks_t null_callbacks = {.on_sent = nullptr};
          i2s_channel_register_event_callback(this->tx_handle_, &null_callbacks, this);
          i2s_channel_preload_data(this->tx_handle_, transfer_buffer->get_buffer_start(), transfer_buffer->available(),
                                   &bytes_written);
        } else {
          // Audio is already playing, use regular write to add to the DMA buffers
          i2s_channel_write(this->tx_handle_, transfer_buffer->get_buffer_start(), transfer_buffer->available(),
                            &bytes_written, DMA_BUFFER_DURATION_MS);
        }

        if (bytes_written > 0) {
          last_data_received_time = millis();
          frames_written += this->current_stream_info_.bytes_to_frames(bytes_written);
          transfer_buffer->decrease_buffer_length(bytes_written);

          if (tx_dma_underflow) {
            tx_dma_underflow = false;
            // Enable the on_sent callback and channel after preload
            xQueueReset(this->i2s_event_queue_);
            const i2s_event_callbacks_t callbacks = {.on_sent = i2s_on_sent_cb};
            i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);
            i2s_channel_enable(this->tx_handle_);
          }
        }
      }
    }
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPING);

  if (transfer_buffer != nullptr) {
    transfer_buffer.reset();
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

  i2s_channel_enable(this->tx_handle_);

  return ESP_OK;
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
