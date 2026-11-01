#include "i2s_audio_microphone.h"

#ifdef USE_ESP32

#include <driver/i2s_std.h>
#include <driver/i2s_pdm.h>
#include <esp_timer.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esphome/components/audio/audio.h"

#include "capture_time.h"

#include <algorithm>

namespace esphome::i2s_audio {

static const UBaseType_t MAX_LISTENERS = 16;

static const uint32_t READ_DURATION_MS = 16;

static constexpr uint32_t DMA_BUFFERS_COUNT = 4;
static constexpr uint32_t DMA_FRAME_NUM = 256;

static const size_t TASK_STACK_SIZE = 4096;
static const ssize_t TASK_PRIORITY = 23;

ESPHOME_LOG_TAG(TAG, "i2s_audio.microphone");

enum MicrophoneEventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),  // stops the microphone task, set and cleared by ``loop``

  TASK_STARTING = (1 << 10),  // set by mic task, cleared by ``loop``
  TASK_RUNNING = (1 << 11),   // set by mic task, cleared by ``loop``
  TASK_STOPPED = (1 << 13),   // set by mic task, cleared by ``loop``

  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

void I2SAudioMicrophone::setup() {
  this->active_listeners_semaphore_ = xSemaphoreCreateCounting(MAX_LISTENERS, MAX_LISTENERS);
  if (this->active_listeners_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Creating semaphore failed");
    this->mark_failed();
    return;
  }

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Creating event group failed");
    this->mark_failed();
    return;
  }

#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  if (this->parent_->is_full_duplex() && this->parent_->is_failed()) {
    this->mark_failed(LOG_STR("Full duplex bus setup failed"));
    return;
  }
#endif

  this->configure_stream_settings_();
}

void I2SAudioMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Microphone:\n"
                "  Pin: %d\n"
                "  PDM: %s\n"
                "  DC offset correction: %s",
                static_cast<int8_t>(this->din_pin_), YESNO(this->pdm_), YESNO(this->correct_dc_offset_));
}

void I2SAudioMicrophone::configure_stream_settings_() {
  uint8_t channel_count = 1;
  uint8_t bits_per_sample = 16;
  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO) {
    bits_per_sample = this->slot_bit_width_;
  }

  if (this->slot_mode_ == I2S_SLOT_MODE_STEREO) {
    channel_count = 2;
  }

#ifdef USE_ESP32_VARIANT_ESP32
  // ESP32 reads audio aligned to a multiple of 2 bytes. For example, if configured for 24 bits per sample, then it will
  // produce 32 bits per sample, where the actual data is in the most significant bits. Other ESP32 variants produce 24
  // bits per sample in this situation.
  if (bits_per_sample < 16) {
    bits_per_sample = 16;
  } else if ((bits_per_sample > 16) && (bits_per_sample <= 32)) {
    bits_per_sample = 32;
  }
#endif

  if (this->pdm_) {
    bits_per_sample = 16;  // PDM mics are always 16 bits per sample
  }

  this->audio_stream_info_ = audio::AudioStreamInfo(bits_per_sample, channel_count, this->sample_rate_);
}

void I2SAudioMicrophone::start() {
  if (!this->is_ready())
    return;

  xSemaphoreTake(this->active_listeners_semaphore_, 0);
}

void I2SAudioMicrophone::build_std_config_(i2s_std_config_t &std_cfg) const {
  i2s_std_slot_config_t std_slot_cfg =
      I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) this->slot_bit_width_, this->slot_mode_);
  std_slot_cfg.slot_bit_width = this->slot_bit_width_;
  std_slot_cfg.slot_mask = this->std_slot_mask_;

  i2s_std_gpio_config_t pin_config = this->parent_->get_pin_config();
  pin_config.din = this->din_pin_;

  std_cfg = {
      .clk_cfg =
          {
              .sample_rate_hz = this->sample_rate_,
              .clk_src = this->get_clock_source_(),
              .mclk_multiple = this->mclk_multiple_,
          },
      .slot_cfg = std_slot_cfg,
      .gpio_cfg = pin_config,
  };
}

#ifdef USE_I2S_AUDIO_FULL_DUPLEX
void I2SAudioMicrophone::build_full_duplex_config(i2s_std_config_t &std_cfg) { this->build_std_config_(std_cfg); }

