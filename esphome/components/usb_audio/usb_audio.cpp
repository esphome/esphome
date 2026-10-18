#include "usb_audio.h"

#ifdef USE_USB_AUDIO

#include <algorithm>
#include <cinttypes>
#include <cmath>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

extern "C" {
#include "esp_err.h"
}

namespace esphome::usb_audio {

ESPHOME_LOG_TAG(TAG, "usb_audio");

static constexpr UBaseType_t UAC_TASK_PRIORITY = 8;
static constexpr uint32_t UAC_TASK_STACK_SIZE = 4096;
// Volume and mute are control transfers that can block for seconds, so retries are spaced out and capped.
static constexpr uint32_t CONTROL_RETRY_INTERVAL_MS = 1000;
static constexpr uint8_t MAX_CONTROL_ATTEMPTS = 5;

static const LogString *endpoint_name(Endpoint endpoint) {
  return endpoint == Endpoint::SPEAKER ? LOG_STR("Speaker") : LOG_STR("Microphone");
}

static uint8_t discrete_rate_count(const uac_host_dev_alt_param_t &alt) {
  return std::min<uint8_t>(alt.sample_freq_type, UAC_FREQ_NUM_MAX);
}

static bool alt_supports_rate(const uac_host_dev_alt_param_t &alt, uint32_t sample_rate) {
  if (alt.sample_freq_type == 0) {
    return sample_rate >= alt.sample_freq_lower && sample_rate <= alt.sample_freq_upper;
  }
  const uint8_t count = discrete_rate_count(alt);
  return std::find(alt.sample_freq, alt.sample_freq + count, sample_rate) != alt.sample_freq + count;
}

void USBAudioComponent::setup() {
  this->event_queue_ = xQueueCreateStatic(EVENT_QUEUE_LENGTH, sizeof(USBAudioEvent), this->event_queue_storage_.data(),
                                          &this->event_queue_struct_);

  uac_host_driver_config_t uac_cfg = {
      .create_background_task = true,
      .task_priority = UAC_TASK_PRIORITY,
      .stack_size = UAC_TASK_STACK_SIZE,
      .core_id = 0,
      .callback = &USBAudioComponent::driver_event_stub,
      .callback_arg = this,
  };

  esp_err_t err = uac_host_install(&uac_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "uac_host_install failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  this->uac_installed_ = true;
  // The USB callbacks wake the loop when there is work.
  this->disable_loop();
}

void USBAudioComponent::loop() {
  USBAudioEvent event;
  while (xQueueReceive(this->event_queue_, &event, 0) == pdTRUE) {
    this->process_event_(event);
  }

  if (const uint32_t errors = this->transfer_errors_.exchange(0); errors != 0) {
    ESP_LOGW(TAG, "%" PRIu32 " USB transfer errors", errors);
  }

  if (this->controls_pending_()) {
    if (App.get_loop_component_start_time() - this->last_control_attempt_ms_ >= CONTROL_RETRY_INTERVAL_MS) {
      this->apply_controls_();
    }
    return;
  }
  this->disable_loop();
}

void USBAudioComponent::dump_config() {
  const auto &speaker = this->endpoint_(Endpoint::SPEAKER);
  const auto &microphone = this->endpoint_(Endpoint::MICROPHONE);
  ESP_LOGCONFIG(TAG,
                "USB Audio:\n"
                "  Device connected: %s\n"
                "  Speaker registered: %s\n"
                "  Microphone registered: %s",
                YESNO(speaker.handle != nullptr || microphone.handle != nullptr), YESNO(speaker.platform != nullptr),
                YESNO(microphone.platform != nullptr));
}

bool USBAudioComponent::teardown() {
  this->handle_disconnect_(this->endpoint_(Endpoint::SPEAKER).handle);
  this->handle_disconnect_(this->endpoint_(Endpoint::MICROPHONE).handle);
  xQueueReset(this->event_queue_);

  if (this->uac_installed_) {
    esp_err_t err = uac_host_uninstall();
    if (err == ESP_ERR_INVALID_STATE) {
      return false;
    }
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "uac_host_uninstall returned %s", esp_err_to_name(err));
    }
    this->uac_installed_ = false;
  }
  return true;
}

