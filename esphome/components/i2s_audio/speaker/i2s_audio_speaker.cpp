#include "i2s_audio_speaker.h"

#ifdef USE_ESP32

#ifdef USE_I2S_LEGACY
#include <driver/i2s.h>
#else
#include <driver/i2s_std.h>
#endif

#include "esphome/components/audio/audio.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

namespace esphome {
namespace i2s_audio {

static const uint8_t DMA_BUFFER_DURATION_MS = 15;
static const size_t DMA_BUFFERS_COUNT = 4;

static const size_t TASK_DELAY_MS = DMA_BUFFER_DURATION_MS * DMA_BUFFERS_COUNT / 2;

static const size_t TASK_STACK_SIZE = 4096;
static const ssize_t TASK_PRIORITY = 23;

static const size_t I2S_EVENT_QUEUE_COUNT = DMA_BUFFERS_COUNT + 1;

static const char *const TAG = "i2s_audio.speaker";

enum SpeakerEventGroupBits : uint32_t {
  COMMAND_START = (1 << 0),            // starts the speaker task
  COMMAND_STOP = (1 << 1),             // stops the speaker task
  COMMAND_STOP_GRACEFULLY = (1 << 2),  // Stops the speaker task once all data has been written
  STATE_STARTING = (1 << 10),
  STATE_RUNNING = (1 << 11),
  STATE_STOPPING = (1 << 12),
  STATE_STOPPED = (1 << 13),
  ERR_TASK_FAILED_TO_START = (1 << 14),
  ERR_ESP_INVALID_STATE = (1 << 15),
  ERR_ESP_NOT_SUPPORTED = (1 << 16),
  ERR_ESP_INVALID_ARG = (1 << 17),
  ERR_ESP_INVALID_SIZE = (1 << 18),
  ERR_ESP_NO_MEM = (1 << 19),
  ERR_ESP_FAIL = (1 << 20),
  ALL_ERR_ESP_BITS = ERR_ESP_INVALID_STATE | ERR_ESP_NOT_SUPPORTED | ERR_ESP_INVALID_ARG | ERR_ESP_INVALID_SIZE |
                     ERR_ESP_NO_MEM | ERR_ESP_FAIL,
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

// Translates a SpeakerEventGroupBits ERR_ESP bit to the coressponding esp_err_t
static esp_err_t err_bit_to_esp_err(uint32_t bit) {
  switch (bit) {
    case SpeakerEventGroupBits::ERR_ESP_INVALID_STATE:
      return ESP_ERR_INVALID_STATE;
    case SpeakerEventGroupBits::ERR_ESP_INVALID_ARG:
      return ESP_ERR_INVALID_ARG;
    case SpeakerEventGroupBits::ERR_ESP_INVALID_SIZE:
      return ESP_ERR_INVALID_SIZE;
    case SpeakerEventGroupBits::ERR_ESP_NO_MEM:
      return ESP_ERR_NO_MEM;
    case SpeakerEventGroupBits::ERR_ESP_NOT_SUPPORTED:
      return ESP_ERR_NOT_SUPPORTED;
    default:
      return ESP_FAIL;
  }
}

/// @brief Multiplies the input array of Q15 numbers by a Q15 constant factor
///
/// Based on `dsps_mulc_s16_ansi` from the esp-dsp library:
/// https://github.com/espressif/esp-dsp/blob/master/modules/math/mulc/fixed/dsps_mulc_s16_ansi.c
/// (accessed on 2024-09-30).
/// @param input Array of Q15 numbers
/// @param output Array of Q15 numbers
/// @param len Length of array
/// @param c Q15 constant factor
static void q15_multiplication(const int16_t *input, int16_t *output, size_t len, int16_t c) {
  for (int i = 0; i < len; i++) {
    int32_t acc = (int32_t) input[i] * (int32_t) c;
    output[i] = (int16_t) (acc >> 15);
  }
}

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
  ESP_LOGCONFIG(TAG, "Setting up I2S Audio Speaker...");

  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }
}

void I2SAudioSpeaker::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & SpeakerEventGroupBits::STATE_STARTING) {
    ESP_LOGD(TAG, "Starting Speaker");
    this->state_ = speaker::STATE_STARTING;
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::STATE_STARTING);
  }
  if (event_group_bits & SpeakerEventGroupBits::STATE_RUNNING) {
    ESP_LOGD(TAG, "Started Speaker");
    this->state_ = speaker::STATE_RUNNING;
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::STATE_RUNNING);
    this->status_clear_warning();
    this->status_clear_error();
  }
  if (event_group_bits & SpeakerEventGroupBits::STATE_STOPPING) {
    ESP_LOGD(TAG, "Stopping Speaker");
    this->state_ = speaker::STATE_STOPPING;
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::STATE_STOPPING);
  }
  if (event_group_bits & SpeakerEventGroupBits::STATE_STOPPED) {
    if (!this->task_created_) {
      ESP_LOGD(TAG, "Stopped Speaker");
      this->state_ = speaker::STATE_STOPPED;
      xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ALL_BITS);
      this->speaker_task_handle_ = nullptr;
    }
  }

  if (event_group_bits & SpeakerEventGroupBits::ERR_TASK_FAILED_TO_START) {
    this->status_set_error("Failed to start speaker task");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ERR_TASK_FAILED_TO_START);
  }

  if (event_group_bits & SpeakerEventGroupBits::ALL_ERR_ESP_BITS) {
    uint32_t error_bits = event_group_bits & SpeakerEventGroupBits::ALL_ERR_ESP_BITS;
    ESP_LOGW(TAG, "Error writing to I2S: %s", esp_err_to_name(err_bit_to_esp_err(error_bits)));
    this->status_set_warning();
  }

  if (event_group_bits & SpeakerEventGroupBits::ERR_ESP_NOT_SUPPORTED) {
    this->status_set_error("Failed to adjust I2S bus to match the incoming audio");
    ESP_LOGE(TAG,
             "Incompatible audio format: sample rate = %" PRIu32 ", channels = %" PRIu8 ", bits per sample = %" PRIu8,
             this->audio_stream_info_.get_sample_rate(), this->audio_stream_info_.get_channels(),
             this->audio_stream_info_.get_bits_per_sample());
  }

  xEventGroupClearBits(this->event_group_, ALL_ERR_ESP_BITS);
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
    ESP_LOGE(TAG, "Cannot play audio, speaker failed to setup");
    return 0;
  }
  if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
    this->start();
  }

  if ((this->state_ != speaker::STATE_RUNNING) || (this->audio_ring_buffer_.use_count() != 1)) {
    // Unable to write data to a running speaker, so delay the max amount of time so it can get ready
    vTaskDelay(ticks_to_wait);
    ticks_to_wait = 0;
  }

  size_t bytes_written = 0;
  if ((this->state_ == speaker::STATE_RUNNING) && (this->audio_ring_buffer_.use_count() == 1)) {
    // Only one owner of the ring buffer (the speaker task), so the ring buffer is allocated and no other components are
    // attempting to write to it.

    // Temporarily share ownership of the ring buffer so it won't be deallocated while writing
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->audio_ring_buffer_;
    bytes_written = temp_ring_buffer->write_without_replacement((void *) data, length, ticks_to_wait);
  }

  return bytes_written;
}

