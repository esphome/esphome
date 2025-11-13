#include "spdif_speaker.h"

#ifdef USE_ESP32

#include <driver/i2s.h>
#include <esp_timer.h>

#include "esphome/components/audio/audio.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace spdif_audio {

static const size_t DMA_BUFFERS_COUNT = 4;

static const size_t TASK_STACK_SIZE = 4096;
static const ssize_t TASK_PRIORITY = 23;

static const size_t I2S_EVENT_QUEUE_COUNT = DMA_BUFFERS_COUNT + 1;

static const char *const TAG = "spdif_audio.speaker";

#if SPDIF_FILL_SILENCE
// A full BMC block's worth of 16-big stereo samples
int16_t silence[SPDIF_BLOCK_SAMPLES * 2];
#endif

void SPDIFSpeaker::i2s_event_task(void *params) {
  SPDIFSpeaker *this_speaker = (SPDIFSpeaker *) params;
  i2s_event_t i2s_event;
#if SPDIF_DEBUG
  int64_t last_error_log_time = 0;
  int64_t last_overflow_log_time = 0;
  // 1 second in microseconds
  const int64_t min_log_interval_us = 1000000;
#endif

  while (true) {
    if (xQueueReceive(this_speaker->i2s_event_queue_, &i2s_event, portMAX_DELAY)) {
#if SPDIF_DEBUG
      int64_t current_time = esp_timer_get_time();
#endif

      if (i2s_event.type == I2S_EVENT_DMA_ERROR) {
#if SPDIF_DEBUG
        if (current_time - last_error_log_time >= min_log_interval_us) {
          ESP_LOGE(TAG, "I2S_EVENT_DMA_ERROR");
          last_error_log_time = current_time;
        }
#endif
      } else if (i2s_event.type == I2S_EVENT_TX_Q_OVF) {
        // I2S DMA sending queue overflowed, the oldest data has been overwritten
        // by the new data in the DMA buffer
#if SPDIF_DEBUG
        if (current_time - last_overflow_log_time >= min_log_interval_us) {
          ESP_LOGE(TAG, "I2S_EVENT_TX_Q_OVF");
          last_overflow_log_time = current_time;
        }
#endif
#if SPDIF_FILL_SILENCE
        // Queue a SPDIF block full of silence when we don't have anything else to play
        this_speaker->spdif_->reset();
        this_speaker->spdif_->write(reinterpret_cast<uint8_t *>(silence), sizeof(silence), 0);
#endif
        this_speaker->tx_dma_underflow_ = true;
      }
    }
  }
}

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
  for (size_t i = 0; i < len; i++) {
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

void SPDIFSpeaker::setup() {
  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

  this->spdif_->setup();
  this->spdif_->set_block_complete_callback([this](uint32_t *data, size_t size, TickType_t ticks_to_wait) -> esp_err_t {
    size_t i2s_write_len;

    esp_err_t err = i2s_write(this->parent_->get_port(), data, size, &i2s_write_len, ticks_to_wait);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(err));
    }

#if SPDIF_DEBUG
    static uint64_t total_bytes = 0;
    static uint64_t last_log_time = 0;
    static uint64_t last_log_bytes = 0;

    total_bytes += i2s_write_len;
    int64_t current_time = esp_timer_get_time();

    if (last_log_time == 0) {
      last_log_time = current_time;
      last_log_bytes = total_bytes;
    }

    // Check if it's time to log sample statistics (every minute)
    if (current_time - last_log_time >= 5000000) {
      uint64_t elapsed_time = current_time - last_log_time;
      uint64_t bytes_since_last_log = total_bytes - last_log_bytes;
      uint64_t samples = bytes_since_last_log / (EMULATED_BMC_BITS_PER_SAMPLE / 8);
      float seconds = elapsed_time / 1000000.0f;
      float hz = samples / seconds;

      ESP_LOGD(TAG, "%llu samples in %.2fs (%.2fHz)", samples, seconds, hz);

      // Reset for next log
      last_log_time = current_time;
      last_log_bytes = total_bytes;
    }
#endif
    return err;
  });

#if SPDIF_FILL_SILENCE
  memset(silence, 0, sizeof(silence));
#endif
}

void SPDIFSpeaker::loop() {
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
}

