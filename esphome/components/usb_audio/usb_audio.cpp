#include "usb_audio.h"

#ifdef USE_USB_AUDIO

#include <algorithm>
#include <cinttypes>
#include <cmath>

#include "esphome/core/log.h"

extern "C" {
#include "esp_err.h"
}

namespace esphome::usb_audio {

ESPHOME_LOG_TAG(TAG, "usb_audio");

static constexpr UBaseType_t UAC_TASK_PRIORITY = 8;
static constexpr uint32_t UAC_TASK_STACK_SIZE = 4096;
// Below the driver's task, so its transfers are not held up by control requests.
static constexpr UBaseType_t WORKER_TASK_PRIORITY = 5;
static constexpr uint32_t WORKER_TASK_STACK_SIZE = 4096;
// The worker retries volume and mute up to this many times, this far apart.
static constexpr uint8_t MAX_CONTROL_ATTEMPTS = 5;
static constexpr uint32_t CONTROL_RETRY_MS = 100;

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
  this->op_queue_ =
      xQueueCreateStatic(OP_QUEUE_LENGTH, sizeof(Op), this->op_queue_storage_.data(), &this->op_queue_struct_);

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

  // Opening, starting, suspending and closing streams and setting volume are control transfers that can block for
  // seconds, so they run here instead of on the main loop.
  if (!this->worker_.create(&USBAudioComponent::worker_task, "usb_audio", WORKER_TASK_STACK_SIZE, this,
                            WORKER_TASK_PRIORITY, false)) {
    this->mark_failed(LOG_STR("Failed to create worker task"));
    return;
  }
  // The USB callbacks and the worker wake the loop when there is work.
  this->disable_loop();
}

void USBAudioComponent::loop() {
  this->process_events_();

  if (const uint32_t errors = this->transfer_errors_.exchange(0); errors != 0) {
    ESP_LOGW(TAG, "%" PRIu32 " USB transfer errors", errors);
  }

  // Stay awake while a lost device still waits to be closed.
  if (std::any_of(this->endpoints_.begin(), this->endpoints_.end(),
                  [](const EndpointState &ep) { return ep.closing != nullptr; })) {
    return;
  }
  this->disable_loop();
}

void USBAudioComponent::process_events_() {
  this->finish_closes_();

  USBAudioEvent event;
  // Only the events already queued: one posted again while this runs waits for the next loop().
  for (UBaseType_t queued = uxQueueMessagesWaiting(this->event_queue_);
       queued > 0 && xQueueReceive(this->event_queue_, &event, 0) == pdTRUE; queued--) {
    this->process_event_(event);
  }
}

void USBAudioComponent::dump_config() {
  const auto &speaker = this->endpoint_(Endpoint::SPEAKER);
  const auto &microphone = this->endpoint_(Endpoint::MICROPHONE);
  ESP_LOGCONFIG(TAG,
                "USB Audio:\n"
                "  Device connected: %s\n"
                "  Speaker registered: %s\n"
                "  Microphone registered: %s",
                YESNO(speaker.handle.load() != nullptr || microphone.handle.load() != nullptr),
                YESNO(speaker.platform != nullptr), YESNO(microphone.platform != nullptr));
}