bool I2SAudioSpeaker::has_buffered_data() const {
  if (this->audio_ring_buffer_ != nullptr) {
    return this->audio_ring_buffer_->available() > 0;
  }
  return false;
}

void I2SAudioSpeaker::speaker_task(void *params) {
  I2SAudioSpeaker *this_speaker = (I2SAudioSpeaker *) params;
  this_speaker->task_created_ = true;

  uint32_t event_group_bits =
      xEventGroupWaitBits(this_speaker->event_group_,
                          SpeakerEventGroupBits::COMMAND_START | SpeakerEventGroupBits::COMMAND_STOP |
                              SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY,  // Bit message to read
                          pdTRUE,                                              // Clear the bits on exit
                          pdFALSE,                                             // Don't wait for all the bits,
                          portMAX_DELAY);                                      // Block indefinitely until a bit is set

  if (event_group_bits & (SpeakerEventGroupBits::COMMAND_STOP | SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY)) {
    // Received a stop signal before the task was requested to start
    this_speaker->delete_task_(0);
  }

  xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::STATE_STARTING);

  audio::AudioStreamInfo audio_stream_info = this_speaker->audio_stream_info_;

  const uint32_t dma_buffers_duration_ms = DMA_BUFFER_DURATION_MS * DMA_BUFFERS_COUNT;
  // Ensure ring buffer duration is at least the duration of all DMA buffers
  const uint32_t ring_buffer_duration = std::max(dma_buffers_duration_ms, this_speaker->buffer_duration_ms_);

  // The DMA buffers may have more bits per sample, so calculate buffer sizes based in the input audio stream info
  const size_t data_buffer_size = audio_stream_info.ms_to_bytes(dma_buffers_duration_ms);
  const size_t ring_buffer_size = audio_stream_info.ms_to_bytes(ring_buffer_duration);

  const size_t single_dma_buffer_input_size = data_buffer_size / DMA_BUFFERS_COUNT;

  if (this_speaker->send_esp_err_to_event_group_(this_speaker->allocate_buffers_(data_buffer_size, ring_buffer_size))) {
    // Failed to allocate buffers
    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
    this_speaker->delete_task_(data_buffer_size);
  }

  if (!this_speaker->send_esp_err_to_event_group_(this_speaker->start_i2s_driver_(audio_stream_info))) {
    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::STATE_RUNNING);

    bool stop_gracefully = false;
    uint32_t last_data_received_time = millis();
    bool tx_dma_underflow = false;

    this_speaker->accumulated_frames_written_ = 0;

    // Keep looping if paused, there is no timeout configured, or data was received more recently than the configured
    // timeout
    while (this_speaker->pause_state_ || !this_speaker->timeout_.has_value() ||
           (millis() - last_data_received_time) <= this_speaker->timeout_.value()) {
      event_group_bits = xEventGroupGetBits(this_speaker->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        xEventGroupClearBits(this_speaker->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
        break;
      }
      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY) {
        xEventGroupClearBits(this_speaker->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
        stop_gracefully = true;
      }

      if (this_speaker->audio_stream_info_ != audio_stream_info) {
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
      bool overflow;
      while (xQueueReceive(this_speaker->i2s_event_queue_, &overflow, 0)) {
        if (overflow) {
          tx_dma_underflow = true;
        }
      }
#endif

      if (this_speaker->pause_state_) {
        // Pause state is accessed atomically, so thread safe
        // Delay so the task can yields, then skip transferring audio data
        delay(TASK_DELAY_MS);
        continue;
      }

      size_t bytes_read = this_speaker->audio_ring_buffer_->read((void *) this_speaker->data_buffer_, data_buffer_size,
                                                                 pdMS_TO_TICKS(TASK_DELAY_MS));

      if (bytes_read > 0) {
        if ((audio_stream_info.get_bits_per_sample() == 16) && (this_speaker->q15_volume_factor_ < INT16_MAX)) {
          // Scale samples by the volume factor in place
          q15_multiplication((int16_t *) this_speaker->data_buffer_, (int16_t *) this_speaker->data_buffer_,
                             bytes_read / sizeof(int16_t), this_speaker->q15_volume_factor_);
        }

#ifdef USE_ESP32_VARIANT_ESP32
        // For ESP32 8/16 bit mono mode samples need to be switched.
        if (audio_stream_info.get_channels() == 1 && audio_stream_info.get_bits_per_sample() <= 16) {
          size_t len = bytes_read / sizeof(int16_t);
          int16_t *tmp_buf = (int16_t *) this_speaker->data_buffer_;
          for (int i = 0; i < len; i += 2) {
            int16_t tmp = tmp_buf[i];
            tmp_buf[i] = tmp_buf[i + 1];
            tmp_buf[i + 1] = tmp;
          }
        }
#endif
        // Write the audio data to a single DMA buffer at a time to reduce latency for the audio duration played
        // callback.
        const uint32_t batches = (bytes_read + single_dma_buffer_input_size - 1) / single_dma_buffer_input_size;

        for (uint32_t i = 0; i < batches; ++i) {
          size_t bytes_written = 0;
          size_t bytes_to_write = std::min(single_dma_buffer_input_size, bytes_read);

#ifdef USE_I2S_LEGACY
          if (audio_stream_info.get_bits_per_sample() == (uint8_t) this_speaker->bits_per_sample_) {
            i2s_write(this_speaker->parent_->get_port(), this_speaker->data_buffer_ + i * single_dma_buffer_input_size,
                      bytes_to_write, &bytes_written, pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS * 5));
          } else if (audio_stream_info.get_bits_per_sample() < (uint8_t) this_speaker->bits_per_sample_) {
            i2s_write_expand(this_speaker->parent_->get_port(),
                             this_speaker->data_buffer_ + i * single_dma_buffer_input_size, bytes_to_write,
                             audio_stream_info.get_bits_per_sample(), this_speaker->bits_per_sample_, &bytes_written,
                             pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS * 5));
          }
#else
          i2s_channel_write(this_speaker->tx_handle_, this_speaker->data_buffer_ + i * single_dma_buffer_input_size,
                            bytes_to_write, &bytes_written, pdMS_TO_TICKS(DMA_BUFFER_DURATION_MS * 5));
#endif

          int64_t now = esp_timer_get_time();

          if (bytes_written != bytes_to_write) {
            xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::ERR_ESP_INVALID_SIZE);
          }
          bytes_read -= bytes_written;

          this_speaker->audio_output_callback_(audio_stream_info.bytes_to_frames(bytes_written),
                                               now + dma_buffers_duration_ms * 1000);

          tx_dma_underflow = false;
          last_data_received_time = millis();
        }
      } else {
        // No data received
        if (stop_gracefully && tx_dma_underflow) {
          break;
        }
      }
    }

    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::STATE_STOPPING);
