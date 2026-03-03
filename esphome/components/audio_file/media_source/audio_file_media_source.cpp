#include "audio_file_media_source.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio_decoder.h"

#include <cstring>

namespace esphome::audio_file {

namespace {
struct AudioSinkAdapter : public audio::AudioSinkCallback {
  media_source::MediaSource *source;
  audio::AudioStreamInfo stream_info;

  size_t audio_sink_write(uint8_t *data, size_t length, TickType_t ticks_to_wait) override {
    return this->source->write_output(data, length, pdTICKS_TO_MS(ticks_to_wait), this->stream_info);
  }
};
}  // namespace

#if defined(USE_AUDIO_OPUS_SUPPORT)
static constexpr uint32_t DECODE_TASK_STACK_SIZE = 5 * 1024;
#else
static constexpr uint32_t DECODE_TASK_STACK_SIZE = 3 * 1024;
#endif

static const char *const TAG = "audio_file_media_source";

enum EventGroupBits : uint32_t {
  // Requests to start playback (set by play_uri, handled by loop)
  REQUEST_START = (1 << 0),
  // Commands from main loop to decode task
  COMMAND_STOP = (1 << 1),
  COMMAND_PAUSE = (1 << 2),
  // Decode task lifecycle signals (one-shot, cleared by loop)
  TASK_STARTING = (1 << 7),
  TASK_RUNNING = (1 << 8),
  TASK_STOPPING = (1 << 10),
  TASK_STOPPED = (1 << 11),
  TASK_ERROR = (1 << 12),
  // Decode task state (level-triggered, set/cleared by decode task)
  TASK_PAUSED = (1 << 9),
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

void AudioFileMediaSource::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio File Media Source:");
  ESP_LOGCONFIG(TAG, "  Registered audio files:");
  for (const auto &named_file : get_named_audio_files()) {
    ESP_LOGCONFIG(TAG, "    - ID: '%s'", named_file.file_id);
  }
  ESP_LOGCONFIG(TAG, "  Task Stack in PSRAM: %s", this->task_stack_in_psram_ ? "Yes" : "No");
}

void AudioFileMediaSource::setup() {
  this->disable_loop();

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }
}

void AudioFileMediaSource::loop() {
  EventBits_t event_bits = xEventGroupGetBits(this->event_group_);

  if (event_bits & REQUEST_START) {
    xEventGroupClearBits(this->event_group_, REQUEST_START);
    this->decoding_state_ = AudioFileDecodingState::START_TASK;
  }

  switch (this->decoding_state_) {
    case AudioFileDecodingState::START_TASK: {
      if (this->decode_task_handle_ == nullptr) {
        xEventGroupClearBits(this->event_group_, ALL_BITS);
        if (!this->create_decode_task_()) {
          this->delete_decode_task_();
          this->set_state_(media_source::MediaSourceState::ERROR);
          this->decoding_state_ = AudioFileDecodingState::IDLE;
          this->status_momentary_error("task_create", 1000);
          return;
        }
      }
      this->decoding_state_ = AudioFileDecodingState::DECODING;
      break;
    }
    case AudioFileDecodingState::DECODING: {
      if (event_bits & TASK_STARTING) {
        ESP_LOGD(TAG, "Starting");
        xEventGroupClearBits(this->event_group_, TASK_STARTING);
      }

      if (event_bits & TASK_RUNNING) {
        ESP_LOGV(TAG, "Started");
        xEventGroupClearBits(this->event_group_, TASK_RUNNING);
        this->set_state_(media_source::MediaSourceState::PLAYING);
      }

      if (event_bits & TASK_PAUSED) {
        if (this->get_state() != media_source::MediaSourceState::PAUSED) {
          this->set_state_(media_source::MediaSourceState::PAUSED);
        }
      } else {
        if (this->get_state() == media_source::MediaSourceState::PAUSED) {
          this->set_state_(media_source::MediaSourceState::PLAYING);
        }
      }

      if (event_bits & TASK_STOPPING) {
        ESP_LOGV(TAG, "Stopping");
        xEventGroupClearBits(this->event_group_, TASK_STOPPING);
      }

      if (event_bits & TASK_ERROR) {
        // Report error so the orchestrator knows playback failed; task will have already logged the specific error
        this->set_state_(media_source::MediaSourceState::ERROR);
      }

      if (event_bits & TASK_STOPPED) {
        ESP_LOGD(TAG, "Stopped");
        xEventGroupClearBits(this->event_group_, ALL_BITS);

        this->delete_decode_task_();
        this->set_state_(media_source::MediaSourceState::IDLE);
        this->decoding_state_ = AudioFileDecodingState::IDLE;
      }
      break;
    }
    case AudioFileDecodingState::IDLE: {
      if (this->get_state() == media_source::MediaSourceState::ERROR && !this->status_has_error()) {
        this->set_state_(media_source::MediaSourceState::IDLE);
      }
      break;
    }
  }

  if ((this->decoding_state_ == AudioFileDecodingState::IDLE) &&
      (this->get_state() == media_source::MediaSourceState::IDLE)) {
    this->disable_loop();
  }
}