bool USBAudioComponent::teardown() {
  if (this->event_queue_ == nullptr) {
    return true;
  }
  // Stop the platforms first, then let the worker close each device once its platform is idle.
  this->begin_close_(Endpoint::SPEAKER);
  this->begin_close_(Endpoint::MICROPHONE);
  this->process_events_();
  if (!this->finish_closes_()) {
    return false;
  }
  // Wait for the worker to finish whatever it was still running.
  if (uxQueueMessagesWaiting(this->op_queue_) != 0 || this->driver_busy_) {
    return false;
  }
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
  ep.request_seq++;
  const uac_host_device_handle_t handle = ep.handle;
  if (handle == nullptr) {
    ESP_LOGD(TAG, "%s interface not connected yet", LOG_STR_ARG(endpoint_name(endpoint)));
    return;
  }
  if (ep.start_pending) {
    // The pending start reports back to the platform, and checks `active` first.
    return;
  }
  if (!ep.stream_started) {
    if (ep.format_rejected) {
      // Already probed and logged for this device; a retrying caller must not flood the log.
      this->stream_failed_(endpoint);
      return;
    }
    this->start_stream_(endpoint);
    return;
  }
  // Queued even while an earlier resume or suspend is pending: the worker runs them in order, so the driver ends in
  // the latest requested state, and only this request's RESUMED reports back.
  Op op;
  op.type = Op::Type::RESUME;
  op.endpoint = endpoint;
  op.handle = handle;
  op.seq = ep.request_seq;
  if (!this->queue_op_(op)) {
    this->stream_failed_(endpoint);
  }
}

void USBAudioComponent::suspend(Endpoint endpoint) {
  auto &ep = this->endpoint_(endpoint);
  ep.active = false;
  ep.request_seq++;
  const uac_host_device_handle_t handle = ep.handle;
  // A start still running on the worker is suspended once its STARTED arrives.
  if (!ep.stream_started || handle == nullptr) {
    return;
  }
  Op op;
  op.type = Op::Type::SUSPEND;
  op.endpoint = endpoint;
  op.handle = handle;
  this->queue_op_(op);
}

esp_err_t USBAudioComponent::read_microphone(uint8_t *buffer, size_t size, size_t *bytes_read, uint32_t timeout_ms) {
  *bytes_read = 0;
  // Counted before the busy check, so the worker waits for this read to leave the driver before it takes it.
  this->driver_users_.fetch_add(1);
  if (this->driver_busy_) {
    this->driver_users_.fetch_sub(1);
    vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    return ESP_ERR_TIMEOUT;
  }
  uac_host_device_handle_t handle = this->endpoint_(Endpoint::MICROPHONE).handle;
  if (handle == nullptr) {
    this->driver_users_.fetch_sub(1);
    return ESP_ERR_INVALID_STATE;
  }
  // The driver collects up to `size` bytes for the whole timeout and returns what it got, or ESP_ERR_TIMEOUT when
  // nothing arrived.
  uint32_t actual = 0;
  esp_err_t err = uac_host_device_read(handle, buffer, static_cast<uint32_t>(size), &actual, pdMS_TO_TICKS(timeout_ms));
  this->driver_users_.fetch_sub(1);
  *bytes_read = actual;
  return err;
}

esp_err_t USBAudioComponent::write_speaker(const uint8_t *data, size_t length, uint32_t timeout_ms) {
  // Counted before the busy check, so the worker waits for this write to leave the driver before it takes it.
  this->driver_users_.fetch_add(1);
  if (this->driver_busy_) {
    this->driver_users_.fetch_sub(1);
    // Back-pressure: a caller with a budget waits it out, a non-blocking caller retries later.
    if (timeout_ms > 0) {
      vTaskDelay(pdMS_TO_TICKS(timeout_ms));
    }
    return ESP_ERR_TIMEOUT;
  }
  uac_host_device_handle_t handle = this->endpoint_(Endpoint::SPEAKER).handle;
  if (handle == nullptr) {
    this->driver_users_.fetch_sub(1);
    return ESP_ERR_INVALID_STATE;
  }
  esp_err_t err = uac_host_device_write(handle, const_cast<uint8_t *>(data), static_cast<uint32_t>(length),
                                        pdMS_TO_TICKS(timeout_ms));
  this->driver_users_.fetch_sub(1);
  if (err == ESP_FAIL || err == ESP_ERR_INVALID_STATE) {
    // Ring buffer full or stream inactive; report as timeout so callers back off and retry.
    err = ESP_ERR_TIMEOUT;
  }
  return err;
}

