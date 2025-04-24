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

namespace esphome {
namespace i2s_audio {

static const size_t BUFFER_SIZE = 512;

static const char *const TAG = "i2s_audio.microphone";

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
}

void I2SAudioMicrophone::start() {
  if (this->is_failed())
    return;
  if (this->state_ == microphone::STATE_RUNNING)
    return;  // Already running
  this->state_ = microphone::STATE_STARTING;
}
void I2SAudioMicrophone::start_() {
  if (!this->parent_->try_lock()) {
    return;  // Waiting for another i2s to return lock
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
      .dma_buf_len = 256,
      .use_apll = this->use_apll_,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
      .mclk_multiple = I2S_MCLK_MULTIPLE_256,
      .bits_per_chan = this->bits_per_channel_,
  };

#if SOC_I2S_SUPPORTS_ADC
  if (this->adc_) {
    config.mode = (i2s_mode_t) (config.mode | I2S_MODE_ADC_BUILT_IN);
    err = i2s_driver_install(this->parent_->get_port(), &config, 0, nullptr);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error installing I2S driver: %s", esp_err_to_name(err));
      this->status_set_error();
      return;
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, this->adc_channel_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error setting ADC mode: %s", esp_err_to_name(err));
      this->status_set_error();
      return;
    }
    err = i2s_adc_enable(this->parent_->get_port());
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error enabling ADC: %s", esp_err_to_name(err));
      this->status_set_error();
      return;
    }

  } else
#endif
  {
    if (this->pdm_)
      config.mode = (i2s_mode_t) (config.mode | I2S_MODE_PDM);

    err = i2s_driver_install(this->parent_->get_port(), &config, 0, nullptr);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error installing I2S driver: %s", esp_err_to_name(err));
      this->status_set_error();
      return;
    }

    i2s_pin_config_t pin_config = this->parent_->get_pin_config();
    pin_config.data_in_num = this->din_pin_;

    err = i2s_set_pin(this->parent_->get_port(), &pin_config);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error setting I2S pin: %s", esp_err_to_name(err));
      this->status_set_error();
      return;
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
    ESP_LOGW(TAG, "Error creating new I2S channel: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
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
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
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
        .mclk_multiple = I2S_MCLK_MULTIPLE_256,
    };
    i2s_data_bit_width_t data_bit_width;
    if (this->slot_bit_width_ != I2S_SLOT_BIT_WIDTH_8BIT) {
      data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
    } else {
      data_bit_width = I2S_DATA_BIT_WIDTH_8BIT;
    }
    i2s_std_slot_config_t std_slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(data_bit_width, this->slot_mode_);
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
    ESP_LOGW(TAG, "Error initializing I2S channel: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
  }

  /* Before reading data, start the RX channel first */
  i2s_channel_enable(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error enabling I2S Microphone: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
  }
#endif

  this->state_ = microphone::STATE_RUNNING;
  this->high_freq_.start();
  this->status_clear_error();
}

void I2SAudioMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;
  if (this->state_ == microphone::STATE_STARTING) {
    this->state_ = microphone::STATE_STOPPED;
    return;
  }
  this->state_ = microphone::STATE_STOPPING;
}

void I2SAudioMicrophone::stop_() {
  esp_err_t err;
#ifdef USE_I2S_LEGACY
#if SOC_I2S_SUPPORTS_ADC
  if (this->adc_) {
    err = i2s_adc_disable(this->parent_->get_port());
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Error disabling ADC: %s", esp_err_to_name(err));
      this->status_set_error();
      return;
    }
  }
#endif
  err = i2s_stop(this->parent_->get_port());
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error stopping I2S microphone: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
  }
  err = i2s_driver_uninstall(this->parent_->get_port());
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error uninstalling I2S driver: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
  }
#else
  /* Have to stop the channel before deleting it */
  err = i2s_channel_disable(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error stopping I2S microphone: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
  }
  /* If the handle is not needed any more, delete it to release the channel resources */
  err = i2s_del_channel(this->rx_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error deleting I2S channel: %s", esp_err_to_name(err));
    this->status_set_error();
    return;
  }
#endif
  this->parent_->unlock();
  this->state_ = microphone::STATE_STOPPED;
  this->high_freq_.stop();
  this->status_clear_error();
}

size_t I2SAudioMicrophone::read(int16_t *buf, size_t len) {
  size_t bytes_read = 0;
#ifdef USE_I2S_LEGACY
  esp_err_t err = i2s_read(this->parent_->get_port(), buf, len, &bytes_read, (100 / portTICK_PERIOD_MS));
#else
  esp_err_t err = i2s_channel_read(this->rx_handle_, buf, len, &bytes_read, (100 / portTICK_PERIOD_MS));
#endif
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error reading from I2S microphone: %s", esp_err_to_name(err));
    this->status_set_warning();
    return 0;
  }
  if (bytes_read == 0) {
    this->status_set_warning();
    return 0;
  }
  this->status_clear_warning();
  // ESP-IDF I2S implementation right-extends 8-bit data to 16 bits,
  // and 24-bit data to 32 bits.
#ifdef USE_I2S_LEGACY
  switch (this->bits_per_sample_) {
    case I2S_BITS_PER_SAMPLE_8BIT:
    case I2S_BITS_PER_SAMPLE_16BIT:
      return bytes_read;
    case I2S_BITS_PER_SAMPLE_24BIT:
    case I2S_BITS_PER_SAMPLE_32BIT: {
      size_t samples_read = bytes_read / sizeof(int32_t);
      for (size_t i = 0; i < samples_read; i++) {
        int32_t temp = reinterpret_cast<int32_t *>(buf)[i] >> 14;
        buf[i] = clamp<int16_t>(temp, INT16_MIN, INT16_MAX);
      }
      return samples_read * sizeof(int16_t);
    }
    default:
      ESP_LOGE(TAG, "Unsupported bits per sample: %d", this->bits_per_sample_);
      return 0;
  }
#else
#ifndef USE_ESP32_VARIANT_ESP32
  // For newer ESP32 variants 8 bit data needs to be extended to 16 bit.
  if (this->slot_bit_width_ == I2S_SLOT_BIT_WIDTH_8BIT) {
    size_t samples_read = bytes_read / sizeof(int8_t);
    for (size_t i = samples_read - 1; i >= 0; i--) {
      int16_t temp = static_cast<int16_t>(reinterpret_cast<int8_t *>(buf)[i]) << 8;
      buf[i] = temp;
    }
    return samples_read * sizeof(int16_t);
  }
#else
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
#endif
}

void I2SAudioMicrophone::read_() {
  std::vector<int16_t> samples;
  samples.resize(BUFFER_SIZE);
  size_t bytes_read = this->read(samples.data(), BUFFER_SIZE / sizeof(int16_t));
  samples.resize(bytes_read / sizeof(int16_t));
  this->data_callbacks_.call(samples);
}

void I2SAudioMicrophone::loop() {
  switch (this->state_) {
    case microphone::STATE_STOPPED:
      break;
    case microphone::STATE_STARTING:
      this->start_();
      break;
    case microphone::STATE_RUNNING:
      if (this->data_callbacks_.size() > 0) {
        this->read_();
      }
      break;
    case microphone::STATE_STOPPING:
      this->stop_();
      break;
  }
}

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
