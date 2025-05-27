#include "i2s_audio_microphone.h"

#ifdef USE_ESP32

#ifdef USE_I2S_LEGACY
#include <driver/i2s.h>
#else
#include <driver/i2s_std.h>
#include <driver/i2s_pdm.h>
#endif

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esphome/components/audio/audio.h"

namespace esphome {
namespace i2s_audio {

static const UBaseType_t MAX_LISTENERS = 16;

static const uint32_t READ_DURATION_MS = 16;

static const size_t TASK_STACK_SIZE = 4096;
static const ssize_t TASK_PRIORITY = 23;

// Use an exponential moving average to correct a DC offset with weight factor 1/1000
static const int32_t DC_OFFSET_MOVING_AVERAGE_COEFFICIENT_DENOMINATOR = 1000;

static const char *const TAG = "i2s_audio.microphone";

enum MicrophoneEventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),  // stops the microphone task, set and cleared by ``loop``

  TASK_STARTING = (1 << 10),  // set by mic task, cleared by ``loop``
  TASK_RUNNING = (1 << 11),   // set by mic task, cleared by ``loop``
  TASK_STOPPED = (1 << 13),   // set by mic task, cleared by ``loop``

  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

void I2SAudioMicrophone::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2S Audio Microphone...");
#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_ADC
  if (this->adc_) {
    if (this->parent_->get_port() != I2S_NUM_0) {
      ESP_LOGE(TAG, "Internal ADC only works on I2S0!");
      this->mark_failed();
      return;
    }
  } else
#endif
#endif
  {
    if (this->pdm_) {
      if (this->parent_->get_port() != I2S_NUM_0) {
        ESP_LOGE(TAG, "PDM only works on I2S0!");
        this->mark_failed();
        return;
      }
    }
  }

  this->active_listeners_semaphore_ = xSemaphoreCreateCounting(MAX_LISTENERS, MAX_LISTENERS);
  if (this->active_listeners_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create semaphore");
    this->mark_failed();
    return;
  }

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

  this->configure_stream_settings_();
}

void I2SAudioMicrophone::configure_stream_settings_() {
  uint8_t channel_count = 1;
#ifdef USE_I2S_LEGACY
  uint8_t bits_per_sample = this->bits_per_sample_;

  if (this->channel_ == I2S_CHANNEL_FMT_RIGHT_LEFT) {
    channel_count = 2;
  }
#else
  uint8_t bits_per_sample = 16;
  if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_AUTO) {
    bits_per_sample = this->slot_bit_width_;
  }

  if (this->slot_mode_ == I2S_SLOT_MODE_STEREO) {
    channel_count = 2;
  }
#endif

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
  if (this->is_failed())
    return;

  xSemaphoreTake(this->active_listeners_semaphore_, 0);
}

bool I2SAudioMicrophone::start_driver_() {
  if (!this->parent_->try_lock()) {
    return false;  // Waiting for another i2s to return lock
  }
  esp_err_t err;

#ifdef USE_I2S_LEGACY
  i2s_driver_config_t config = {
      .mode = (i2s_mode_t) (this->i2s_mode_ | I2S_MODE_RX),
      .sample_rate = this->sample_rate_,
      .bits_per_sample = this->bits_per_sample_,
      .channel_format = this->channel_,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 240,  // Must be divisible by 3 to support 24 bits per sample on old driver and newer variants
      .use_apll = this->use_apll_,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
      .mclk_multiple = this->mclk_multiple_,
      .bits_per_chan = this->bits_per_channel_,
  };

#if SOC_I2S_SUPPORTS_ADC
  if (this->adc_) {
    config.mode = (i2s_mode_t) (config.mode | I2S_MODE_ADC_BUILT_IN);
    err = i2s_driver_install(this->parent_->get_port(), &config, 0, nullptr);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error installing I2S driver: %s", esp_err_to_name(err));
      return false;
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, this->adc_channel_);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error setting ADC mode: %s", esp_err_to_name(err));
      return false;
    }

    err = i2s_adc_enable(this->parent_->get_port());
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error enabling ADC: %s", esp_err_to_name(err));
      return false;
    }
  } else
#endif
  {
    if (this->pdm_)
      config.mode = (i2s_mode_t) (config.mode | I2S_MODE_PDM);

    err = i2s_driver_install(this->parent_->get_port(), &config, 0, nullptr);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error installing I2S driver: %s", esp_err_to_name(err));
      return false;
    }

    i2s_pin_config_t pin_config = this->parent_->get_pin_config();
    pin_config.data_in_num = this->din_pin_;

    err = i2s_set_pin(this->parent_->get_port(), &pin_config);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Error setting I2S pin: %s", esp_err_to_name(err));
      return false;
    }
  }