void USBAudioComponent::resume(Endpoint endpoint) {
  auto &ep = this->endpoint_(endpoint);
  ep.active = true;
  if (ep.handle == nullptr) {
    ESP_LOGD(TAG, "%s interface not connected yet", LOG_STR_ARG(endpoint_name(endpoint)));
    return;
  }
  if (!ep.stream_started) {
    this->start_stream_(endpoint);
    return;
  }
  esp_err_t err = uac_host_device_resume(ep.handle);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to resume %s stream: %s", LOG_STR_ARG(endpoint_name(endpoint)), esp_err_to_name(err));
    return;
  }
  ep.platform->on_stream_state(true);
}

void USBAudioComponent::suspend(Endpoint endpoint) {
  auto &ep = this->endpoint_(endpoint);
  ep.active = false;
  if (!ep.stream_started) {
    return;
  }
  esp_err_t err = uac_host_device_suspend(ep.handle);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to suspend %s stream: %s", LOG_STR_ARG(endpoint_name(endpoint)), esp_err_to_name(err));
  }
}

esp_err_t USBAudioComponent::read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
  *bytes_read = 0;
  uac_host_device_handle_t handle = this->endpoint_(Endpoint::MICROPHONE).handle;
  if (handle == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  uint32_t actual = 0;
  esp_err_t err = uac_host_device_read(handle, buffer, static_cast<uint32_t>(size), &actual, pdMS_TO_TICKS(timeout_ms));
  if (err == ESP_FAIL && actual == 0) {
    // The driver reports ESP_FAIL when the ring buffer has no data within the timeout window; treat like timeout.
    err = ESP_ERR_TIMEOUT;
  }
  *bytes_read = actual;
  return err;
}