#ifdef USE_I2S_LEGACY
    i2s_driver_uninstall(this_speaker->parent_->get_port());
#else
    i2s_channel_disable(this_speaker->tx_handle_);
    i2s_del_channel(this_speaker->tx_handle_);
#endif

    this_speaker->parent_->unlock();
  }

  this_speaker->delete_task_(data_buffer_size);
}

void I2SAudioSpeaker::start() {
  if (!this->is_ready() || this->is_failed() || this->status_has_error())
    return;
  if ((this->state_ == speaker::STATE_STARTING) || (this->state_ == speaker::STATE_RUNNING))
    return;

  if (!this->task_created_ && (this->speaker_task_handle_ == nullptr)) {
    xTaskCreate(I2SAudioSpeaker::speaker_task, "speaker_task", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY,
                &this->speaker_task_handle_);

    if (this->speaker_task_handle_ != nullptr) {
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
    } else {
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_TASK_FAILED_TO_START);
    }
  }
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

bool I2SAudioSpeaker::send_esp_err_to_event_group_(esp_err_t err) {
  switch (err) {
    case ESP_OK:
      return false;
    case ESP_ERR_INVALID_STATE:
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_INVALID_STATE);
      return true;
    case ESP_ERR_INVALID_ARG:
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_INVALID_ARG);
      return true;
    case ESP_ERR_INVALID_SIZE:
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_INVALID_SIZE);
      return true;
    case ESP_ERR_NO_MEM:
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
      return true;
    case ESP_ERR_NOT_SUPPORTED:
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NOT_SUPPORTED);
      return true;
    default:
      xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_FAIL);
      return true;
  }
}