void SPDIFSpeaker::set_volume(float volume) {
  this->volume_ = volume;
  // Software volume control by using a Q15 fixed point scaling factor
  ssize_t decibel_index = remap<ssize_t, float>(volume, 0.0f, 1.0f, 0, Q15_VOLUME_SCALING_FACTORS.size() - 1);
  this->q15_volume_factor_ = Q15_VOLUME_SCALING_FACTORS[decibel_index];
}

void SPDIFSpeaker::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  if (mute_state) {
    // Software volume control and scale by 0
    this->q15_volume_factor_ = 0;
  } else {
    // Revert to previous volume when unmuting
    this->set_volume(this->volume_);
  }
}

size_t SPDIFSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot play audio, speaker failed to setup");
    return 0;
  }
  if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
    this->start();
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

bool SPDIFSpeaker::has_buffered_data() const {
  if (this->audio_ring_buffer_ != nullptr) {
    return this->audio_ring_buffer_->available() > 0;
  }
  return false;
}

void SPDIFSpeaker::speaker_task(void *params) {
  SPDIFSpeaker *this_speaker = (SPDIFSpeaker *) params;
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

  const uint32_t bytes_per_ms = audio_stream_info.get_channels() * audio_stream_info.samples_to_bytes(1) *
                                audio_stream_info.get_sample_rate() / 1000;

  const size_t dma_buffers_size = DMA_BUFFERS_COUNT * SPDIF_BLOCK_SAMPLES * audio_stream_info.get_channels() *
                                  audio_stream_info.samples_to_bytes(1);

  int task_delay_ms = bytes_per_ms * DMA_BUFFERS_COUNT / 2;

  // Ensure ring buffer is at least as large as the total size of the DMA buffers
  const size_t ring_buffer_size =
      std::max((uint32_t) dma_buffers_size, this_speaker->buffer_duration_ms_ * bytes_per_ms);

  if (this_speaker->send_esp_err_to_event_group_(this_speaker->allocate_buffers_(dma_buffers_size, ring_buffer_size))) {
    // Failed to allocate buffers
    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
    this_speaker->delete_task_(dma_buffers_size);
  }

  if (!this_speaker->send_esp_err_to_event_group_(this_speaker->start_i2s_driver_(audio_stream_info))) {
    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::STATE_RUNNING);

    bool stop_gracefully = false;
    uint32_t last_data_received_time = millis();
    this_speaker->tx_dma_underflow_ = false;

    while (!this_speaker->timeout_.has_value() ||
           (millis() - last_data_received_time) <= this_speaker->timeout_.value()) {
      event_group_bits = xEventGroupGetBits(this_speaker->event_group_);

      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP) {
        break;
      }
      if (event_group_bits & SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY) {
        stop_gracefully = true;
      }

      if (this_speaker->audio_stream_info_ != audio_stream_info) {
        // Audio stream info has changed, stop the speaker task so it will restart with the proper settings.

        break;
      }

      size_t bytes_to_read = dma_buffers_size;

      size_t bytes_read = this_speaker->audio_ring_buffer_->read((void *) this_speaker->data_buffer_, bytes_to_read,
                                                                 pdMS_TO_TICKS(task_delay_ms));
      if (bytes_read > 0) {
        if ((audio_stream_info.get_bits_per_sample() == 16) && (this_speaker->q15_volume_factor_ < INT16_MAX)) {
          // Scale samples by the volume factor in place
          q15_multiplication((int16_t *) this_speaker->data_buffer_, (int16_t *) this_speaker->data_buffer_,
                             bytes_read / sizeof(int16_t), this_speaker->q15_volume_factor_);
        }

        this_speaker->spdif_->write(this_speaker->data_buffer_, bytes_read, portMAX_DELAY);

        this_speaker->tx_dma_underflow_ = false;
        last_data_received_time = millis();
      } else {
        // No data received
        if (stop_gracefully && this_speaker->tx_dma_underflow_) {
          break;
        }
      }
    }

    xEventGroupSetBits(this_speaker->event_group_, SpeakerEventGroupBits::STATE_STOPPING);

    i2s_driver_uninstall(this_speaker->parent_->get_port());

    this_speaker->parent_->unlock();
  }

  this_speaker->delete_task_(dma_buffers_size);
}

void SPDIFSpeaker::start() {
  if (!this->is_ready() || this->is_failed() || this->status_has_error())
    return;
  if ((this->state_ == speaker::STATE_STARTING) || (this->state_ == speaker::STATE_RUNNING))
    return;

  if (this->speaker_task_handle_ == nullptr) {
    xTaskCreate(SPDIFSpeaker::speaker_task, "speaker_task", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY,
                &this->speaker_task_handle_);
  }

  if (this->speaker_task_handle_ != nullptr) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
    this->task_created_ = true;
  } else {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::ERR_TASK_FAILED_TO_START);
  }
}