void USBAudioComponent::process_event_(const USBAudioEvent &event) {
  switch (event.type) {
    case USBAudioEvent::Type::CONNECTED: {
      auto &ep = this->endpoint_(event.endpoint);
      if (ep.closing != nullptr) {
        // A device that came back waits until its old handle is closed; the queue keeps draining meanwhile, so the
        // worker's CLOSED can arrive.
        ep.connect_deferred = true;
        ep.deferred_addr = event.addr;
        ep.deferred_iface = event.iface_num;
        break;
      }
      this->open_stream_(event.endpoint, event.addr, event.iface_num);
      break;
    }
    case USBAudioEvent::Type::DISCONNECTED:
      this->handle_disconnect_(event.handle);
      break;
    case USBAudioEvent::Type::OPENED:
      this->on_opened_(event);
      break;
    case USBAudioEvent::Type::STARTED:
      this->on_started_(event);
      break;
    case USBAudioEvent::Type::RESUMED:
      this->on_resumed_(event);
      break;
    case USBAudioEvent::Type::CLOSED:
      this->on_closed_(event);
      break;
  }
}

void USBAudioComponent::open_stream_(Endpoint endpoint, uint8_t addr, uint8_t iface_num) {
  auto &ep = this->endpoint_(endpoint);
  const LogString *name = endpoint_name(endpoint);
  if (ep.platform == nullptr) {
    ESP_LOGD(TAG, "%s interface connected but not configured", LOG_STR_ARG(name));
    return;
  }
  if (ep.handle.load() != nullptr || ep.open_pending) {
    ESP_LOGW(TAG, "%s interface already open, ignoring duplicate event", LOG_STR_ARG(name));
    return;
  }
  Op op;
  op.type = Op::Type::OPEN;
  op.endpoint = endpoint;
  op.addr = addr;
  op.iface_num = iface_num;
  if (!this->queue_op_(op)) {
    // The event is already off the queue, so post it again for a later loop() instead of losing the device.
    USBAudioEvent event;
    event.type = USBAudioEvent::Type::CONNECTED;
    event.endpoint = endpoint;
    event.addr = addr;
    event.iface_num = iface_num;
    this->post_event_(event);
    return;
  }
  ep.open_pending = true;
}

void USBAudioComponent::on_opened_(const USBAudioEvent &event) {
  auto &ep = this->endpoint_(event.endpoint);
  ep.open_pending = false;
  if (!event.ok) {
    return;
  }
  ep.handle = event.handle;
  ep.iface = event.iface_num;
  ESP_LOGI(TAG, "%s interface %u connected (addr=%u)", LOG_STR_ARG(endpoint_name(event.endpoint)), ep.iface,
           event.addr);
  for (auto &lost : this->lost_while_opening_) {
    if (lost == event.handle) {
      // It went away while the worker was opening it.
      lost = nullptr;
      this->handle_disconnect_(event.handle);
      return;
    }
  }
  if (ep.active) {
    this->start_stream_(event.endpoint);
  }
}

void USBAudioComponent::start_stream_(Endpoint endpoint) {
  auto &ep = this->endpoint_(endpoint);
  Op op;
  op.type = Op::Type::START;
  op.endpoint = endpoint;
  op.handle = ep.handle;
  if (!this->queue_op_(op)) {
    this->stream_failed_(endpoint);
    return;
  }
  ep.start_pending = true;
}

void USBAudioComponent::on_started_(const USBAudioEvent &event) {
  auto &ep = this->endpoint_(event.endpoint);
  ep.start_pending = false;
  if (event.handle != ep.handle.load()) {
    // The device went away meanwhile; the platform was already told.
    return;
  }
  if (!event.ok) {
    ep.format_rejected = event.format_rejected;
    if (ep.active) {
      this->stream_failed_(event.endpoint);
    }
    return;
  }
  ep.stream_started = true;
  if (event.endpoint == Endpoint::SPEAKER) {
    // Send the current volume and mute to the newly started stream.
    this->volume_.pending = true;
    this->mute_.pending = true;
    this->queue_controls_();
  }
  if (!ep.active) {
    // The platform stopped while the stream was starting.
    this->suspend(event.endpoint);
    return;
  }
  ep.platform->on_stream_state(StreamEvent::RUNNING);
}