void I2SAudioMicrophone::full_duplex_rx_enabling(i2s_chan_handle_t rx_handle, uint32_t dma_desc_num) {
  // A failure only loses capture times; the speaker may still need RX enabled, so it is not passed on
  this->start_capture_count_(rx_handle, dma_desc_num);
}
#endif

bool I2SAudioMicrophone::start_capture_count_(i2s_chan_handle_t rx_handle, uint32_t dma_desc_num) {
  // Neither the interrupt nor the mic task runs while the channel is disabled
  this->dma_bytes_received_ = 0;
  this->dma_buffers_dropped_.store(0, std::memory_order_relaxed);
  this->dma_receive_time_us_ = 0;
  this->dma_bytes_read_ = 0;
  this->dma_desc_num_ = dma_desc_num;
  const i2s_event_callbacks_t callbacks = {
      .on_recv = i2s_on_recv_cb,
      .on_recv_q_ovf = i2s_on_recv_q_ovf_cb,
  };
  esp_err_t err = i2s_channel_register_event_callback(rx_handle, &callbacks, this);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Registering callbacks failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool I2SAudioMicrophone::read_dma_buffer_size_() {
  i2s_chan_info_t chan_info{};
  esp_err_t err = i2s_channel_get_info(this->rx_handle_, &chan_info);
  if ((err != ESP_OK) || (chan_info.total_dma_buf_size == 0) || (this->dma_desc_num_ == 0)) {
    ESP_LOGE(TAG, "Getting the DMA buffer size failed");
    return false;
  }
  this->dma_buffer_bytes_ = chan_info.total_dma_buf_size / this->dma_desc_num_;
  return true;
}

bool I2SAudioMicrophone::start_driver_() {
#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  if (this->parent_->is_full_duplex()) {
    this->rx_handle_ = this->parent_->acquire_rx_channel();
    if (this->rx_handle_ == nullptr) {
      ESP_LOGE(TAG, "Full duplex channel unavailable");
      return false;
    }
    this->configure_stream_settings_();
    return this->read_dma_buffer_size_();
  }
#endif

  if (!this->parent_->try_lock()) {
    return false;  // Waiting for another i2s to return lock
  }
  this->locked_driver_ = true;
  esp_err_t err;

  i2s_chan_config_t chan_cfg = {
      .id = this->parent_->get_port(),
      .role = this->i2s_role_,
      .dma_desc_num = DMA_BUFFERS_COUNT,
      .dma_frame_num = DMA_FRAME_NUM,
      .auto_clear = false,
  };
  /* Allocate a new RX channel and get the handle of this channel */
  err = i2s_new_channel(&chan_cfg, NULL, &this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error creating channel: %s", esp_err_to_name(err));
    return false;
  }

#if SOC_I2S_SUPPORTS_PDM_RX
  if (this->pdm_) {
    i2s_std_gpio_config_t pin_config = this->parent_->get_pin_config();
    i2s_pdm_rx_clk_config_t clk_cfg = {
        .sample_rate_hz = this->sample_rate_,
        .clk_src = this->get_clock_source_(),
        .mclk_multiple = this->mclk_multiple_,
        .dn_sample_mode = this->pdm_dsr_,
    };

    i2s_pdm_rx_slot_config_t slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, this->slot_mode_);
    switch (this->std_slot_mask_) {
      case I2S_STD_SLOT_LEFT:
        slot_cfg.slot_mask = I2S_PDM_SLOT_LEFT;
        break;
      case I2S_STD_SLOT_RIGHT:
        slot_cfg.slot_mask = I2S_PDM_SLOT_RIGHT;
        break;
      case I2S_STD_SLOT_BOTH:
        slot_cfg.slot_mask = I2S_PDM_SLOT_BOTH;
        break;
    }

    /* Init the channel into PDM RX mode */
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = slot_cfg,
        .gpio_cfg =
            {
                .clk = pin_config.ws,
                .din = this->din_pin_,
                .invert_flags =
                    {
                        .clk_inv = pin_config.invert_flags.ws_inv,
                    },
            },
    };
    err = i2s_channel_init_pdm_rx_mode(this->rx_handle_, &pdm_rx_cfg);
  } else