#else
  i2s_chan_config_t chan_cfg = {
      .id = this->parent_->get_port(),
      .role = this->i2s_role_,
      .dma_desc_num = 4,
      .dma_frame_num = 256,
      .auto_clear = false,
  };
  /* Allocate a new RX channel and get the handle of this channel */
  err = i2s_new_channel(&chan_cfg, NULL, &this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error creating new I2S channel: %s", esp_err_to_name(err));
    return false;
  }

  i2s_clock_src_t clk_src = I2S_CLK_SRC_DEFAULT;
#ifdef I2S_CLK_SRC_APLL
  if (this->use_apll_) {
    clk_src = I2S_CLK_SRC_APLL;
  }
#endif
  i2s_std_gpio_config_t pin_config = this->parent_->get_pin_config();
#if SOC_I2S_SUPPORTS_PDM_RX
  if (this->pdm_) {
    i2s_pdm_rx_clk_config_t clk_cfg = {
        .sample_rate_hz = this->sample_rate_,
        .clk_src = clk_src,
        .mclk_multiple = this->mclk_multiple_,
        .dn_sample_mode = I2S_PDM_DSR_8S,
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
    i2s_std_clk_config_t clk_cfg = {
        .sample_rate_hz = this->sample_rate_,
        .clk_src = clk_src,
        .mclk_multiple = this->mclk_multiple_,
    };
    i2s_std_slot_config_t std_slot_cfg =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG((i2s_data_bit_width_t) this->slot_bit_width_, this->slot_mode_);
    std_slot_cfg.slot_bit_width = this->slot_bit_width_;
    std_slot_cfg.slot_mask = this->std_slot_mask_;

    pin_config.din = this->din_pin_;

    i2s_std_config_t std_cfg = {
        .clk_cfg = clk_cfg,
        .slot_cfg = std_slot_cfg,
        .gpio_cfg = pin_config,
    };
    /* Initialize the channel */
    err = i2s_channel_init_std_mode(this->rx_handle_, &std_cfg);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error initializing I2S channel: %s", esp_err_to_name(err));
    return false;
  }

  /* Before reading data, start the RX channel first */
  i2s_channel_enable(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error enabling I2S Microphone: %s", esp_err_to_name(err));
    return false;
  }
#endif

  this->configure_stream_settings_();  // redetermine the settings in case some settings were changed after compilation

  return true;
}

void I2SAudioMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;

  xSemaphoreGive(this->active_listeners_semaphore_);
}

void I2SAudioMicrophone::stop_driver_() {
  // There is no harm continuing to unload the driver if an error is ever returned by the various functions. This
  // ensures that we stop/unload the driver when it only partially starts.

  esp_err_t err;
#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_ADC
  if (this->adc_) {
    err = i2s_adc_disable(this->parent_->get_port());
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error disabling ADC - it may not have started: %s", esp_err_to_name(err));
    }
  }
#endif
  err = i2s_stop(this->parent_->get_port());
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error stopping I2S microphone - it may not have started: %s", esp_err_to_name(err));
  }
  err = i2s_driver_uninstall(this->parent_->get_port());
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error uninstalling I2S driver - it may not have started: %s", esp_err_to_name(err));
  }
#else
  /* Have to stop the channel before deleting it */
  err = i2s_channel_disable(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error stopping I2S microphone - it may not have started: %s", esp_err_to_name(err));
  }
  /* If the handle is not needed any more, delete it to release the channel resources */
  err = i2s_del_channel(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error deleting I2S channel - it may not have started: %s", esp_err_to_name(err));
  }
#endif
  this->parent_->unlock();
}

