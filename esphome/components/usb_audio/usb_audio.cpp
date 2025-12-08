#include "usb_audio.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

extern "C" {
#include "esp_err.h"
#include "esp_intr_alloc.h"
}

namespace esphome {
namespace usb_audio {

namespace {
const char *const TAG = "usb_audio";

constexpr UBaseType_t USB_HOST_TASK_PRIORITY = 8;
constexpr uint32_t USB_HOST_TASK_STACK_SIZE = 4096;
constexpr uint32_t EVENT_QUEUE_LENGTH = 64;
constexpr TickType_t HOST_EVENT_TIMEOUT_TICKS = pdMS_TO_TICKS(100);
}  // namespace

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

  this->event_queue_ = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(USBAudioEvent));
  if (this->event_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create USB audio event queue");
    this->mark_failed();
    return;
  }

  usb_host_config_t host_cfg = {
      .skip_phy_setup = false,
      .intr_flags = ESP_INTR_FLAG_LEVEL1,
  };

  esp_err_t err = usb_host_install(&host_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_install failed: %s", esp_err_to_name(err));
    if (this->event_queue_ != nullptr) {
      vQueueDelete(this->event_queue_);
      this->event_queue_ = nullptr;
    }
    this->mark_failed();
    return;
  }
  this->host_installed_ = true;

  uac_host_driver_config_t uac_cfg = {
      .create_background_task = true,
      .task_priority = USB_HOST_TASK_PRIORITY,
      .stack_size = USB_HOST_TASK_STACK_SIZE,
      .core_id = 0,
      .callback = &USBAudioComponent::driver_event_stub,
      .callback_arg = this,
  };

  err = uac_host_install(&uac_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uac_host_install failed: %s", esp_err_to_name(err));
    usb_host_uninstall();
    this->host_installed_ = false;
    if (this->event_queue_ != nullptr) {
      vQueueDelete(this->event_queue_);
      this->event_queue_ = nullptr;
    }
    this->mark_failed();
    return;
  }
  this->uac_installed_ = true;

  this->host_task_running_ = true;
  BaseType_t created =
      xTaskCreatePinnedToCore(&USBAudioComponent::usb_host_task_stub, "usb_audio_host", USB_HOST_TASK_STACK_SIZE, this,
                              USB_HOST_TASK_PRIORITY, &this->usb_host_task_handle_, 0);
  if (created != pdPASS) {
    ESP_LOGE(TAG, "Failed to create USB host event task");
    this->host_task_running_ = false;
    this->usb_host_task_handle_ = nullptr;
    uac_host_uninstall();
    this->uac_installed_ = false;
    usb_host_uninstall();
    this->host_installed_ = false;
    if (this->event_queue_ != nullptr) {
      vQueueDelete(this->event_queue_);
      this->event_queue_ = nullptr;
    }
    this->mark_failed();
    return;
  }
}

void USBAudioComponent::loop() {
  if (this->event_queue_ == nullptr) {
    return;
  }

  USBAudioEvent event;
  while (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
    this->process_event_(event);
  }

  if (this->speaker_handle_ != nullptr) {
    if (this->pending_speaker_volume_) {
      this->apply_speaker_volume_();
    }
    if (this->pending_speaker_mute_) {
      this->apply_speaker_mute_();
    }
  }
}

void USBAudioComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "USB Audio:");
  ESP_LOGCONFIG(TAG, "  Host installed: %s", YESNO(this->host_installed_));
  ESP_LOGCONFIG(TAG, "  UAC driver installed: %s", YESNO(this->uac_installed_));
  ESP_LOGCONFIG(TAG, "  Device connected: %s", YESNO(this->device_connected_));
  ESP_LOGCONFIG(TAG, "  Microphone configured: %s", YESNO(this->mic_config_.configured));
  ESP_LOGCONFIG(TAG, "  Microphone buffer size: %u", this->mic_config_.buffer_size);
  ESP_LOGCONFIG(TAG, "  Speaker configured: %s", YESNO(this->speaker_config_.configured));
  ESP_LOGCONFIG(TAG, "  Speaker buffer size: %u", this->speaker_config_.buffer_size);
  ESP_LOGCONFIG(TAG, "  Connect timeout: %u ms", this->connect_timeout_ms_);
}