#endif
  {
    i2s_std_config_t std_cfg;
    this->build_std_config_(std_cfg);
    /* Initialize the channel */
    err = i2s_channel_init_std_mode(this->rx_handle_, &std_cfg);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing channel: %s", esp_err_to_name(err));
    return false;
  }

  if (!this->start_capture_count_(this->rx_handle_, DMA_BUFFERS_COUNT)) {
    return false;
  }

  /* Before reading data, start the RX channel first */
  err = i2s_channel_enable(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Enabling failed: %s", esp_err_to_name(err));
    return false;
  }

  this->configure_stream_settings_();  // redetermine the settings in case some settings were changed after compilation

  return this->read_dma_buffer_size_();
}

void I2SAudioMicrophone::stop() {
  if (!this->is_ready())
    return;

  // Not gated on state_: a start() that loop() has not acted on yet must be given back too
  xSemaphoreGive(this->active_listeners_semaphore_);
}

void I2SAudioMicrophone::stop_driver_() {
  // There is no harm continuing to unload the driver if an error is ever returned by the various functions. This
  // ensures that we stop/unload the driver when it only partially starts.

#ifdef USE_I2S_AUDIO_FULL_DUPLEX
  if (this->parent_->is_full_duplex()) {
    // The parent owns the channel pair, so only hand the RX channel back
    if (this->rx_handle_ != nullptr) {
      this->parent_->release_rx_channel();
      this->rx_handle_ = nullptr;
    }
    return;
  }
#endif

  esp_err_t err;
  if (this->rx_handle_ != nullptr) {
    /* Have to stop the channel before deleting it */
    err = i2s_channel_disable(this->rx_handle_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error stopping: %s", esp_err_to_name(err));
    }
    /* If the handle is not needed any more, delete it to release the channel resources */
    err = i2s_del_channel(this->rx_handle_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error deleting channel: %s", esp_err_to_name(err));
    }
    this->rx_handle_ = nullptr;
  }
  if (this->locked_driver_) {
    this->parent_->unlock();
    this->locked_driver_ = false;
  }
}

void I2SAudioMicrophone::mic_task(void *params) {
  I2SAudioMicrophone *this_microphone = (I2SAudioMicrophone *) params;
  xEventGroupSetBits(this_microphone->event_group_, MicrophoneEventGroupBits::TASK_STARTING);

  {  // Ensures the samples vector is freed when the task stops

    // Copied so the format cannot change while the task runs
    const audio::AudioStreamInfo stream_info = this_microphone->audio_stream_info_;

    // Read whole DMA buffers. When its queue is nearly full, the driver discards the unread rest of a buffer it is
    // partway through without reporting it, which would throw off the capture time count.
    const size_t dma_buffer_bytes = this_microphone->dma_buffer_bytes_;
    const size_t bytes_to_read =
        std::max<size_t>(1, (stream_info.ms_to_bytes(READ_DURATION_MS) + dma_buffer_bytes / 2) / dma_buffer_bytes) *
        dma_buffer_bytes;
    const uint32_t read_duration_ms =
        (stream_info.bytes_to_frames(bytes_to_read) * 1000 + stream_info.get_sample_rate() - 1) /
        stream_info.get_sample_rate();
    std::vector<uint8_t> samples;
    samples.reserve(bytes_to_read);

    xEventGroupSetBits(this_microphone->event_group_, MicrophoneEventGroupBits::TASK_RUNNING);

    while (!(xEventGroupGetBits(this_microphone->event_group_) & MicrophoneEventGroupBits::COMMAND_STOP)) {
      if (this_microphone->has_data_callbacks_()) {
        const uint32_t drops_before_read = this_microphone->dma_buffers_dropped_.load(std::memory_order_relaxed);

        samples.resize(bytes_to_read);
        size_t bytes_read = this_microphone->read_(samples.data(), bytes_to_read, 2 * read_duration_ms);
        if (bytes_read == 0) {
          continue;
        }

        portENTER_CRITICAL(&this_microphone->dma_lock_);
        const uint32_t bytes_received = this_microphone->dma_bytes_received_;
        const uint32_t drops = this_microphone->dma_buffers_dropped_.load(std::memory_order_relaxed);
        const int64_t receive_time_us = this_microphone->dma_receive_time_us_;
        portEXIT_CRITICAL(&this_microphone->dma_lock_);

        const int64_t capture_time_us = dma_capture_time_us(
            bytes_received, drops, drops_before_read, receive_time_us, this_microphone->dma_bytes_read_, bytes_read,
            this_microphone->dma_desc_num_, dma_buffer_bytes, stream_info);
        this_microphone->dma_bytes_read_ += bytes_read;

        samples.resize(bytes_read);
        if (this_microphone->correct_dc_offset_) {
          this_microphone->fix_dc_offset_(samples);
        }
        this_microphone->publish_audio_(samples, stream_info, capture_time_us);
      } else {
        vTaskDelay(pdMS_TO_TICKS(READ_DURATION_MS));
      }
    }
  }

  xEventGroupSetBits(this_microphone->event_group_, MicrophoneEventGroupBits::TASK_STOPPED);
  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

bool IRAM_ATTR I2SAudioMicrophone::i2s_on_recv_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
  const int64_t now = esp_timer_get_time();
  auto *this_microphone = static_cast<I2SAudioMicrophone *>(user_ctx);
  portENTER_CRITICAL_ISR(&this_microphone->dma_lock_);
  this_microphone->dma_bytes_received_ += event->size;
  this_microphone->dma_receive_time_us_ = now;
  portEXIT_CRITICAL_ISR(&this_microphone->dma_lock_);
  return false;
}