void I2SAudioMicrophone::mic_task(void *params) {
  I2SAudioMicrophone *this_microphone = (I2SAudioMicrophone *) params;
  xEventGroupSetBits(this_microphone->event_group_, MicrophoneEventGroupBits::TASK_STARTING);

  {  // Ensures the samples vector is freed when the task stops

    const size_t bytes_to_read = this_microphone->audio_stream_info_.ms_to_bytes(READ_DURATION_MS);
    std::vector<uint8_t> samples;
    samples.reserve(bytes_to_read);

    xEventGroupSetBits(this_microphone->event_group_, MicrophoneEventGroupBits::TASK_RUNNING);

    while (!(xEventGroupGetBits(this_microphone->event_group_) & MicrophoneEventGroupBits::COMMAND_STOP)) {
      if (this_microphone->data_callbacks_.size() > 0) {
        samples.resize(bytes_to_read);
        size_t bytes_read = this_microphone->read_(samples.data(), bytes_to_read, 2 * pdMS_TO_TICKS(READ_DURATION_MS));
        samples.resize(bytes_read);
        if (this_microphone->correct_dc_offset_) {
          this_microphone->fix_dc_offset_(samples);
        }
        this_microphone->data_callbacks_.call(samples);
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

void I2SAudioMicrophone::fix_dc_offset_(std::vector<uint8_t> &data) {
  const size_t bytes_per_sample = this->audio_stream_info_.samples_to_bytes(1);
  const uint32_t total_samples = this->audio_stream_info_.bytes_to_samples(data.size());

  if (total_samples == 0) {
    return;
  }

  int64_t offset_accumulator = 0;
  for (uint32_t sample_index = 0; sample_index < total_samples; ++sample_index) {
    const uint32_t byte_index = sample_index * bytes_per_sample;
    int32_t sample = audio::unpack_audio_sample_to_q31(&data[byte_index], bytes_per_sample);
    offset_accumulator += sample;
    sample -= this->dc_offset_;
    audio::pack_q31_as_audio_sample(sample, &data[byte_index], bytes_per_sample);
  }

  const int32_t new_offset = offset_accumulator / total_samples;
  this->dc_offset_ = new_offset / DC_OFFSET_MOVING_AVERAGE_COEFFICIENT_DENOMINATOR +
                     (DC_OFFSET_MOVING_AVERAGE_COEFFICIENT_DENOMINATOR - 1) * this->dc_offset_ /
                         DC_OFFSET_MOVING_AVERAGE_COEFFICIENT_DENOMINATOR;
}

size_t I2SAudioMicrophone::read_(uint8_t *buf, size_t len, TickType_t ticks_to_wait) {
  size_t bytes_read = 0;
#ifdef USE_I2S_LEGACY
  esp_err_t err = i2s_read(this->parent_->get_port(), buf, len, &bytes_read, ticks_to_wait);
#else
  // i2s_channel_read expects the timeout value in ms, not ticks
  esp_err_t err = i2s_channel_read(this->rx_handle_, buf, len, &bytes_read, pdTICKS_TO_MS(ticks_to_wait));
#endif
  if ((err != ESP_OK) && ((err != ESP_ERR_TIMEOUT) || (ticks_to_wait != 0))) {
    // Ignore ESP_ERR_TIMEOUT if ticks_to_wait = 0, as it will read the data on the next call
    if (!this->status_has_warning()) {
      // Avoid spamming the logs with the error message if its repeated
      ESP_LOGW(TAG, "Error reading from I2S microphone: %s", esp_err_to_name(err));
    }
    this->status_set_warning();
    return 0;
  }
  if ((bytes_read == 0) && (ticks_to_wait > 0)) {
    this->status_set_warning();
    return 0;
  }
  this->status_clear_warning();
#if defined(USE_ESP32_VARIANT_ESP32) and not defined(USE_I2S_LEGACY)
  // For ESP32 8/16 bit standard mono mode samples need to be switched.
  if (this->slot_mode_ == I2S_SLOT_MODE_MONO && this->slot_bit_width_ <= 16 && !this->pdm_) {
    size_t samples_read = bytes_read / sizeof(int16_t);
    for (int i = 0; i < samples_read; i += 2) {
      int16_t tmp = buf[i];
      buf[i] = buf[i + 1];
      buf[i + 1] = tmp;
    }
  }
#endif
  return bytes_read;
}

void I2SAudioMicrophone::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & MicrophoneEventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Task started, attempting to allocate buffer");
    xEventGroupClearBits(this->event_group_, MicrophoneEventGroupBits::TASK_STARTING);
  }

  if (event_group_bits & MicrophoneEventGroupBits::TASK_RUNNING) {
    ESP_LOGD(TAG, "Task is running and reading data");

    xEventGroupClearBits(this->event_group_, MicrophoneEventGroupBits::TASK_RUNNING);
    this->state_ = microphone::STATE_RUNNING;
  }

  if ((event_group_bits & MicrophoneEventGroupBits::TASK_STOPPED)) {
    ESP_LOGD(TAG, "Task finished, freeing resources and uninstalling I2S driver");

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
        this->status_momentary_error("I2S driver failed to start, unloading it and attempting again in 1 second", 1000);
        this->stop_driver_();  // Stop/frees whatever possibly started
        break;
      }

      if (this->task_handle_ == nullptr) {
        xTaskCreate(I2SAudioMicrophone::mic_task, "mic_task", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY,
                    &this->task_handle_);

        if (this->task_handle_ == nullptr) {
          this->status_momentary_error("Task failed to start, attempting again in 1 second", 1000);
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

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
