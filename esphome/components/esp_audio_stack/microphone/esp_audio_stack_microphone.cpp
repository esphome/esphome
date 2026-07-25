#include "esp_audio_stack_microphone.h"

#ifdef USE_ESP32

#include <cinttypes>
#include <cstring>

#include "esphome/core/log.h"
#include "../audio_core_log_utils.h"

namespace esphome::esp_audio_stack {

static const char *const TAG = "audio_stack.mic";

void ESPAudioStackMicrophone::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP Audio Stack Microphone...");

  // Counting semaphore for listener refcounting (take to decrement, give
  // to increment). Initial count == max count == MAX_LISTENERS.
  this->active_listeners_semaphore_ = xSemaphoreCreateCounting(MAX_LISTENERS, MAX_LISTENERS);
  if (this->active_listeners_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create semaphore");
    this->mark_failed();
    return;
  }

  // Configure audio stream info for 16-bit mono PCM at output rate
  // When rate conversion is active, mic delivers data at output_sample_rate (e.g. 16 kHz).
  // AudioStreamInfo constructor: (bits_per_sample, channels, sample_rate)
  this->audio_stream_info_ = audio::AudioStreamInfo(16, 1, this->parent_->get_output_sample_rate());

  // Pre-allocate callback buffer to avoid heap allocation in the RT audio task.
  // Do not grow this vector in on_audio_data_(): that callback runs in the
  // parent audio task.
  const size_t callback_buffer_bytes = this->parent_->get_mic_callback_buffer_size();
  this->audio_buffer_.reserve(callback_buffer_bytes);
  ESP_LOGCONFIG(TAG, "  Callback Buffer: %u bytes", (unsigned) callback_buffer_bytes);

  // Standard microphone output is always post-processor. MWW, VA and
  // call components all consume the same cleaned stream.
  if (!this->parent_->add_mic_data_callback(ESPAudioStackMicrophone::mic_data_callback, this)) {
    ESP_LOGE(TAG, "Failed to register parent mic callback");
    this->mark_failed();
  }
}

void ESPAudioStackMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP Audio Stack Microphone:");
  ESP_LOGCONFIG(TAG, "  Sample Rate: %" PRIu32 " Hz", this->parent_->get_output_sample_rate());
  ESP_LOGCONFIG(TAG, "  Bits Per Sample: 16");
  ESP_LOGCONFIG(TAG, "  Channels: 1 (mono)");
}

void ESPAudioStackMicrophone::start() {
  if (this->is_failed())
    return;

  // Take semaphore to register as active listener
  if (xSemaphoreTake(this->active_listeners_semaphore_, 0) != pdTRUE) {
    ESP_LOGW(TAG, "No free semaphore slots");
    return;
  }

  // Native ESPHome microphone semantics are asynchronous: start() only
  // registers interest; loop() performs the state transition.
  this->enable_loop_soon_any_context();
}

void ESPAudioStackMicrophone::stop() {
  if (this->is_failed() || this->active_listeners_semaphore_ == nullptr)
    return;

  if (uxSemaphoreGetCount(this->active_listeners_semaphore_) >= MAX_LISTENERS)
    return;

  // Give semaphore to unregister as listener. Do this even if loop() has not
  // transitioned to STATE_RUNNING yet, otherwise start()+immediate stop() leaks
  // a listener slot and keeps the parent mic path alive.
  xSemaphoreGive(this->active_listeners_semaphore_);

  if (uxSemaphoreGetCount(this->active_listeners_semaphore_) < MAX_LISTENERS)
    return;

  if (this->state_ == microphone::STATE_STOPPED) {
    return;
  }

  // loop() will process the stop edge; do not block the ESPHome main loop.
  this->enable_loop_soon_any_context();
}

void ESPAudioStackMicrophone::mic_data_callback(void *ctx, const uint8_t *data, size_t len) {
  static_cast<ESPAudioStackMicrophone *>(ctx)->on_audio_data_(data, len);
}

void ESPAudioStackMicrophone::on_audio_data_(const uint8_t *data, size_t len) {
  if (this->state_ != microphone::STATE_RUNNING) {
    return;
  }

  if (len > this->audio_buffer_.capacity()) {
    LOG_W_THROTTLED("Mic callback frame too large: %u > %u bytes; dropping", (unsigned) len,
                    (unsigned) this->audio_buffer_.capacity());
    return;
  }
  this->audio_buffer_.resize(len);
  const bool muted = this->mute_state_;
  if (muted) {
    std::memset(this->audio_buffer_.data(), 0, len);
  } else {
    std::memcpy(this->audio_buffer_.data(), data, len);
  }
  // ESPHome's base Microphone wrapper allocates a temporary zero vector when
  // mute_state_ is true. We have already zero-filled the preallocated callback
  // buffer, so clear the flag only while dispatching to avoid RT-task heap churn.
  if (muted)
    this->mute_state_ = false;
  this->data_callbacks_.call(this->audio_buffer_);
  if (muted)
    this->mute_state_ = true;
}

void ESPAudioStackMicrophone::loop() {
  // Propagate I2S errors from parent audio task
  if (this->parent_->has_i2s_error() && !this->status_has_error()) {
    ESP_LOGE(TAG, "I2S error detected in audio task");
    this->status_set_error(LOG_STR("I2S read error in audio task"));
  }

  // Check semaphore count to decide when to start/stop
  UBaseType_t count = uxSemaphoreGetCount(this->active_listeners_semaphore_);

  // Start the microphone if any semaphores are taken (listeners active)
  if ((count < MAX_LISTENERS) && (this->state_ == microphone::STATE_STOPPED)) {
    this->state_ = microphone::STATE_STARTING;
  }

  // Stop the microphone if all semaphores are returned (no listeners)
  if ((count == MAX_LISTENERS) && (this->state_ == microphone::STATE_RUNNING)) {
    this->state_ = microphone::STATE_STOPPING;
  }

  switch (this->state_) {
    case microphone::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }
      if (uxSemaphoreGetCount(this->active_listeners_semaphore_) >= MAX_LISTENERS) {
        this->state_ = microphone::STATE_STOPPED;
        break;
      }
      ESP_LOGI(TAG, "Microphone started");
      if (!this->parent_->register_mic_consumer(this)) {
        ESP_LOGW(TAG, "Parent audio stack refused mic consumer registration");
        xSemaphoreGive(this->active_listeners_semaphore_);
        this->state_ = microphone::STATE_STOPPED;
        break;
      }
      if (!this->parent_->is_running()) {
        ESP_LOGW(TAG, "Parent audio stack failed to start; aborting microphone start");
        this->parent_->unregister_mic_consumer(this);
        xSemaphoreGive(this->active_listeners_semaphore_);
        this->state_ = microphone::STATE_STOPPED;
        break;
      }
      this->state_ = microphone::STATE_RUNNING;
      break;

    case microphone::STATE_RUNNING:
      break;

    case microphone::STATE_STOPPING:
      ESP_LOGI(TAG, "Microphone stopped");
      this->parent_->unregister_mic_consumer(this);
      this->state_ = microphone::STATE_STOPPED;
      break;

    case microphone::STATE_STOPPED:
      this->disable_loop();
      break;
  }
}

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