// Called from the orchestrator's main loop, so no synchronization needed with loop()
bool AudioFileMediaSource::play_uri(const std::string &uri) {
  if (!this->is_ready() || this->is_failed() || this->status_has_error()) {
    return false;
  }

  // Check if source is already playing
  if (this->get_state() != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s': source is busy", uri.c_str());
    return false;
  }

  // Validate URI starts with "audio-file://"
  if (!uri.starts_with("audio-file://")) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  // Strip "audio-file://" prefix and find the file
  const char *file_id = uri.c_str() + 13;  // "audio-file://" is 13 characters

  for (const auto &named_file : get_named_audio_files()) {
    if (strcmp(named_file.file_id, file_id) == 0) {
      this->current_file_ = named_file.file;
      xEventGroupSetBits(this->event_group_, EventGroupBits::REQUEST_START);
      this->enable_loop();
      return true;
    }
  }

  ESP_LOGE(TAG, "File not found: '%s'", file_id);
  return false;
}

// Called from the orchestrator's main loop, so no synchronization needed with loop()
void AudioFileMediaSource::handle_command(media_source::MediaSourceCommand command) {
  if (this->event_group_ == nullptr) {
    return;
  }

  switch (command) {
    case media_source::MediaSourceCommand::STOP:
      if (this->decoding_state_ == AudioFileDecodingState::DECODING) {
        xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_STOP);
      }
      break;
    case media_source::MediaSourceCommand::PAUSE:
      if (this->decoding_state_ == AudioFileDecodingState::DECODING) {
        xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_PAUSE);
      }
      break;
    case media_source::MediaSourceCommand::PLAY:
      if (this->decoding_state_ == AudioFileDecodingState::DECODING) {
        xEventGroupClearBits(this->event_group_, EventGroupBits::COMMAND_PAUSE);
      }
      break;
    default:
      break;
  }
}

bool AudioFileMediaSource::create_decode_task_() {
  if (this->decode_task_stack_buffer_ == nullptr) {
    if (this->task_stack_in_psram_) {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
    } else {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
      this->decode_task_stack_buffer_ = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
    }
  }
  if (this->decode_task_stack_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate stack");
    return false;
  }

  this->decode_task_handle_ = xTaskCreateStatic(decode_task, "AudioFileDec", DECODE_TASK_STACK_SIZE, this, 1,
                                                this->decode_task_stack_buffer_, &this->decode_task_stack_);
  if (this->decode_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create task");
    return false;
  }

  return true;
}