void SPDIFSpeaker::stop() { this->stop_(false); }

void SPDIFSpeaker::finish() { this->stop_(true); }

void SPDIFSpeaker::stop_(bool wait_on_empty) {
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

bool SPDIFSpeaker::send_esp_err_to_event_group_(esp_err_t err) {
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

esp_err_t SPDIFSpeaker::allocate_buffers_(size_t data_buffer_size, size_t ring_buffer_size) {
  if (this->data_buffer_ == nullptr) {
    // Allocate data buffer for temporarily storing audio from the ring buffer before writing to the I2S bus
    ExternalRAMAllocator<uint8_t> allocator(ExternalRAMAllocator<uint8_t>::ALLOW_FAILURE);
    this->data_buffer_ = allocator.allocate(data_buffer_size);
  }

  if (this->data_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate data_buffer_");
    return ESP_ERR_NO_MEM;
  }

  if (this->audio_ring_buffer_.use_count() == 0) {
    // Allocate ring buffer. Uses a shared_ptr to ensure it isn't improperly deallocated.
    this->audio_ring_buffer_ = RingBuffer::create(ring_buffer_size);
  }

  if (this->audio_ring_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate audio_ring_buffer_");
    return ESP_ERR_NO_MEM;
  }

  return ESP_OK;
}

esp_err_t SPDIFSpeaker::start_i2s_driver_(audio::AudioStreamInfo &audio_stream_info) {
  if (this->sample_rate_ != audio_stream_info.get_sample_rate()) {  // NOLINT
    //  Can't reconfigure I2S bus, so the sample rate must match the configured value
    ESP_LOGE(TAG, "SPDIF only supports a single sample rate");
    return ESP_ERR_NOT_SUPPORTED;
  }

  // Currently only 16-bit samples are supported
  if (audio_stream_info.get_bits_per_sample() != 16) {
    ESP_LOGE(TAG, "SPDIF only supports 16 bits per sample");
    return ESP_ERR_NOT_SUPPORTED;
  }

  // Currently only stereo is supported
  if (audio_stream_info.get_channels() != 2) {
    ESP_LOGE(TAG, "SPDIF only supports stereo");
    return ESP_ERR_NOT_SUPPORTED;
  }

  if (!this->parent_->try_lock()) {
    return ESP_ERR_INVALID_STATE;
  }

  constexpr uint32_t i2s_bits_per_sample = 32;
  uint32_t sample_rate = this->sample_rate_ * 2;
  i2s_config_t config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = sample_rate,
    .bits_per_sample = static_cast<i2s_bits_per_sample_t>(i2s_bits_per_sample),
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = DMA_BUFFERS_COUNT,
    .dma_buf_len = SPDIF_BLOCK_SIZE_U32,
    .use_apll = true,
#if SPDIF_FILL_SILENCE
    .tx_desc_auto_clear = false,
#else
    .tx_desc_auto_clear = true,
#endif
    .fixed_mclk = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  esp_err_t err =
      i2s_driver_install(this->parent_->get_port(), &config, I2S_EVENT_QUEUE_COUNT, &this->i2s_event_queue_);

  // Event taks runs in its own thread so it can fill the SPDIF block buffer with silence when the DMA underflows
  xTaskCreate(i2s_event_task, "i2s_event_task", 3072, (void *) this, 10, NULL);

  if (err != ESP_OK) {
    // Failed to install the driver, so unlock the I2S port
    this->parent_->unlock();
    return err;
  }

  i2s_pin_config_t pin_config = {
      .mck_io_num = -1,
      .bck_io_num = -1,
      .ws_io_num = -1,
      .data_out_num = this->data_pin_,
      .data_in_num = -1,
  };

  err = i2s_set_pin(this->parent_->get_port(), &pin_config);

  if (err != ESP_OK) {
    // Failed to set the data out pin, so uninstall the driver and unlock the I2S port
    i2s_driver_uninstall(this->parent_->get_port());
    this->parent_->unlock();
  }

  return err;
}

void SPDIFSpeaker::delete_task_(size_t buffer_size) {
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

}  // namespace spdif_audio
}  // namespace esphome

#endif  // USE_ESP32