bool USBAudioComponent::ensure_started(Endpoint endpoint) {
  if (this->event_queue_ != nullptr) {
    USBAudioEvent event;
    while (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
      this->process_event_(event);
    }
  }

  const uint32_t start_ms = millis();

  auto get_handle = [this, endpoint]() -> uac_host_device_handle_t & {
    if (endpoint == Endpoint::SPEAKER) {
      return this->speaker_handle_;
    }
    return this->microphone_handle_;
  };

  auto &target_handle = get_handle();

  while (target_handle == nullptr && this->connect_timeout_ms_ != 0 &&
         (millis() - start_ms) < this->connect_timeout_ms_) {
    if (this->event_queue_ != nullptr) {
      USBAudioEvent event;
      if (xQueueReceive(this->event_queue_, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
        this->process_event_(event);
        continue;
      }
    } else {
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }

  const char *role = endpoint == Endpoint::SPEAKER ? "speaker" : "microphone";

  if (target_handle == nullptr) {
    ESP_LOGW(TAG, "USB %s interface not connected", role);
    return false;
  }

  bool ok = true;
  if (this->speaker_handle_ != nullptr && !this->speaker_stream_started_) {
    ok &= this->start_speaker_stream_();
  }
  if (this->microphone_handle_ != nullptr && !this->microphone_stream_started_) {
    ok &= this->start_microphone_stream_();
  }

  this->device_connected_ = (this->speaker_handle_ != nullptr) || (this->microphone_handle_ != nullptr);

  bool target_started =
      endpoint == Endpoint::SPEAKER ? this->speaker_stream_started_ : this->microphone_stream_started_;
  return ok && target_started;
}

bool USBAudioComponent::teardown() {
  if (!this->teardown_initiated_) {
    this->teardown_initiated_ = true;
    this->host_task_running_ = false;
  }

  this->close_all_devices_();

  if (this->uac_installed_) {
    esp_err_t err = uac_host_uninstall();
    if (err == ESP_ERR_INVALID_STATE) {
      vTaskDelay(pdMS_TO_TICKS(20));
      return false;
    }
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uac_host_uninstall returned %s", esp_err_to_name(err));
    }
    this->uac_installed_ = false;
  }

  if (this->usb_host_task_handle_ != nullptr) {
    vTaskDelay(pdMS_TO_TICKS(20));
    return false;
  }

  if (this->host_installed_) {
    esp_err_t err = usb_host_uninstall();
    if (err == ESP_ERR_INVALID_STATE) {
      vTaskDelay(pdMS_TO_TICKS(20));
      return false;
    }
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "usb_host_uninstall returned %s", esp_err_to_name(err));
    }
    this->host_installed_ = false;
  }

  if (this->event_queue_ != nullptr) {
    USBAudioEvent event;
    while (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
      // Drain queue completely before deletion.
    }
    vQueueDelete(this->event_queue_);
    this->event_queue_ = nullptr;
  }

  return true;
}

