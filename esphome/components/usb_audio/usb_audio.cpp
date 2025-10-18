#include "usb_audio.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "esphome/core/log.h"

#include "esp_err.h"

namespace esphome {
namespace usb_audio {

static const char *const TAG = "usb_audio";

void USBAudioComponent::set_microphone_params(uint8_t channels, uint16_t bits, uint32_t sample_rate) {
  this->mic_config_.channels = channels;
  this->mic_config_.bits_per_sample = bits;
  this->mic_config_.sample_rate = sample_rate;
  this->mic_config_.configured = true;
}

void USBAudioComponent::set_speaker_params(uint8_t channels, uint16_t bits, uint32_t sample_rate) {
  this->speaker_config_.channels = channels;
  this->speaker_config_.bits_per_sample = bits;
  this->speaker_config_.sample_rate = sample_rate;
  this->speaker_config_.configured = true;
}

void USBAudioComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up USB audio host");

  if (this->mic_config_.buffer_size == 0) {
    this->mic_config_.buffer_size = USB_AUDIO_DEFAULT_BUFFER_SIZE;
  }
  if (this->speaker_config_.buffer_size == 0) {
    this->speaker_config_.buffer_size = USB_AUDIO_DEFAULT_BUFFER_SIZE;
  }

  esp_err_t err = usb_streaming_state_register(&USBAudioComponent::state_callback_, this);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to register USB state callback: %s", esp_err_to_name(err));
  }

  this->configure_streams_();

  err = usb_streaming_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start USB streaming: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  this->stream_started_ = true;

  err = usb_streaming_connect_wait(this->connect_timeout_ms_);
  if (err == ESP_OK) {
    this->device_connected_ = true;
  } else if (err == ESP_ERR_TIMEOUT) {
    ESP_LOGW(TAG, "USB audio device did not connect within %u ms", this->connect_timeout_ms_);
  } else if (err != ESP_OK) {
    ESP_LOGW(TAG, "Error waiting for USB audio device: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "USB Audio:");
  ESP_LOGCONFIG(TAG, "  Microphone configured: %s", YESNO(this->mic_config_.configured));
  ESP_LOGCONFIG(TAG, "  Microphone buffer size: %u", this->mic_config_.buffer_size);
  ESP_LOGCONFIG(TAG, "  Speaker configured: %s", YESNO(this->speaker_config_.configured));
  ESP_LOGCONFIG(TAG, "  Speaker buffer size: %u", this->speaker_config_.buffer_size);
  ESP_LOGCONFIG(TAG, "  Connect timeout: %u ms", this->connect_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Device connected: %s", YESNO(this->device_connected_));
}

bool USBAudioComponent::ensure_started() {
  if (this->stream_started_) {
    return true;
  }

  this->configure_streams_();
  esp_err_t err = usb_streaming_start();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start USB streaming: %s", esp_err_to_name(err));
    return false;
  }
  this->stream_started_ = true;
  return true;
}

void USBAudioComponent::resume_microphone() {
  if (!this->ensure_started()) {
    return;
  }
  esp_err_t err = usb_streaming_control(STREAM_UAC_MIC, CTRL_RESUME, nullptr);
  if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
    ESP_LOGW(TAG, "Failed to resume microphone stream: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::suspend_microphone() {
  if (!this->stream_started_) {
    return;
  }
  esp_err_t err = usb_streaming_control(STREAM_UAC_MIC, CTRL_SUSPEND, nullptr);
  if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
    ESP_LOGW(TAG, "Failed to suspend microphone stream: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::resume_speaker() {
  if (!this->ensure_started()) {
    return;
  }
  esp_err_t err = usb_streaming_control(STREAM_UAC_SPK, CTRL_RESUME, nullptr);
  if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
    ESP_LOGW(TAG, "Failed to resume speaker stream: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::suspend_speaker() {
  if (!this->stream_started_) {
    return;
  }
  esp_err_t err = usb_streaming_control(STREAM_UAC_SPK, CTRL_SUSPEND, nullptr);
  if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) {
    ESP_LOGW(TAG, "Failed to suspend speaker stream: %s", esp_err_to_name(err));
  }
}

esp_err_t USBAudioComponent::read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
  if (!this->stream_started_) {
    return ESP_ERR_INVALID_STATE;
  }
  return uac_mic_streaming_read(buffer, size, bytes_read, timeout_ms);
}

esp_err_t USBAudioComponent::write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms) {
  if (!this->stream_started_) {
    return ESP_ERR_INVALID_STATE;
  }
  // The underlying API expects a non-const pointer. The data is not modified by the driver.
  return uac_spk_streaming_write(const_cast<uint8_t *>(data), length, timeout_ms);
}

void USBAudioComponent::configure_streams_() {
  uac_config_t config = {};

  config.mic_ch_num = this->mic_config_.configured ? this->mic_config_.channels : UAC_CH_ANY;
  config.mic_bit_resolution = this->mic_config_.configured ? this->mic_config_.bits_per_sample : UAC_BITS_ANY;
  config.mic_samples_frequence = this->mic_config_.configured ? this->mic_config_.sample_rate : UAC_FREQUENCY_ANY;
  config.mic_buf_size = this->mic_config_.buffer_size;
  config.mic_cb = nullptr;
  config.mic_cb_arg = nullptr;

  config.spk_ch_num = this->speaker_config_.configured ? this->speaker_config_.channels : UAC_CH_ANY;
  config.spk_bit_resolution = this->speaker_config_.configured ? this->speaker_config_.bits_per_sample : UAC_BITS_ANY;
  config.spk_samples_frequence =
      this->speaker_config_.configured ? this->speaker_config_.sample_rate : UAC_FREQUENCY_ANY;
  config.spk_buf_size = this->speaker_config_.buffer_size;

  esp_err_t err = uac_streaming_config(&config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to configure USB audio streams: %s", esp_err_to_name(err));
    this->mark_failed();
  }
}

void USBAudioComponent::handle_state_change_(usb_stream_state_t state) {
  this->device_connected_ = (state == STREAM_CONNECTED);
  ESP_LOGD(TAG, "USB audio device state changed: %s", this->device_connected_ ? "connected" : "disconnected");
}

void USBAudioComponent::state_callback_(usb_stream_state_t state, void *user_data) {
  auto *self = static_cast<USBAudioComponent *>(user_data);
  if (self == nullptr) {
    return;
  }
  self->handle_state_change_(state);
}

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)