esp_err_t I2SAudioSpeaker::allocate_buffers_(size_t data_buffer_size, size_t ring_buffer_size) {
  if (this->data_buffer_ == nullptr) {
    // Allocate data buffer for temporarily storing audio from the ring buffer before writing to the I2S bus
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    this->data_buffer_ = allocator.allocate(data_buffer_size);
  }

  if (this->data_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  if (this->audio_ring_buffer_.use_count() == 0) {
    // Allocate ring buffer. Uses a shared_ptr to ensure it isn't improperly deallocated.
    this->audio_ring_buffer_ = RingBuffer::create(ring_buffer_size);
  }

  if (this->audio_ring_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

esp_err_t I2SAudioSpeaker::start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info) {
#ifdef USE_I2S_LEGACY
  if ((this->i2s_mode_ & I2S_MODE_SLAVE) && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {  // NOLINT
#else
  if ((this->i2s_role_ & I2S_ROLE_SLAVE) && (this->sample_rate_ != audio_stream_info.get_sample_rate())) {  // NOLINT
#endif
    // Can't reconfigure I2S bus, so the sample rate must match the configured value
    return ESP_ERR_NOT_SUPPORTED;
  }

#ifdef USE_I2S_LEGACY
  if ((i2s_bits_per_sample_t) audio_stream_info.get_bits_per_sample() > this->bits_per_sample_) {
#else
  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO &&
      (i2s_slot_bit_width_t) audio_stream_info.get_bits_per_sample() > this->slot_bit_width_) {
#endif
    // Currently can't handle the case when the incoming audio has more bits per sample than the configured value
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (!this->parent_->try_lock()) {
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
  };
  /* Allocate a new TX channel and get the handle of this channel */
  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, NULL);
  if (err != ESP_OK) {
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
    std_slot_cfg.ws_width = static_cast<uint32_t>(this->slot_bit_width_);
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
    i2s_del_channel(this->tx_handle_);
    this->parent_->unlock();
    return err;
  }
  if (this->i2s_event_queue_ == nullptr) {
    this->i2s_event_queue_ = xQueueCreate(1, sizeof(bool));
  }
  const i2s_event_callbacks_t callbacks = {
      .on_send_q_ovf = i2s_overflow_cb,
  };

  i2s_channel_register_event_callback(this->tx_handle_, &callbacks, this);

  /* Before reading data, start the TX channel first */
  i2s_channel_enable(this->tx_handle_);
  if (err != ESP_OK) {
    i2s_del_channel(this->tx_handle_);
    this->parent_->unlock();
  }
#endif

  return err;
}

void I2SAudioSpeaker::delete_task_(size_t buffer_size) {
  this->audio_ring_buffer_.reset();  // Releases ownership of the shared_ptr

  if (this->data_buffer_ != nullptr) {
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    allocator.deallocate(this->data_buffer_, buffer_size);
    this->data_buffer_ = nullptr;
  }

  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::STATE_STOPPED);

  this->task_created_ = false;
  vTaskDelete(nullptr);
}

#ifndef USE_I2S_LEGACY
bool IRAM_ATTR I2SAudioSpeaker::i2s_overflow_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
  I2SAudioSpeaker *this_speaker = (I2SAudioSpeaker *) user_ctx;
  bool overflow = true;
  xQueueOverwrite(this_speaker->i2s_event_queue_, &overflow);
  return false;
}
#endif

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