void USBAudioComponent::resume_microphone() {
  if (this->microphone_handle_ == nullptr) {
    return;
  }
  if (!this->microphone_stream_started_) {
    if (!this->start_microphone_stream_()) {
      return;
    }
  }
  esp_err_t err = uac_host_device_resume(this->microphone_handle_);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to resume microphone stream: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::suspend_microphone() {
  if (this->microphone_handle_ == nullptr) {
    return;
  }
  esp_err_t err = uac_host_device_suspend(this->microphone_handle_);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to suspend microphone stream: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::resume_speaker() {
  if (this->speaker_handle_ == nullptr) {
    return;
  }
  if (!this->speaker_stream_started_) {
    if (!this->start_speaker_stream_()) {
      return;
    }
  }
  this->pending_speaker_volume_ = true;
  this->pending_speaker_mute_ = true;
  esp_err_t err = uac_host_device_resume(this->speaker_handle_);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to resume speaker stream: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::suspend_speaker() {
  if (this->speaker_handle_ == nullptr) {
    return;
  }
  esp_err_t err = uac_host_device_suspend(this->speaker_handle_);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to suspend speaker stream: %s", esp_err_to_name(err));
  }
}

void USBAudioComponent::close_all_devices_() {
  if (this->speaker_handle_ != nullptr) {
    auto handle = this->speaker_handle_;
    this->handle_disconnect_(handle);
  }

  if (this->microphone_handle_ != nullptr) {
    auto handle = this->microphone_handle_;
    this->handle_disconnect_(handle);
  }

  if (this->event_queue_ != nullptr) {
    USBAudioEvent pending_event;
    while (xQueueReceive(this->event_queue_, &pending_event, 0) == pdTRUE) {
      // Drain remaining events before deleting the queue.
    }
  }
}

esp_err_t USBAudioComponent::read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
  if (this->microphone_handle_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  if (bytes_read != nullptr) {
    *bytes_read = 0;
  }
  uint32_t actual = 0;
  TickType_t timeout_ticks = timeout_ms == 0 ? 0 : pdMS_TO_TICKS(timeout_ms);
  esp_err_t err =
      uac_host_device_read(this->microphone_handle_, buffer, static_cast<uint32_t>(size), &actual, timeout_ticks);
  if (err == ESP_FAIL && actual == 0) {
    // The driver reports ESP_FAIL when the ring buffer has no data within the timeout window; treat like timeout.
    err = ESP_ERR_TIMEOUT;
  }
  if (bytes_read != nullptr) {
    *bytes_read = actual;
  }
  return err;
}

esp_err_t USBAudioComponent::write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms) {
  if (this->speaker_handle_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  TickType_t timeout_ticks = timeout_ms == 0 ? 0 : pdMS_TO_TICKS(timeout_ms);
  esp_err_t err = uac_host_device_write(this->speaker_handle_, const_cast<uint8_t *>(data),
                                        static_cast<uint32_t>(length), timeout_ticks);
  if (err == ESP_FAIL || err == ESP_ERR_INVALID_STATE) {
    // Ring buffer full or stream inactive; report as timeout so callers back off and retry.
    err = ESP_ERR_TIMEOUT;
  }

  return err;
}

void USBAudioComponent::process_event_(const USBAudioEvent &event) {
  switch (event.type) {
    case USBAudioEvent::Type::DRIVER_TX_CONNECTED:
    case USBAudioEvent::Type::DRIVER_RX_CONNECTED:
      this->handle_driver_event_(event);
      break;
    case USBAudioEvent::Type::DRIVER_DISCONNECTED:
      this->handle_driver_disconnect_(event.driver.addr, event.driver.iface_num);
      break;
    case USBAudioEvent::Type::DEVICE_EVENT:
      this->handle_device_event_(event);
      break;
  }
}

void USBAudioComponent::handle_driver_event_(const USBAudioEvent &event) {
  if (!this->open_stream_(event)) {
    ESP_LOGW(TAG, "Failed to open %s interface",
             event.type == USBAudioEvent::Type::DRIVER_TX_CONNECTED ? "speaker" : "microphone");
  }
}

void USBAudioComponent::handle_device_event_(const USBAudioEvent &event) {
  switch (event.device.event) {
    case UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR:
      ESP_LOGW(TAG, "UAC transfer error reported");
      break;
    case UAC_HOST_DEVICE_EVENT_RX_DONE:
    case UAC_HOST_DEVICE_EVENT_TX_DONE:
    default:
      break;
  }
}

void USBAudioComponent::handle_driver_disconnect_(uint8_t addr, uint8_t iface_num) {
  bool handled = false;

  if (this->speaker_handle_ != nullptr && this->speaker_iface_ == iface_num && this->speaker_addr_ == addr) {
    auto handle = this->speaker_handle_;
    ESP_LOGI(TAG, "Speaker interface %u disconnected", iface_num);
    this->handle_disconnect_(handle);
    handled = true;
  }

  if (this->microphone_handle_ != nullptr && this->microphone_iface_ == iface_num && this->microphone_addr_ == addr) {
    auto handle = this->microphone_handle_;
    ESP_LOGI(TAG, "Microphone interface %u disconnected", iface_num);
    this->handle_disconnect_(handle);
    handled = true;
  }

  if (!handled) {
    ESP_LOGD(TAG, "Driver disconnect event for interface %u with no active stream", iface_num);
  }
}

bool USBAudioComponent::open_stream_(const USBAudioEvent &event) {
  const bool is_speaker = event.type == USBAudioEvent::Type::DRIVER_TX_CONNECTED;
  AudioEndpointConfig &cfg = is_speaker ? this->speaker_config_ : this->mic_config_;

  if (!this->is_ready_(cfg)) {
    ESP_LOGW(TAG, "%s interface connected before configuration", is_speaker ? "Speaker" : "Microphone");
    return false;
  }

  if (is_speaker && this->speaker_handle_ != nullptr) {
    ESP_LOGW(TAG, "Speaker interface already open, ignoring duplicate event");
    return true;
  }
  if (!is_speaker && this->microphone_handle_ != nullptr) {
    ESP_LOGW(TAG, "Microphone interface already open, ignoring duplicate event");
    return true;
  }

  uac_host_device_config_t dev_cfg = {
      .addr = event.driver.addr,
      .iface_num = event.driver.iface_num,
      .buffer_size = cfg.buffer_size,
      .buffer_threshold = std::max<uint32_t>(cfg.buffer_size / 4, 1U),
      .callback = &USBAudioComponent::device_event_stub,
      .callback_arg = this,
  };

  uac_host_device_handle_t handle = nullptr;
  esp_err_t err = uac_host_device_open(&dev_cfg, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uac_host_device_open failed: %s", esp_err_to_name(err));
    return false;
  }

  if (is_speaker) {
    this->speaker_handle_ = handle;
    this->speaker_iface_ = event.driver.iface_num;
    this->speaker_addr_ = event.driver.addr;
    this->speaker_stream_started_ = false;
    this->device_connected_ = true;
    ESP_LOGI(TAG, "Speaker interface %u connected (addr=%u)", this->speaker_iface_, this->speaker_addr_);
    if (!this->start_speaker_stream_()) {
      ESP_LOGW(TAG, "Unable to start speaker stream");
    }
  } else {
    this->microphone_handle_ = handle;
    this->microphone_iface_ = event.driver.iface_num;
    this->microphone_addr_ = event.driver.addr;
    this->microphone_stream_started_ = false;
    this->device_connected_ = true;
    ESP_LOGI(TAG, "Microphone interface %u connected (addr=%u)", this->microphone_iface_, this->microphone_addr_);
    if (!this->start_microphone_stream_()) {
      ESP_LOGW(TAG, "Unable to start microphone stream");
    }
  }

  return true;
}

bool USBAudioComponent::start_speaker_stream_() {
  if (this->speaker_handle_ == nullptr || !this->is_ready_(this->speaker_config_)) {
    return false;
  }

  uac_host_stream_config_t stm_cfg{};
  if (!this->populate_stream_config_(this->speaker_handle_, this->speaker_config_, stm_cfg, "Speaker")) {
    return false;
  }

  esp_err_t err = uac_host_device_start(this->speaker_handle_, &stm_cfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "uac_host_device_start (speaker) failed: %s", esp_err_to_name(err));
    return false;
  }

  this->speaker_stream_started_ = true;
  this->device_connected_ = true;
  this->pending_speaker_volume_ = true;
  this->pending_speaker_mute_ = true;
  this->apply_speaker_volume_();
  this->apply_speaker_mute_();
  return true;
}

bool USBAudioComponent::start_microphone_stream_() {
  if (this->microphone_handle_ == nullptr || !this->is_ready_(this->mic_config_)) {
    return false;
  }

  uac_host_stream_config_t stm_cfg{};
  if (!this->populate_stream_config_(this->microphone_handle_, this->mic_config_, stm_cfg, "Microphone")) {
    return false;
  }

  esp_err_t err = uac_host_device_start(this->microphone_handle_, &stm_cfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "uac_host_device_start (microphone) failed: %s", esp_err_to_name(err));
    return false;
  }

  this->microphone_stream_started_ = true;
  this->device_connected_ = true;
  return true;
}