bool IRAM_ATTR I2SAudioMicrophone::i2s_on_recv_q_ovf_cb(i2s_chan_handle_t handle, i2s_event_data_t *event,
                                                        void *user_ctx) {
  auto *this_microphone = static_cast<I2SAudioMicrophone *>(user_ctx);
  // A load and a store rather than fetch_add, which is not a hardware instruction on every variant
  this_microphone->dma_buffers_dropped_.store(this_microphone->dma_buffers_dropped_.load(std::memory_order_relaxed) + 1,
                                              std::memory_order_relaxed);
  return false;
}

void I2SAudioMicrophone::fix_dc_offset_(std::vector<uint8_t> &data) {
  /**
   * From https://www.musicdsp.org/en/latest/Filters/135-dc-filter.html:
   *
   *     y(n) = x(n) - x(n-1) + R * y(n-1)
   *     R = 1 - (pi * 2 * frequency / samplerate)
   *
   * From https://en.wikipedia.org/wiki/Hearing_range:
   *     The human range is commonly given as 20Hz up.
   *
   * From https://en.wikipedia.org/wiki/High-resolution_audio:
   *     A reasonable upper bound for sample rate seems to be 96kHz.
   *
   * Calculate R value for 20Hz on a 96kHz sample rate:
   *     R = 1 - (pi * 2 * 20 / 96000)
   *     R = 0.9986910031
   *
   * Transform floating point to bit-shifting approximation:
   *     output = input - prev_input + R * prev_output
   *     output = input - prev_input + (prev_output - (prev_output >> S))
   *
   * Approximate bit-shift value S from R:
   *     R = 1 - (1 >> S)
   *     R = 1 - (1 / 2^S)
   *     R = 1 - 2^-S
   *     0.9986910031 = 1 - 2^-S
   *     S = 9.57732 ~= 10
   *
   * Actual R from S:
   *     R = 1 - 2^-10 = 0.9990234375
   *
   * Confirm this has effect outside human hearing on 96000kHz sample:
   *     0.9990234375 = 1 - (pi * 2 * f / 96000)
   *     f = 14.9208Hz
   *
   * Confirm this has effect outside human hearing on PDM 16kHz sample:
   *     0.9990234375 = 1 - (pi * 2 * f / 16000)
   *     f = 2.4868Hz
   *
   */
  const uint8_t dc_filter_shift = 10;
  const size_t bytes_per_sample = this->audio_stream_info_.samples_to_bytes(1);
  const uint32_t total_samples = this->audio_stream_info_.bytes_to_samples(data.size());
  for (uint32_t sample_index = 0; sample_index < total_samples; ++sample_index) {
    const uint32_t byte_index = sample_index * bytes_per_sample;
    int32_t input = audio::unpack_audio_sample_to_q31(&data[byte_index], bytes_per_sample);
    int32_t output = input - this->dc_offset_prev_input_ +
                     (this->dc_offset_prev_output_ - (this->dc_offset_prev_output_ >> dc_filter_shift));
    this->dc_offset_prev_input_ = input;
    this->dc_offset_prev_output_ = output;
    audio::pack_q31_as_audio_sample(output, &data[byte_index], bytes_per_sample);
  }
}