esp_err_t USBAudioComponent::write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms) {
  uac_host_device_handle_t handle = this->endpoint_(Endpoint::SPEAKER).handle;
  if (handle == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_err_t err = uac_host_device_write(handle, const_cast<uint8_t *>(data), static_cast<uint32_t>(length),
                                        pdMS_TO_TICKS(timeout_ms));
  if (err == ESP_FAIL || err == ESP_ERR_INVALID_STATE) {
    // Ring buffer full or stream inactive; report as timeout so callers back off and retry.
    err = ESP_ERR_TIMEOUT;
  }
  return err;
}

void USBAudioComponent::process_event_(const USBAudioEvent &event) {
  switch (event.type) {
    case USBAudioEvent::Type::CONNECTED:
      this->open_stream_(event);
      break;
    case USBAudioEvent::Type::DISCONNECTED:
      this->handle_disconnect_(event.handle);
      break;
  }
}

void USBAudioComponent::open_stream_(const USBAudioEvent &event) {
  auto &ep = this->endpoint_(event.endpoint);
  const LogString *name = endpoint_name(event.endpoint);
  if (ep.platform == nullptr) {
    ESP_LOGD(TAG, "%s interface connected but not configured", LOG_STR_ARG(name));
    return;
  }
  if (ep.handle != nullptr) {
    ESP_LOGW(TAG, "%s interface already open, ignoring duplicate event", LOG_STR_ARG(name));
    return;
  }

  const uint32_t buffer_size = ep.platform->get_buffer_size();
  uac_host_device_config_t dev_cfg = {
      .addr = event.driver.addr,
      .iface_num = event.driver.iface_num,
      .buffer_size = buffer_size,
      .buffer_threshold = std::max<uint32_t>(buffer_size / 4, 1U),
      .callback = &USBAudioComponent::device_event_stub,
      .callback_arg = this,
  };

  esp_err_t err = uac_host_device_open(&dev_cfg, &ep.handle);
  if (err != ESP_OK) {
    ep.handle = nullptr;
    ESP_LOGE(TAG, "Failed to open %s interface: %s", LOG_STR_ARG(name), esp_err_to_name(err));
    return;
  }

  ep.iface = event.driver.iface_num;
  ESP_LOGI(TAG, "%s interface %u connected (addr=%u)", LOG_STR_ARG(name), ep.iface, event.driver.addr);
  if (ep.active) {
    this->start_stream_(event.endpoint);
  }
}

void USBAudioComponent::start_stream_(Endpoint endpoint) {
  auto &ep = this->endpoint_(endpoint);
  uac_host_stream_config_t stm_cfg{};
  if (!this->populate_stream_config_(endpoint, stm_cfg)) {
    return;
  }

  esp_err_t err = uac_host_device_start(ep.handle, &stm_cfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGW(TAG, "Failed to start %s stream: %s", LOG_STR_ARG(endpoint_name(endpoint)), esp_err_to_name(err));
    return;
  }

  ep.stream_started = true;
  if (endpoint == Endpoint::SPEAKER) {
    this->mark_controls_pending_();
  }
  ep.platform->on_stream_state(true);
}

bool USBAudioComponent::populate_stream_config_(Endpoint endpoint, uac_host_stream_config_t &stream_config) {
  const auto &ep = this->endpoint_(endpoint);
  const LogString *name = endpoint_name(endpoint);
  const audio::AudioStreamInfo info = ep.platform->get_usb_stream_info();
  const uint8_t channels = info.get_channels();
  const uint8_t bits = info.get_bits_per_sample();
  const uint32_t sample_rate = info.get_sample_rate();

  uac_host_dev_info_t dev_info{};
  esp_err_t err = uac_host_get_device_info(ep.handle, &dev_info);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to query %s interface info: %s", LOG_STR_ARG(name), esp_err_to_name(err));
    return false;
  }

  for (uint8_t alt = 1; alt <= dev_info.iface_alt_num; alt++) {
    uac_host_dev_alt_param_t alt_param{};
    if (uac_host_get_device_alt_param(ep.handle, alt, &alt_param) != ESP_OK) {
      continue;
    }
    if (alt_param.channels == channels && alt_param.bit_resolution == bits &&
        alt_supports_rate(alt_param, sample_rate)) {
      stream_config.channels = channels;
      stream_config.bit_resolution = bits;
      stream_config.sample_freq = sample_rate;
      stream_config.flags = 0;
      return true;
    }
  }

  ESP_LOGE(TAG, "%s interface %u does not support requested format (channels=%u bits=%u sample_rate=%" PRIu32 " Hz)",
           LOG_STR_ARG(name), dev_info.iface_num, channels, bits, sample_rate);
  for (uint8_t alt = 1; alt <= dev_info.iface_alt_num; alt++) {
    uac_host_dev_alt_param_t alt_param{};
    if (uac_host_get_device_alt_param(ep.handle, alt, &alt_param) != ESP_OK) {
      continue;
    }
    if (alt_param.sample_freq_type == 0) {
      ESP_LOGI(TAG, "  alt %u: channels=%u bits=%u sample rate range=%" PRIu32 "-%" PRIu32 " Hz", alt,
               alt_param.channels, alt_param.bit_resolution, alt_param.sample_freq_lower, alt_param.sample_freq_upper);
      continue;
    }
    ESP_LOGI(TAG, "  alt %u: channels=%u bits=%u sample rates:", alt, alt_param.channels, alt_param.bit_resolution);
    const uint8_t count = discrete_rate_count(alt_param);
    for (uint8_t i = 0; i < count; i++) {
      ESP_LOGI(TAG, "    %" PRIu32 " Hz", alt_param.sample_freq[i]);
    }
  }
  return false;
}

void USBAudioComponent::set_speaker_volume_level(float volume) {
  this->speaker_volume_level_ = std::clamp(volume, 0.0f, 1.0f);
  this->volume_.pending = this->volume_.supported;
  this->volume_.attempts = 0;
  this->apply_controls_();
}

void USBAudioComponent::set_speaker_mute_state(bool mute_state) {
  this->speaker_muted_ = mute_state;
  this->mute_.pending = this->mute_.supported;
  this->mute_.attempts = 0;
  this->apply_controls_();
}

void USBAudioComponent::mark_controls_pending_() {
  for (ControlState *control : {&this->volume_, &this->mute_}) {
    control->pending = control->supported;
    control->attempts = 0;
  }
  this->apply_controls_();
}

bool USBAudioComponent::controls_pending_() const {
  return this->endpoint_(Endpoint::SPEAKER).handle != nullptr && (this->volume_.pending || this->mute_.pending);
}