bool USBAudioComponent::populate_stream_config_(uac_host_device_handle_t handle, const AudioEndpointConfig &config,
                                                uac_host_stream_config_t &stream_config, const char *role) {
  if (handle == nullptr) {
    return false;
  }

  if (config.bits_per_sample == 0 || config.bits_per_sample > UINT8_MAX) {
    ESP_LOGE(TAG, "%s configuration has unsupported bit depth %u", role, config.bits_per_sample);
    return false;
  }

  if (config.channels == 0 || config.sample_rate == 0) {
    ESP_LOGE(TAG, "%s configuration missing channels (%u) or sample_rate (%" PRIu32 ")", role, config.channels,
             config.sample_rate);
    return false;
  }

  uac_host_dev_info_t info{};
  esp_err_t err = uac_host_get_device_info(handle, &info);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to query %s interface info: %s", role, esp_err_to_name(err));
    return false;
  }

  for (uint8_t alt = 1; alt <= info.iface_alt_num; alt++) {
    uac_host_dev_alt_param_t alt_param{};
    err = uac_host_get_device_alt_param(handle, alt, &alt_param);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Failed to query %s alt setting %u: %s", role, alt, esp_err_to_name(err));
      continue;
    }

    if (alt_param.channels != config.channels) {
      continue;
    }
    if (alt_param.bit_resolution != config.bits_per_sample) {
      continue;
    }

    bool frequency_supported = false;
    if (alt_param.sample_freq_type == 0) {
      if (config.sample_rate >= alt_param.sample_freq_lower && config.sample_rate <= alt_param.sample_freq_upper) {
        frequency_supported = true;
      }
    } else {
      const uint8_t freq_count = std::min<uint8_t>(alt_param.sample_freq_type, UAC_FREQ_NUM_MAX);
      for (uint8_t i = 0; i < freq_count; i++) {
        if (alt_param.sample_freq[i] == config.sample_rate) {
          frequency_supported = true;
          break;
        }
      }
    }

    if (!frequency_supported) {
      continue;
    }

    stream_config.channels = config.channels;
    stream_config.bit_resolution = static_cast<uint8_t>(config.bits_per_sample);
    stream_config.sample_freq = config.sample_rate;
    stream_config.flags = 0;
    return true;
  }

  this->log_alt_capabilities_(role, config, info, handle);
  return false;
}