size_t I2SAudioMicrophone::read_(uint8_t *buf, size_t len, uint32_t timeout_ms) {
  size_t bytes_read = 0;
  esp_err_t err = i2s_channel_read(this->rx_handle_, buf, len, &bytes_read, timeout_ms);
  if ((err != ESP_OK) && ((err != ESP_ERR_TIMEOUT) || (timeout_ms != 0))) {
    // Ignore ESP_ERR_TIMEOUT if timeout_ms = 0, as it will read the data on the next call
    if (!this->status_has_warning()) {
      // Avoid spamming the logs with the error message if its repeated
      ESP_LOGW(TAG, "Read error: %s", esp_err_to_name(err));
    }
    this->status_set_warning();
  } else if ((bytes_read == 0) && (timeout_ms > 0)) {
    this->status_set_warning();
  } else {
    this->status_clear_warning();
  }
  // Even after an error, any bytes read have left the DMA buffers and are valid audio
#ifdef USE_ESP32_VARIANT_ESP32
  // For ESP32 16-bit standard mono mode, adjacent samples need to be swapped.
  if (this->slot_mode_ == I2S_SLOT_MODE_MONO && this->slot_bit_width_ == I2S_SLOT_BIT_WIDTH_16BIT && !this->pdm_) {
    int16_t *samples = reinterpret_cast<int16_t *>(buf);
    size_t sample_count = bytes_read / sizeof(int16_t);
    for (size_t i = 0; i + 1 < sample_count; i += 2) {
      int16_t tmp = samples[i];
      samples[i] = samples[i + 1];
      samples[i + 1] = tmp;
    }
  }
#endif
  return bytes_read;
}

void I2SAudioMicrophone::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & MicrophoneEventGroupBits::TASK_STARTING) {
    ESP_LOGV(TAG, "Task started, attempting to allocate buffer");
    xEventGroupClearBits(this->event_group_, MicrophoneEventGroupBits::TASK_STARTING);
  }

  if (event_group_bits & MicrophoneEventGroupBits::TASK_RUNNING) {
    ESP_LOGV(TAG, "Task is running and reading data");

    xEventGroupClearBits(this->event_group_, MicrophoneEventGroupBits::TASK_RUNNING);
    this->state_ = microphone::STATE_RUNNING;
  }

  if ((event_group_bits & MicrophoneEventGroupBits::TASK_STOPPED)) {
    ESP_LOGV(TAG, "Task finished, freeing resources and uninstalling driver");

    vTaskDelete(this->task_handle_);
    this->task_handle_ = nullptr;
    this->stop_driver_();
    xEventGroupClearBits(this->event_group_, ALL_BITS);
    this->status_clear_error();

    this->state_ = microphone::STATE_STOPPED;
  }

  // Start the microphone if any semaphores are taken
  if ((uxSemaphoreGetCount(this->active_listeners_semaphore_) < MAX_LISTENERS) &&
      (this->state_ == microphone::STATE_STOPPED)) {
    this->state_ = microphone::STATE_STARTING;
  }

  // Stop the microphone if all semaphores are returned
  if ((uxSemaphoreGetCount(this->active_listeners_semaphore_) == MAX_LISTENERS) &&
      (this->state_ == microphone::STATE_RUNNING)) {
    this->state_ = microphone::STATE_STOPPING;
  }

  switch (this->state_) {
    case microphone::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }

      if (!this->start_driver_()) {
        ESP_LOGE(TAG, "Driver failed to start; retrying in 1 second");
        this->status_momentary_error(1000);
        this->stop_driver_();  // Stop/frees whatever possibly started
        break;
      }

      if (this->task_handle_ == nullptr) {
        xTaskCreate(I2SAudioMicrophone::mic_task, "mic_task", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY,
                    &this->task_handle_);

        if (this->task_handle_ == nullptr) {
          ESP_LOGE(TAG, "Task failed to start, retrying in 1 second");
          this->status_momentary_error(1000);
          this->stop_driver_();  // Stops the driver to return the lock; will be reloaded in next attempt
        }
      }

      break;
    case microphone::STATE_RUNNING:
      break;
    case microphone::STATE_STOPPING:
      xEventGroupSetBits(this->event_group_, MicrophoneEventGroupBits::COMMAND_STOP);
      break;
    case microphone::STATE_STOPPED:
      break;
  }
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