void AudioFileMediaSource::delete_decode_task_() {
  if (this->decode_task_handle_ != nullptr) {
    vTaskDelete(this->decode_task_handle_);
    this->decode_task_handle_ = nullptr;
  }
  if (this->decode_task_stack_buffer_ != nullptr) {
    if (this->task_stack_in_psram_) {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      stack_allocator.deallocate(this->decode_task_stack_buffer_, DECODE_TASK_STACK_SIZE);
    } else {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
      stack_allocator.deallocate(this->decode_task_stack_buffer_, DECODE_TASK_STACK_SIZE);
    }
    this->decode_task_stack_buffer_ = nullptr;
  }
}

void AudioFileMediaSource::decode_task(void *params) {
  AudioFileMediaSource *this_source = static_cast<AudioFileMediaSource *>(params);

  {  // Ensures C++ objects fall out of scope before task is deleted

    xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_STARTING);

    // 0 bytes for input transfer buffer makes it an inplace buffer
    std::unique_ptr<audio::AudioDecoder> decoder = make_unique<audio::AudioDecoder>(0, 4096);

    esp_err_t err = decoder->start(this_source->current_file_->file_type);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to start decoder: %s", esp_err_to_name(err));
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_ERROR | EventGroupBits::TASK_STOPPED);
      vTaskSuspend(nullptr);  // Suspend this task indefinitely until the loop method deletes it
    }

    // Add the file as a const data source
    decoder->add_source(this_source->current_file_->data, this_source->current_file_->length);

    xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_RUNNING);

    AudioSinkAdapter audio_sink;
    bool has_stream_info = false;

    while (true) {
      EventBits_t event_bits = xEventGroupGetBits(this_source->event_group_);

      if (event_bits & EventGroupBits::COMMAND_STOP) {
        break;
      }

      bool paused = event_bits & EventGroupBits::COMMAND_PAUSE;
      decoder->set_pause_output_state(paused);
      if (paused) {
        xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_PAUSED);
        vTaskDelay(pdMS_TO_TICKS(20));
      } else {
        xEventGroupClearBits(this_source->event_group_, EventGroupBits::TASK_PAUSED);
      }

      // Will stop gracefully once finished with the current file
      audio::AudioDecoderState decoder_state = decoder->decode(true);

      if (decoder_state == audio::AudioDecoderState::FINISHED) {
        break;
      } else if (decoder_state == audio::AudioDecoderState::FAILED) {
        ESP_LOGE(TAG, "Decoder failed");
        xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_ERROR);
        break;
      }

      if (!has_stream_info && decoder->get_audio_stream_info().has_value()) {
        has_stream_info = true;

        audio::AudioStreamInfo stream_info = decoder->get_audio_stream_info().value();

        if (stream_info.get_bits_per_sample() != 16) {
          ESP_LOGE(TAG, "Incompatible bits per sample. Only 16 bits per sample is supported");
          xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_ERROR);
          break;
        } else if (stream_info.get_channels() > 2) {
          ESP_LOGE(TAG, "Incompatible number of channels. Only 1 or 2 channel audio is supported.");
          xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_ERROR);
          break;
        } else {
          ESP_LOGD(TAG, "Bits per sample: %d, Channels: %d, Sample rate: %d", stream_info.get_bits_per_sample(),
                   stream_info.get_channels(), stream_info.get_sample_rate());

          if (this_source->has_listener()) {
            audio_sink.source = this_source;
            audio_sink.stream_info = stream_info;
            esp_err_t err = decoder->add_sink(&audio_sink);
            if (err != ESP_OK) {
              ESP_LOGE(TAG, "Failed to add sink to decoder: %s", esp_err_to_name(err));
              xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_ERROR);
              break;
            }
          } else {
            ESP_LOGE(TAG, "Listener not set");
            xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_ERROR);
            break;
          }
        }
      }
    }
    xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_STOPPING);
  }
  xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_STOPPED);
  vTaskSuspend(nullptr);  // Suspend this task indefinitely until the loop method deletes it
}

}  // namespace esphome::audio_file

#endif  // USE_ESP32