void USBAudioComponent::log_alt_capabilities_(const char *role, const AudioEndpointConfig &config,
                                              const uac_host_dev_info_t &info, uac_host_device_handle_t handle) {
  ESP_LOGE(TAG, "%s interface %u does not support requested format (channels=%u bits=%u sample_rate=%" PRIu32 " Hz)",
           role, info.iface_num, config.channels, config.bits_per_sample, config.sample_rate);

  for (uint8_t alt = 1; alt <= info.iface_alt_num; alt++) {
    uac_host_dev_alt_param_t alt_param{};
    if (uac_host_get_device_alt_param(handle, alt, &alt_param) != ESP_OK) {
      continue;
    }

    if (alt_param.sample_freq_type == 0) {
      ESP_LOGI(TAG, "  alt %u: channels=%u bits=%u sample rate range=%" PRIu32 "-%" PRIu32 " Hz", alt,
               alt_param.channels, alt_param.bit_resolution, alt_param.sample_freq_lower, alt_param.sample_freq_upper);
    } else {
      ESP_LOGI(TAG, "  alt %u: channels=%u bits=%u sample rates:", alt, alt_param.channels, alt_param.bit_resolution);
      const uint8_t freq_count = std::min<uint8_t>(alt_param.sample_freq_type, UAC_FREQ_NUM_MAX);
      for (uint8_t i = 0; i < freq_count; i++) {
        ESP_LOGI(TAG, "    %" PRIu32 " Hz", alt_param.sample_freq[i]);
      }
    }
  }
}