void USBAudioComponent::on_resumed_(const USBAudioEvent &event) {
  auto &ep = this->endpoint_(event.endpoint);
  if (event.handle != ep.handle.load() || event.seq != ep.request_seq || !ep.active) {
    // The device went away, or a later suspend or resume is queued behind this one and decides the state.
    return;
  }
  if (!event.ok) {
    this->stream_failed_(event.endpoint);
    return;
  }
  ep.platform->on_stream_state(StreamEvent::RUNNING);
}

void USBAudioComponent::stream_failed_(Endpoint endpoint) {
  auto &ep = this->endpoint_(endpoint);
  // The platform goes back to stopped, so a reconnect must not restart the stream; its next start() retries.
  ep.active = false;
  ep.platform->on_stream_state(StreamEvent::FAILED);
}

void USBAudioComponent::set_speaker_volume_level(float volume) {
  this->speaker_volume_ = static_cast<uint8_t>(std::lround(std::clamp(volume, 0.0f, 1.0f) * 100.0f));
  this->volume_.pending = true;
  this->queue_controls_();
}

void USBAudioComponent::set_speaker_mute_state(bool mute_state) {
  this->speaker_muted_ = mute_state;
  this->mute_.pending = true;
  this->queue_controls_();
}

void USBAudioComponent::queue_controls_() {
  if (this->op_queue_ == nullptr) {
    return;
  }
  if (this->controls_queued_.exchange(true)) {
    // The queued op sends the latest values, so lifecycle ops keep their queue slots.
    return;
  }
  Op op;
  op.type = Op::Type::CONTROLS;
  if (xQueueSend(this->op_queue_, &op, 0) != pdTRUE) {
    // The controls stay pending and the next start or change queues them again.
    this->controls_queued_ = false;
  }
}

void USBAudioComponent::handle_disconnect_(uac_host_device_handle_t handle) {
  for (size_t i = 0; i < this->endpoints_.size(); i++) {
    auto &ep = this->endpoints_[i];
    if (handle != nullptr && ep.handle.load() == handle) {
      const auto endpoint = static_cast<Endpoint>(i);
      ESP_LOGI(TAG, "%s interface %u disconnected", LOG_STR_ARG(endpoint_name(endpoint)), ep.iface);
      this->begin_close_(endpoint);
      return;
    }
  }
  if (std::any_of(this->endpoints_.begin(), this->endpoints_.end(),
                  [](const EndpointState &ep) { return ep.open_pending; })) {
    // Its open is still running on the worker; close it once it reports back.
    for (auto &lost : this->lost_while_opening_) {
      if (lost == nullptr || lost == handle) {
        lost = handle;
        return;
      }
    }
    // Both endpoints are already waiting on a lost open; the oldest one is the least likely to still report back.
    this->lost_while_opening_[0] = handle;
    return;
  }
  ESP_LOGD(TAG, "Disconnect event for an interface with no active stream");
}

void USBAudioComponent::begin_close_(Endpoint endpoint) {
  auto &ep = this->endpoint_(endpoint);
  // Platform reads and writes see nullptr from here on and return at once.
  const uac_host_device_handle_t handle = ep.handle.exchange(nullptr);
  if (handle == nullptr) {
    return;
  }
  ep.closing = handle;
  ep.close_queued = false;
  // `active` is kept, so the stream restarts when the device returns.
  ep.platform->on_stream_state(StreamEvent::LOST);
}

bool USBAudioComponent::finish_closes_() {
  bool done = true;
  for (size_t i = 0; i < this->endpoints_.size(); i++) {
    auto &ep = this->endpoints_[i];
    if (ep.closing == nullptr) {
      continue;
    }
    done = false;
    // Closing frees the driver's interface, so wait until no platform task can still be using the handle.
    if (ep.close_queued || !ep.platform->stream_idle()) {
      continue;
    }
    Op op;
    op.type = Op::Type::CLOSE;
    op.endpoint = static_cast<Endpoint>(i);
    op.handle = ep.closing;
    op.stop = ep.stream_started;
    ep.close_queued = this->queue_op_(op);
  }
  return done;
}

