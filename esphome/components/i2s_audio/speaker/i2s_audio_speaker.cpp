#include "i2s_audio_speaker.h"

#ifdef USE_ESP32

#ifdef USE_I2S_LEGACY
#include <driver/i2s.h>
#else
#include <driver/i2s_std.h>
#endif

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

namespace esphome {
namespace i2s_audio {

static const uint32_t DMA_BUFFER_DURATION_MS = 15;
static const size_t DMA_BUFFERS_COUNT = 4;

static const size_t TASK_STACK_SIZE = 4096;
static const ssize_t TASK_PRIORITY = 19;

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

void I2SAudioSpeaker::setup() {
  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }
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
#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_DAC
  ESP_LOGCONFIG(TAG, "  Internal DAC mode: %d", static_cast<int8_t>(this->internal_dac_mode_));
#endif
  ESP_LOGCONFIG(TAG, "  Communication format: %d", static_cast<int8_t>(this->i2s_comm_fmt_));
#else
  ESP_LOGCONFIG(TAG, "  Communication format: %s", this->i2s_comm_fmt_.c_str());
#endif
}

void I2SAudioSpeaker::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if ((event_group_bits & SpeakerEventGroupBits::COMMAND_START) && (this->state_ == speaker::STATE_STOPPED)) {
    this->state_ = speaker::STATE_STARTING;
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
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
        this->status_momentary_error("driver-faiure", 1000);
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
#endif
  {
    // Fallback to software volume control by using a Q15 fixed point scaling factor
    ssize_t decibel_index = remap<ssize_t, float>(volume, 0.0f, 1.0f, 0, Q15_VOLUME_SCALING_FACTORS.size() - 1);
    this->q15_volume_factor_ = Q15_VOLUME_SCALING_FACTORS[decibel_index];
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
#endif
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

  const uint32_t dma_buffers_duration_ms = DMA_BUFFER_DURATION_MS * DMA_BUFFERS_COUNT;
  // Ensure ring buffer duration is at least the duration of all DMA buffers
  const uint32_t ring_buffer_duration = std::max(dma_buffers_duration_ms, this_speaker->buffer_duration_ms_);

  // The DMA buffers may have more bits per sample, so calculate buffer sizes based in the input audio stream info
  const size_t ring_buffer_size = this_speaker->current_stream_info_.ms_to_bytes(ring_buffer_duration);

  const uint32_t frames_to_fill_single_dma_buffer =
      this_speaker->current_stream_info_.ms_to_frames(DMA_BUFFER_DURATION_MS);
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

    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::TASK_RUNNING);

    while (this_speaker->pause_state_ || !this_speaker->timeout_.has_value() ||
           (millis() - last_data_received_time) <= this_speaker->timeout_.value()) {
      uint32_t event_group_bits = xEventGroupGetBits(this_speaker->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        xEventGroupClearBits(this_speaker->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
        break;
      }
      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY) {
        xEventGroupClearBits(this_speaker->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
        stop_gracefully = true;
      }

      if (this_speaker->audio_stream_info_ != this_speaker->current_stream_info_) {
        // Audio stream info changed, stop the speaker task so it will restart with the proper settings.
        break;
      }
#ifdef USE_I2S_LEGACY
      i2s_event_t i2s_event;
      while (xQueueReceive(this_speaker->i2s_event_queue_, &i2s_event, 0)) {
        if (i2s_event.type == I2S_EVENT_TX_Q_OVF) {
          tx_dma_underflow = true;
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
        if (frames_sent > 0) {
          this_speaker->audio_output_callback_(frames_sent, write_timestamp);
        }
      }
#endif

      if (this_speaker->pause_state_) {
        // Pause state is accessed atomically, so thread safe
        // Delay so the task yields, then skip transferring audio data
        vTaskDelay(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
        continue;
      }

      // Wait half the duration of the data already written to the DMA buffers for new audio data
      // The millisecond helper modifies the frames_written variable, so use the microsecond helper and divide by 1000
      const uint32_t read_delay =
          (this_speaker->current_stream_info_.frames_to_microseconds(frames_written) / 1000) / 2;

      uint8_t *new_data = transfer_buffer->get_buffer_end();  // track start of any newly copied bytes
      size_t bytes_read = transfer_buffer->transfer_data_from_source(pdMS_TO_TICKS(read_delay));

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
#endif
      }

      if (transfer_buffer->available() == 0) {
        if (stop_gracefully && tx_dma_underflow) {
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS / 2));
      } else {
        size_t bytes_written = 0;
#ifdef USE_I2S_LEGACY
        if (this_speaker->current_stream_info_.get_bits_per_sample() == (uint8_t) this_speaker->bits_per_sample_) {
          i2s_write(this_speaker->parent_->get_port(), transfer_buffer->get_buffer_start(),
                    transfer_buffer->available(), &bytes_written, pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
        } else if (this_speaker->current_stream_info_.get_bits_per_sample() <
                   (uint8_t) this_speaker->bits_per_sample_) {
          i2s_write_expand(this_speaker->parent_->get_port(), transfer_buffer->get_buffer_start(),
                           transfer_buffer->available(), this_speaker->current_stream_info_.get_bits_per_sample(),
                           this_speaker->bits_per_sample_, &bytes_written, pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS));
        }
#else
        if (tx_dma_underflow) {
          // Temporarily disable channel and callback to reset the I2S driver's internal DMA buffer queue so timing
          // callbacks are accurate. Preload the data.
          i2s_channel_disable(this_speaker->tx_handle_);
          const i2s_event_callbacks_t callbacks = {
              .on_sent = nullptr,
          };

          i2s_channel_register_event_callback(this_speaker->tx_handle_, &callbacks, this_speaker);
          i2s_channel_preload_data(this_speaker->tx_handle_, transfer_buffer->get_buffer_start(),
                                   transfer_buffer->available(), &bytes_written);
        } else {
          // Audio is already playing, use regular I2S write to add to the DMA buffers
          i2s_channel_write(this_speaker->tx_handle_, transfer_buffer->get_buffer_start(), transfer_buffer->available(),
                            &bytes_written, DMA_BUFFER_DURATION_MS);
        }
#endif
        if (bytes_written > 0) {
          last_data_received_time = millis();
          frames_written += this_speaker->current_stream_info_.bytes_to_frames(bytes_written);
          transfer_buffer->decrease_buffer_length(bytes_written);
          if (tx_dma_underflow) {
            tx_dma_underflow = false;
#ifndef USE_I2S_LEGACY
            // Reset the event queue timestamps
            // Enable the on_sent callback to accurately track the timestamps of played audio
            // Enable the I2S channel to start sending the preloaded audio

            xQueueReset(this_speaker->i2s_event_queue_);

            const i2s_event_callbacks_t callbacks = {
                .on_sent = i2s_on_sent_cb,
            };
            i2s_channel_register_event_callback(this_speaker->tx_handle_, &callbacks, this_speaker);

            i2s_channel_enable(this_speaker->tx_handle_);
#endif
          }
#ifdef USE_I2S_LEGACY
          // The legacy driver doesn't easily support the callback approach for timestamps, so fall back to a direct but
          // less accurate approach.
          this_speaker->audio_output_callback_(this_speaker->current_stream_info_.bytes_to_frames(bytes_written),
                                               esp_timer_get_time() + dma_buffers_duration_ms * 1000);
#endif
        }
      }
    }
  }

  xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::TASK_STOPPING);

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

