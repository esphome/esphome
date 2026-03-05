#include "http_request_media_source.h"

#ifdef USE_ESP32

#include "http_request_media_source_internal.h"

#include "esphome/core/log.h"

namespace esphome::http_request {

void HTTPRequestMediaSource::dump_config() {
  ESP_LOGCONFIG(TAG, "HTTP Media Source:");
  ESP_LOGCONFIG(TAG, "  Buffer Size: %zu bytes", this->buffer_size_);
  ESP_LOGCONFIG(TAG, "  Task Stack in PSRAM: %s", this->task_stack_in_psram_ ? "Yes" : "No");
}

void HTTPRequestMediaSource::setup() {
  this->disable_loop();

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }
}

void HTTPRequestMediaSource::loop() {
  EventBits_t event_bits = xEventGroupGetBits(this->event_group_);

  if (event_bits & REQUEST_START) {
    xEventGroupClearBits(this->event_group_, REQUEST_START);
    this->decoding_state_ = HTTPDecodingState::START_TASKS;
  }

  switch (this->decoding_state_) {
    case HTTPDecodingState::START_TASKS: {
      if (!this->read_task_.is_created() && !this->decode_task_.is_created()) {
        xEventGroupClearBits(this->event_group_, ALL_BITS);

        // Reader task uses HttpContainer which uses esp_http_client. This crashes on IDF 5.4 if the task
        // stack is in PSRAM. As a workaround, always allocate the read task in internal memory.
        if (!this->read_task_.create(read_task, "HTTPRead", READ_TASK_STACK_SIZE, this, 1, false)) {
          ESP_LOGE(TAG, "Failed to create task");
          this->status_momentary_error("task_create", 1000);
          this->set_state_(media_source::MediaSourceState::ERROR);
          this->decoding_state_ = HTTPDecodingState::IDLE;
          return;
        }

        if (!this->decode_task_.create(decode_task, "HTTPDecode", DECODE_TASK_STACK_SIZE, this, 1,
                                       this->task_stack_in_psram_)) {
          ESP_LOGE(TAG, "Failed to create task");
          // Signal read task to stop and let the DECODING state handle cleanup.
          // Set DECODER_STOPPED + DECODER_RINGBUF_ACQUIRED since no decode task exists to set them.
          xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_STOP | EventGroupBits::DECODER_STOPPED |
                                                     EventGroupBits::DECODER_RINGBUF_ACQUIRED);
          this->status_momentary_error("task_create", 1000);
          this->set_state_(media_source::MediaSourceState::ERROR);
          this->decoding_state_ = HTTPDecodingState::DECODING;
          return;
        }
      }
      this->decoding_state_ = HTTPDecodingState::DECODING;
      break;
    }
    case HTTPDecodingState::DECODING: {
      if (event_bits & DECODER_STARTING) {
        ESP_LOGD(TAG, "Starting");
        xEventGroupClearBits(this->event_group_, DECODER_STARTING);
      }

      if (event_bits & DECODER_RUNNING) {
        ESP_LOGV(TAG, "Started");
        xEventGroupClearBits(this->event_group_, DECODER_RUNNING);
        this->set_state_(media_source::MediaSourceState::PLAYING);
      }

      if ((event_bits & DECODER_PAUSED) && this->get_state() != media_source::MediaSourceState::PAUSED) {
        this->set_state_(media_source::MediaSourceState::PAUSED);
      } else if (!(event_bits & DECODER_PAUSED) && this->get_state() == media_source::MediaSourceState::PAUSED) {
        this->set_state_(media_source::MediaSourceState::PLAYING);
      }

      if (event_bits & (READER_ERROR | DECODER_ERROR)) {
        // Report error so the orchestrator knows playback failed; task will have already logged the specific error
        this->set_state_(media_source::MediaSourceState::ERROR);
      }

      if (event_bits & DECODER_STOPPING) {
        ESP_LOGV(TAG, "Stopping");
        xEventGroupClearBits(this->event_group_, DECODER_STOPPING);
      }

      // Check if both tasks have stopped
      if ((event_bits & READER_STOPPED) && (event_bits & DECODER_STOPPED)) {
        ESP_LOGD(TAG, "Stopped");
        xEventGroupClearBits(this->event_group_, ALL_BITS);

        this->read_task_.destroy();
        this->decode_task_.destroy();
        this->set_state_(media_source::MediaSourceState::IDLE);
        this->decoding_state_ = HTTPDecodingState::IDLE;
      }
      break;
    }
    case HTTPDecodingState::IDLE: {
      if (this->get_state() == media_source::MediaSourceState::ERROR && !this->status_has_error()) {
        this->set_state_(media_source::MediaSourceState::IDLE);
      }
      break;
    }
  }

  if ((this->decoding_state_ == HTTPDecodingState::IDLE) &&
      (this->get_state() == media_source::MediaSourceState::IDLE)) {
    this->disable_loop();
  }
}

// Called from the orchestrator's main loop, so no synchronization needed with loop()
bool HTTPRequestMediaSource::play_uri(const std::string &uri) {
  if (!this->is_ready() || this->is_failed() || this->status_has_error() || !this->has_listener() ||
      xEventGroupGetBits(this->event_group_) & REQUEST_START) {
    return false;
  }

  if (this->get_state() != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s': source is busy", uri.c_str());
    return false;
  }

  if (!uri.starts_with("http://") && !uri.starts_with("https://")) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  this->current_uri_ = uri;
  xEventGroupSetBits(this->event_group_, EventGroupBits::REQUEST_START);
  this->enable_loop();
  return true;
}

// Called from the orchestrator's main loop, so no synchronization needed with loop()
void HTTPRequestMediaSource::handle_command(media_source::MediaSourceCommand command) {
  if (this->decoding_state_ != HTTPDecodingState::DECODING) {
    return;
  }

  switch (command) {
    case media_source::MediaSourceCommand::STOP:
      xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_STOP);
      break;
    case media_source::MediaSourceCommand::PAUSE:
      xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_PAUSE);
      break;
    case media_source::MediaSourceCommand::PLAY:
      xEventGroupClearBits(this->event_group_, EventGroupBits::COMMAND_PAUSE);
      break;
    default:
      break;
  }
}

}  // namespace esphome::http_request

#endif  // USE_ESP32
