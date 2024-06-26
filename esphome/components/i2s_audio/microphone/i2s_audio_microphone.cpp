#include "i2s_audio_microphone.h"

#ifdef USE_ESP32

#include <driver/i2s.h>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace i2s_audio {

static const size_t BUFFER_SIZE = 512;

static const char *const TAG = "i2s_audio.microphone";

void I2SAudioMicrophone::setup() {
  ESP_LOGCONFIG(TAG, "Setting up I2S Audio Microphone...");
#if SOC_I2S_SUPPORTS_ADC
  if (this->adc_) {
    if (this->parent_->get_port() != I2S_NUM_0) {
      ESP_LOGE(TAG, "Internal ADC only works on I2S0!");
      this->mark_failed();
      return;
    }
  } else
#endif
      if (this->pdm_) {
    if (this->parent_->get_port() != I2S_NUM_0) {
      ESP_LOGE(TAG, "PDM only works on I2S0!");
      this->mark_failed();
      return;
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
  i2s_driver_config_t config = {
      .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX),
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
      .bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
  };

  esp_err_t err;

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
  this->parent_->unlock();
  this->state_ = microphone::STATE_STOPPED;
  this->high_freq_.stop();
  this->status_clear_error();
}

size_t I2SAudioMicrophone::read(int16_t *buf, size_t len) {
  size_t bytes_read = 0;
  esp_err_t err = i2s_read(this->parent_->get_port(), buf, len, &bytes_read, (100 / portTICK_PERIOD_MS));
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
  if (this->bits_per_sample_ == I2S_BITS_PER_SAMPLE_16BIT) {
    return bytes_read;
  } else if (this->bits_per_sample_ == I2S_BITS_PER_SAMPLE_32BIT) {
    std::vector<int16_t> samples;
    size_t samples_read = bytes_read / sizeof(int32_t);
    samples.resize(samples_read);
    for (size_t i = 0; i < samples_read; i++) {
      int32_t temp = reinterpret_cast<int32_t *>(buf)[i] >> 14;
      samples[i] = clamp<int16_t>(temp, INT16_MIN, INT16_MAX);
    }
    memcpy(buf, samples.data(), samples_read * sizeof(int16_t));
    return samples_read * sizeof(int16_t);
  } else {
    ESP_LOGE(TAG, "Unsupported bits per sample: %d", this->bits_per_sample_);
    return 0;
  }
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