void USBAudioComponent::set_speaker_volume_level(float volume) {
  this->speaker_volume_level_ = std::clamp(volume, 0.0f, 1.0f);
  if (this->speaker_volume_supported_) {
    this->speaker_volume_warned_ = false;
  }
  this->pending_speaker_volume_ = true;
  this->apply_speaker_volume_();
}

void USBAudioComponent::set_speaker_mute_state(bool mute_state) {
  this->speaker_muted_ = mute_state;
  if (this->speaker_mute_supported_) {
    this->speaker_mute_warned_ = false;
  }
  this->pending_speaker_mute_ = true;
  this->apply_speaker_mute_();
}

bool USBAudioComponent::apply_speaker_volume_() {
  if (this->speaker_handle_ == nullptr) {
    return false;
  }
  if (!this->speaker_volume_supported_) {
    this->pending_speaker_volume_ = false;
    return false;
  }

  const int rounded = static_cast<int>(std::lround(this->speaker_volume_level_ * 100.0f));
  const uint8_t volume = static_cast<uint8_t>(std::clamp(rounded, 0, 100));
  esp_err_t err = uac_host_device_set_volume(this->speaker_handle_, volume);
  if (err == ESP_OK) {
    this->pending_speaker_volume_ = false;
    this->speaker_volume_warned_ = false;
    return true;
  }
  if (err == ESP_ERR_NOT_SUPPORTED) {
    if (!this->speaker_volume_warned_) {
      ESP_LOGW(TAG, "Speaker volume control not supported by device");
      this->speaker_volume_warned_ = true;
    }
    this->speaker_volume_supported_ = false;
    this->pending_speaker_volume_ = false;
    return false;
  }
  if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_TIMEOUT) {
    return false;
  }
  if (!this->speaker_volume_warned_) {
    ESP_LOGW(TAG, "Failed to set speaker volume: %s", esp_err_to_name(err));
    this->speaker_volume_warned_ = true;
  }
  return false;
}

bool USBAudioComponent::apply_speaker_mute_() {
  if (this->speaker_handle_ == nullptr) {
    return false;
  }
  if (!this->speaker_mute_supported_) {
    this->pending_speaker_mute_ = false;
    return false;
  }

  esp_err_t err = uac_host_device_set_mute(this->speaker_handle_, this->speaker_muted_);
  if (err == ESP_OK) {
    this->pending_speaker_mute_ = false;
    this->speaker_mute_warned_ = false;
    return true;
  }
  if (err == ESP_ERR_NOT_SUPPORTED) {
    if (!this->speaker_mute_warned_) {
      ESP_LOGW(TAG, "Speaker mute control not supported by device");
      this->speaker_mute_warned_ = true;
    }
    this->speaker_mute_supported_ = false;
    this->pending_speaker_mute_ = false;
    return false;
  }
  if (err == ESP_ERR_INVALID_STATE || err == ESP_ERR_TIMEOUT) {
    return false;
  }
  if (!this->speaker_mute_warned_) {
    ESP_LOGW(TAG, "Failed to set speaker mute: %s", esp_err_to_name(err));
    this->speaker_mute_warned_ = true;
  }
  return false;
}