void USBAudioComponent::on_closed_(const USBAudioEvent &event) {
  auto &ep = this->endpoint_(event.endpoint);
  ep.closing = nullptr;
  ep.close_queued = false;
  ep.stream_started = false;
  ep.format_rejected = false;
  ep.start_pending = false;
  if (ep.connect_deferred) {
    // The device came back while its old handle was closing.
    ep.connect_deferred = false;
    this->open_stream_(event.endpoint, ep.deferred_addr, ep.deferred_iface);
  }
}

bool USBAudioComponent::queue_op_(const Op &op) {
  if (xQueueSend(this->op_queue_, &op, 0) != pdTRUE) {
    ESP_LOGW(TAG, "USB audio work queue full, dropping %s request", LOG_STR_ARG(endpoint_name(op.endpoint)));
    return false;
  }
  return true;
}

void USBAudioComponent::post_event_(const USBAudioEvent &event) {
  if (xQueueSend(this->event_queue_, &event, 0) != pdTRUE) {
    ESP_LOGW(TAG, "USB audio event queue full, dropping event");
  }
  this->enable_loop_soon_any_context();
}

void USBAudioComponent::worker_task(void *param) {
  auto *self = static_cast<USBAudioComponent *>(param);
  Op op;
  while (true) {
    if (xQueueReceive(self->op_queue_, &op, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    self->lock_driver_();
    self->run_op_(op);
    self->unlock_driver_();
  }
}

void USBAudioComponent::lock_driver_() {
  this->driver_busy_ = true;
  // Reads and writes give up within their timeout; wait for those already inside the driver.
  while (this->driver_users_ != 0) {
    vTaskDelay(1);
  }
}

void USBAudioComponent::unlock_driver_() { this->driver_busy_ = false; }

void USBAudioComponent::run_op_(const Op &op) {
  const LogString *name = endpoint_name(op.endpoint);
  USBAudioEvent result;
  result.endpoint = op.endpoint;
  result.handle = op.handle;

  switch (op.type) {
    case Op::Type::OPEN: {
      const uint32_t buffer_size = this->endpoint_(op.endpoint).platform->get_buffer_size();
      uac_host_device_config_t dev_cfg = {
          .addr = op.addr,
          .iface_num = op.iface_num,
          .buffer_size = buffer_size,
          .buffer_threshold = std::max<uint32_t>(buffer_size / 4, 1U),
          .callback = &USBAudioComponent::device_event_stub,
          .callback_arg = this,
      };
      uac_host_device_handle_t handle{nullptr};
      esp_err_t err = uac_host_device_open(&dev_cfg, &handle);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open %s interface: %s", LOG_STR_ARG(name), esp_err_to_name(err));
      }
      result.type = USBAudioEvent::Type::OPENED;
      result.ok = err == ESP_OK;
      result.handle = handle;
      result.addr = op.addr;
      result.iface_num = op.iface_num;
      this->post_event_(result);
      return;
    }
    case Op::Type::START: {
      result.type = USBAudioEvent::Type::STARTED;
      uac_host_stream_config_t stm_cfg{};
      if (this->populate_stream_config_(op.endpoint, op.handle, stm_cfg, result.format_rejected)) {
        esp_err_t err = uac_host_device_start(op.handle, &stm_cfg);
        result.ok = err == ESP_OK || err == ESP_ERR_INVALID_STATE;
        if (!result.ok) {
          ESP_LOGW(TAG, "Failed to start %s stream: %s", LOG_STR_ARG(name), esp_err_to_name(err));
        }
      }
      this->post_event_(result);
      return;
    }
    case Op::Type::RESUME: {
      esp_err_t err = uac_host_device_resume(op.handle);
      result.type = USBAudioEvent::Type::RESUMED;
      result.seq = op.seq;
      result.ok = err == ESP_OK || err == ESP_ERR_INVALID_STATE;
      if (!result.ok) {
        ESP_LOGW(TAG, "Failed to resume %s stream: %s", LOG_STR_ARG(name), esp_err_to_name(err));
      }
      this->post_event_(result);
      return;
    }
    case Op::Type::SUSPEND: {
      esp_err_t err = uac_host_device_suspend(op.handle);
      if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Failed to suspend %s stream: %s", LOG_STR_ARG(name), esp_err_to_name(err));
      }
      return;
    }
    case Op::Type::CLOSE: {
      if (op.stop) {
        esp_err_t err = uac_host_device_stop(op.handle);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
          ESP_LOGW(TAG, "Failed to stop %s stream: %s", LOG_STR_ARG(name), esp_err_to_name(err));
        }
      }
      esp_err_t err = uac_host_device_close(op.handle);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "uac_host_device_close failed: %s", esp_err_to_name(err));
      }
      if (op.endpoint == Endpoint::SPEAKER) {
        // A reconnected device may support different controls.
        for (ControlState *control : {&this->volume_, &this->mute_}) {
          control->supported = true;
          control->attempts = 0;
        }
      }
      result.type = USBAudioEvent::Type::CLOSED;
      this->post_event_(result);
      return;
    }
    case Op::Type::CONTROLS: {
      // Cleared first, so a change made during the transfer queues another op.
      this->controls_queued_ = false;
      if (this->apply_pending_speaker_controls_()) {
        // Give the device a moment before retrying; the queue keeps any other work in order.
        this->unlock_driver_();
        vTaskDelay(pdMS_TO_TICKS(CONTROL_RETRY_MS));
        this->lock_driver_();
        this->queue_controls_();
      }
      return;
    }
  }
}