  if (wait_on_empty) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
  } else {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
  }
}

esp_err_t I2SAudioSpeaker::start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info) {
  this->current_stream_info_ = audio_stream_info;  // store the stream info settings the driver will use

#ifdef USE_I2S_LEGACY
  if ((this->i2s_mode_ & I2S_MODE_SLAVE) && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {  // NOLINT
#else
  if ((this->i2s_role_ & I2S_ROLE_SLAVE) && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {  // NOLINT
#endif
    // Can't reconfigure I2S bus, so the sample rate must match the configured value
    ESP_LOGE(TAG, "Audio stream settings are not compatible with this I2S configuration");
    return ESP_ERR_NOT_SUPPORTED;
  }

#ifdef USE_I2S_LEGACY
  if ((i2s_bits_per_sample_t) audio_stream_info.get_bits_per_sample() > this->bits_per_sample_) {
#else
  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO &&
      (i2s_slot_bit_width_t) audio_stream_info.get_bits_per_sample() > this->slot_bit_width_) {
#endif
    // Currently can't handle the case when the incoming audio has more bits per sample than the configured value
    ESP_LOGE(TAG, "Audio streams with more bits per sample than the I2S speaker's configuration is not supported");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (!this->parent_->try_lock()) {
    ESP_LOGE(TAG, "Parent I2S bus not free");
    return ESP_ERR_INVALID_STATE;
  }

  uint32_t dma_buffer_length = audio_stream_info.ms_to_frames(DMA_BUFFER_DURATION_MS);

#ifdef USE_I2S_LEGACY
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
#endif
  };
#if SOC_I2S_SUPPORTS_DAC
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    config.mode = (i2s_mode_t) (config.mode | I2S_MODE_DAC_BUILT_IN);
  }
#endif

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
#endif
    i2s_pin_config_t pin_config = this->parent_->get_pin_config();
    pin_config.data_out_num = this->dout_pin_;

    err = i2s_set_pin(this->parent_->get_port(), &pin_config);
#if SOC_I2S_SUPPORTS_DAC
  } else {
    i2s_set_dac_mode(this->internal_dac_mode_);
  }
#endif

  if (err != ESP_OK) {
    // Failed to set the data out pin, so uninstall the driver and unlock the I2S port
    ESP_LOGE(TAG, "Failed to set the data out pin");
    i2s_driver_uninstall(this->parent_->get_port());
    this->parent_->unlock();
  }
#else
  i2s_chan_config_t chan_cfg = {
      .id = this->parent_->get_port(),
      .role = this->i2s_role_,
      .dma_desc_num = DMA_BUFFERS_COUNT,
      .dma_frame_num = dma_buffer_length,
      .auto_clear = true,
      .intr_priority = 3,
  };
  /* Allocate a new TX channel and get the handle of this channel */
  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to allocate new I2S channel");
    this->parent_->unlock();
    return err;
  }

  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;
#ifdef I2S_CLK_SRC_APLL
  if (this->use_apll_) {
    clk_src = I2S_CLK_SRC_APLL;
  }
#endif
  i2s_std_gpio_config_t pin_config = this->parent_->get_pin_config();

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

  i2s_std_slot_config_t std_slot_cfg;
  if (this->i2s_comm_fmt_ == "std") {
    std_slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(), slot_mode);
  } else if (this->i2s_comm_fmt_ == "pcm") {
    std_slot_cfg =
        I2S_STD_PCM_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(), slot_mode);
  } else {
    std_slot_cfg =
        I2S_STD_MSB_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) audio_stream_info.get_bits_per_sample(), slot_mode);
  }