void USBAudioComponent::handle_disconnect_(uac_host_device_handle_t handle) {
  if (handle == nullptr) {
    return;
  }

  if (handle == this->speaker_handle_) {
    if (this->speaker_stream_started_) {
      esp_err_t err = uac_host_device_stop(handle);
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to stop speaker stream: %s", esp_err_to_name(err));
      }
    }
    this->speaker_handle_ = nullptr;
    this->speaker_stream_started_ = false;
    this->speaker_iface_ = 0;
    this->speaker_addr_ = 0;
    this->pending_speaker_volume_ = true;
    this->pending_speaker_mute_ = true;
    this->speaker_volume_supported_ = true;
    this->speaker_mute_supported_ = true;
    this->speaker_volume_warned_ = false;
    this->speaker_mute_warned_ = false;
  }
  if (handle == this->microphone_handle_) {
    if (this->microphone_stream_started_) {
      esp_err_t err = uac_host_device_stop(handle);
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to stop microphone stream: %s", esp_err_to_name(err));
      }
    }
    this->microphone_handle_ = nullptr;
    this->microphone_stream_started_ = false;
    this->microphone_iface_ = 0;
    this->microphone_addr_ = 0;
  }

  esp_err_t err = uac_host_device_close(handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uac_host_device_close failed: %s", esp_err_to_name(err));
  }

  this->device_connected_ = (this->speaker_handle_ != nullptr) || (this->microphone_handle_ != nullptr);
}

void USBAudioComponent::driver_event_stub(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event,
                                          void *user_data) {
  auto *self = static_cast<USBAudioComponent *>(user_data);
  if (self == nullptr || self->event_queue_ == nullptr) {
    return;
  }

  USBAudioEvent evt;
  switch (event) {
    case UAC_HOST_DRIVER_EVENT_TX_CONNECTED:
      evt.type = USBAudioEvent::Type::DRIVER_TX_CONNECTED;
      break;
    case UAC_HOST_DRIVER_EVENT_RX_CONNECTED:
      evt.type = USBAudioEvent::Type::DRIVER_RX_CONNECTED;
      break;
    case UAC_HOST_DRIVER_EVENT_DISCONNECTED:
      evt.type = USBAudioEvent::Type::DRIVER_DISCONNECTED;
      break;
    default:
      return;
  }
  evt.driver.addr = addr;
  evt.driver.iface_num = iface_num;

  if (xQueueSend(self->event_queue_, &evt, 0) != pdTRUE) {
    ESP_LOGW(TAG, "USB audio event queue full, dropping driver event");
  }
}

void USBAudioComponent::device_event_stub(uac_host_device_handle_t handle, const uac_host_device_event_t event,
                                          void *user_data) {
  auto *self = static_cast<USBAudioComponent *>(user_data);
  if (self == nullptr || self->event_queue_ == nullptr) {
    return;
  }

  if (event != UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR) {
    return;
  }

  USBAudioEvent evt;
  evt.type = USBAudioEvent::Type::DEVICE_EVENT;
  evt.device.handle = handle;
  evt.device.event = event;

  if (xQueueSend(self->event_queue_, &evt, 0) != pdTRUE) {
    ESP_LOGW(TAG, "USB audio event queue full, dropping device event");
  }
}

void USBAudioComponent::usb_host_task_stub(void *param) {
  auto *self = static_cast<USBAudioComponent *>(param);
  if (self != nullptr) {
    self->usb_host_task_();
  }
  vTaskDelete(nullptr);
}

void USBAudioComponent::usb_host_task_() {
  while (this->host_task_running_) {
    uint32_t event_flags = 0;
    esp_err_t err = usb_host_lib_handle_events(HOST_EVENT_TIMEOUT_TICKS, &event_flags);
    if (err != ESP_OK) {
      if (err != ESP_ERR_TIMEOUT) {
        ESP_LOGW(TAG, "usb_host_lib_handle_events returned %s", esp_err_to_name(err));
      }
      continue;
    }

    if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
      ESP_LOGD(TAG, "USB host reports no registered clients");
    }
  }

  this->host_task_running_ = false;
  this->usb_host_task_handle_ = nullptr;
}

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)