bool USBAudioComponent::populate_stream_config_(Endpoint endpoint, uac_host_device_handle_t handle,
                                                uac_host_stream_config_t &stream_config, bool &format_rejected) {
  const LogString *name = endpoint_name(endpoint);
  const audio::AudioStreamInfo info = this->endpoint_(endpoint).platform->get_usb_stream_info();
  const uint8_t channels = info.get_channels();
  const uint8_t bits = info.get_bits_per_sample();
  const uint32_t sample_rate = info.get_sample_rate();

  uac_host_dev_info_t dev_info{};
  esp_err_t err = uac_host_get_device_info(handle, &dev_info);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to query %s interface info: %s", LOG_STR_ARG(name), esp_err_to_name(err));
    return false;
  }

  // uac_host_device_start() picks the alternate setting itself from channels, bit depth and rate: the last match with
  // discrete rates, or the first match with a rate range. Find the one it will pick, so its subframe size is checked.
  uint8_t selected_alt = 0;
  uint8_t selected_subframe = 0;
  for (uint8_t alt = 1; alt <= dev_info.iface_alt_num; alt++) {
    uac_host_dev_alt_param_t alt_param{};
    if (uac_host_get_device_alt_param(handle, alt, &alt_param) != ESP_OK || alt_param.channels != channels ||
        alt_param.bit_resolution != bits || !alt_supports_rate(alt_param, sample_rate)) {
      continue;
    }
    selected_alt = alt;
    selected_subframe = alt_param.subframe_size;
    if (alt_param.sample_freq_type == 0) {
      break;
    }
  }

  // ESPHome's samples are packed, so a device that pads them (e.g. 24-bit samples in 4-byte subframes) cannot be
  // streamed unchanged.
  if (selected_alt != 0 && selected_subframe * 8 == bits) {
    stream_config.channels = channels;
    stream_config.bit_resolution = bits;
    stream_config.sample_freq = sample_rate;
    stream_config.flags = 0;
    return true;
  }

  format_rejected = true;
  if (selected_alt != 0) {
    ESP_LOGW(TAG, "%s interface %u would stream alt %u with %u-bit samples padded to %u bytes, which is not supported",
             LOG_STR_ARG(name), dev_info.iface_num, selected_alt, bits, selected_subframe);
  }
  ESP_LOGW(TAG, "%s interface %u does not support requested format (channels=%u bits=%u sample_rate=%" PRIu32 " Hz)",
           LOG_STR_ARG(name), dev_info.iface_num, channels, bits, sample_rate);
  for (uint8_t alt = 1; alt <= dev_info.iface_alt_num; alt++) {
    uac_host_dev_alt_param_t alt_param{};
    if (uac_host_get_device_alt_param(handle, alt, &alt_param) != ESP_OK) {
      continue;
    }
    if (alt_param.sample_freq_type == 0) {
      ESP_LOGI(TAG, "  alt %u: channels=%u bits=%u subframe=%u bytes sample rate range=%" PRIu32 "-%" PRIu32 " Hz", alt,
               alt_param.channels, alt_param.bit_resolution, alt_param.subframe_size, alt_param.sample_freq_lower,
               alt_param.sample_freq_upper);
      continue;
    }
    ESP_LOGI(TAG, "  alt %u: channels=%u bits=%u subframe=%u bytes sample rates:", alt, alt_param.channels,
             alt_param.bit_resolution, alt_param.subframe_size);
    const uint8_t count = discrete_rate_count(alt_param);
    for (uint8_t i = 0; i < count; i++) {
      ESP_LOGI(TAG, "    %" PRIu32 " Hz", alt_param.sample_freq[i]);
    }
  }
  return false;
}