#ifdef USE_ESP32_VARIANT_ESP32
  // There seems to be a bug on the ESP32 (non-variant) platform where setting the slot bit width higher then the bits
  // per sample causes the audio to play too fast. Setting the ws_width to the configured slot bit width seems to
  // make it play at the correct speed while sending more bits per slot.
  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO) {
    uint32_t configured_bit_width = static_cast<uint32_t>(this->slot_bit_width_);
    std_slot_cfg.ws_width = configured_bit_width;
    if (configured_bit_width > 16) {
      std_slot_cfg.msb_right = false;
    }
  }
#else
  std_slot_cfg.slot_bit_width = this->slot_bit_width_;
#endif
  std_slot_cfg.slot_mask = slot_mask;

  pin_config.dout = this->dout_pin_;

  i2s_std_config_t std_cfg = {
      .clk_cfg = clk_cfg,
      .slot_cfg = std_slot_cfg,
      .gpio_cfg = pin_config,
  };
  /* Initialize the channel */
  err = i2s_channel_init_std_mode(this->tx_handle_, &std_cfg);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize channel");
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
    this->parent_->unlock();
    return err;
  }
  if (this->i2s_event_queue_ == nullptr) {
    this->i2s_event_queue_ = xQueueCreate(I2S_EVENT_QUEUE_COUNT, sizeof(int64_t));
  }

  i2s_channel_enable(this->tx_handle_);
#endif

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
#endif

void I2SAudioSpeaker::stop_i2s_driver_() {
#ifdef USE_I2S_LEGACY
  i2s_driver_uninstall(this->parent_->get_port());
#else
  i2s_channel_disable(this->tx_handle_);
  i2s_del_channel(this->tx_handle_);
  this->tx_handle_ = nullptr;
#endif
  this->parent_->unlock();
}

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