void USBAudioComponent::apply_controls_() {
  uac_host_device_handle_t handle = this->endpoint_(Endpoint::SPEAKER).handle;
  if (handle == nullptr) {
    return;
  }
  this->last_control_attempt_ms_ = App.get_loop_component_start_time();
  if (this->volume_.pending) {
    const auto volume = static_cast<uint8_t>(std::lround(this->speaker_volume_level_ * 100.0f));
    this->apply_control_(this->volume_, uac_host_device_set_volume(handle, volume), LOG_STR("volume"));
  }
  if (this->mute_.pending) {
    this->apply_control_(this->mute_, uac_host_device_set_mute(handle, this->speaker_muted_), LOG_STR("mute"));
  }
  if (this->controls_pending_()) {
    this->enable_loop();
  }
}

void USBAudioComponent::apply_control_(ControlState &control, esp_err_t err, const LogString *name) {
  if (err == ESP_OK) {
    control.pending = false;
    return;
  }
  if (err == ESP_ERR_NOT_SUPPORTED) {
    ESP_LOGW(TAG, "Speaker %s control not supported by device", LOG_STR_ARG(name));
    control.supported = false;
    control.pending = false;
    return;
  }
  if (++control.attempts >= MAX_CONTROL_ATTEMPTS) {
    ESP_LOGW(TAG, "Failed to set speaker %s: %s", LOG_STR_ARG(name), esp_err_to_name(err));
    control.pending = false;
  }
}

void USBAudioComponent::handle_disconnect_(uac_host_device_handle_t handle) {
  if (handle == nullptr) {
    return;
  }
  auto it = std::find_if(this->endpoints_.begin(), this->endpoints_.end(),
                         [handle](const EndpointState &ep) { return ep.handle == handle; });
  if (it == this->endpoints_.end()) {
    ESP_LOGD(TAG, "Disconnect event for an interface with no active stream");
    return;
  }
  const auto endpoint = static_cast<Endpoint>(it - this->endpoints_.begin());
  ESP_LOGI(TAG, "%s interface %u disconnected", LOG_STR_ARG(endpoint_name(endpoint)), it->iface);

  if (it->stream_started) {
    esp_err_t err = uac_host_device_stop(handle);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG, "Failed to stop %s stream: %s", LOG_STR_ARG(endpoint_name(endpoint)), esp_err_to_name(err));
    }
  }
  esp_err_t err = uac_host_device_close(handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uac_host_device_close failed: %s", esp_err_to_name(err));
  }

  it->handle = nullptr;
  it->stream_started = false;
  if (endpoint == Endpoint::SPEAKER) {
    // A reconnected device may support different controls.
    this->volume_ = ControlState{};
    this->mute_ = ControlState{};
  }
  it->platform->on_stream_state(false);
}

void USBAudioComponent::driver_event_stub(uint8_t addr, uint8_t iface_num, const uac_host_driver_event_t event,
                                          void *user_data) {
  auto *self = static_cast<USBAudioComponent *>(user_data);
  USBAudioEvent evt;
  switch (event) {
    case UAC_HOST_DRIVER_EVENT_TX_CONNECTED:
      evt.endpoint = Endpoint::SPEAKER;
      break;
    case UAC_HOST_DRIVER_EVENT_RX_CONNECTED:
      evt.endpoint = Endpoint::MICROPHONE;
      break;
    default:
      return;
  }
  evt.driver.addr = addr;
  evt.driver.iface_num = iface_num;

  if (xQueueSend(self->event_queue_, &evt, 0) != pdTRUE) {
    ESP_LOGW(TAG, "USB audio event queue full, dropping driver event");
  }
  self->enable_loop_soon_any_context();
}

void USBAudioComponent::device_event_stub(uac_host_device_handle_t handle, const uac_host_device_event_t event,
                                          void *user_data) {
  auto *self = static_cast<USBAudioComponent *>(user_data);
  if (event == UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR) {
    self->transfer_errors_.fetch_add(1, std::memory_order_relaxed);
  } else if (event == UAC_HOST_DRIVER_EVENT_DISCONNECTED) {
    USBAudioEvent evt;
    evt.type = USBAudioEvent::Type::DISCONNECTED;
    evt.handle = handle;
    if (xQueueSend(self->event_queue_, &evt, 0) != pdTRUE) {
      ESP_LOGW(TAG, "USB audio event queue full, dropping disconnect event");
    }
  } else {
    return;
  }
  self->enable_loop_soon_any_context();
}

}  // namespace esphome::usb_audio

#endif