bool USBAudioComponent::apply_pending_speaker_controls_() {
  const uac_host_device_handle_t handle = this->endpoint_(Endpoint::SPEAKER).handle;
  if (handle == nullptr) {
    // Sent once the stream starts.
    return false;
  }
  bool retry = false;
  if (this->volume_.pending.exchange(false) && this->volume_.supported) {
    retry |= this->apply_control_(this->volume_, uac_host_device_set_volume(handle, this->speaker_volume_),
                                  LOG_STR("volume"));
  }
  if (this->mute_.pending.exchange(false) && this->mute_.supported) {
    retry |= this->apply_control_(this->mute_, uac_host_device_set_mute(handle, this->speaker_muted_), LOG_STR("mute"));
  }
  return retry;
}

bool USBAudioComponent::apply_control_(ControlState &control, esp_err_t err, const LogString *name) {
  if (err == ESP_OK) {
    control.attempts = 0;
    return false;
  }
  if (err == ESP_ERR_NOT_SUPPORTED) {
    ESP_LOGW(TAG, "Speaker %s control not supported by device", LOG_STR_ARG(name));
    control.supported = false;
    return false;
  }
  if (err == ESP_ERR_INVALID_STATE) {
    // The stream is not started yet; starting it queues the controls again.
    control.pending = true;
    return false;
  }
  if (++control.attempts >= MAX_CONTROL_ATTEMPTS) {
    ESP_LOGW(TAG, "Failed to set speaker %s: %s", LOG_STR_ARG(name), esp_err_to_name(err));
    control.attempts = 0;
    return false;
  }
  control.pending = true;
  return true;
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
  evt.type = USBAudioEvent::Type::CONNECTED;
  evt.addr = addr;
  evt.iface_num = iface_num;
  self->post_event_(evt);
}

void USBAudioComponent::device_event_stub(uac_host_device_handle_t handle, const uac_host_device_event_t event,
                                          void *user_data) {
  auto *self = static_cast<USBAudioComponent *>(user_data);
  if (event == UAC_HOST_DEVICE_EVENT_TRANSFER_ERROR) {
    self->transfer_errors_.fetch_add(1, std::memory_order_relaxed);
    self->enable_loop_soon_any_context();
  } else if (event == UAC_HOST_DRIVER_EVENT_DISCONNECTED) {
    USBAudioEvent evt;
    evt.type = USBAudioEvent::Type::DISCONNECTED;
    evt.handle = handle;
    self->post_event_(evt);
  }
}

}  // namespace esphome::usb_audio

#endif
