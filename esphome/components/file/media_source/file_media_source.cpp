#include "file_media_source.h"

#include "esphome/components/audio/audio_decoder.h"

namespace esphome {
namespace file {

static const uint32_t DECODE_TASK_STACK_SIZE = 3 * 1024;

static const char *const TAG = "file_media_source";

enum class SourceControls : uint8_t {
  START = 0,
  STOP = 1,
  PAUSE = 2,
  RESUME = 3,
};

struct ControlMessage {
  SourceControls control;
  audio::AudioFile *new_file{nullptr};
};

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  COMMAND_PAUSE = (1 << 1),
  TASK_STARTING = (1 << 7),
  TASK_RUNNING = (1 << 8),
  TASK_STOPPING = (1 << 9),
  TASK_STOPPED = (1 << 10),
};

void FileMediaSource::init_pipelines(size_t pipeline_count) {
  media_source::MediaSource::init_pipelines(pipeline_count);

  this->file_pipelines_.init(pipeline_count);
  for (size_t i = 0; i < pipeline_count; i++) {
    FileSourcePipeline ctx;

    // Create event group and queue upfront so they're available when play_uri is called
    ctx.event_group = xEventGroupCreate();
    ctx.controls_queue = xQueueCreate(3, sizeof(ControlMessage));

    this->file_pipelines_.push_back(ctx);
  }
}

bool FileMediaSource::play_uri(const std::string &uri, size_t pipeline) {
  if (pipeline >= this->file_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return false;
  }

  // Check if pipeline is already playing
  if (this->get_state(pipeline) != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s' on pipeline %zu: pipeline is busy", uri.c_str(), pipeline);
    return false;
  }

  // Validate URI starts with "file://"
  if (uri.find("file://") != 0) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  // Strip "file://" prefix and find the file
  std::string file_id = uri.substr(7);  // "file://" is 7 characters

  for (const auto &named_file : this->files_) {
    if (named_file.file_id == file_id) {
      if (!this->is_ready() || this->is_failed()) {
        return false;
      }

      auto &ctx = this->file_pipelines_[pipeline];

      // Queue playback start
      ControlMessage message = {.control = SourceControls::START, .new_file = named_file.file};
      xQueueSend(ctx.controls_queue, &message, 0);
      this->enable_loop_soon_any_context();
      return true;
    }
  }

  ESP_LOGE(TAG, "File not found: '%s'", file_id.c_str());
  return false;
}

void FileMediaSource::setup() {
  this->disable_loop();

  // Pipeline initialization happens via init_pipelines() called by MediaPlayer
  // Individual pipeline resources (event groups, queues, tasks) are created on-demand in loop()
}

void FileMediaSource::loop() {
  // Process each pipeline's state machine
  for (size_t pipeline = 0; pipeline < this->file_pipelines_.size(); pipeline++) {
    auto &ctx = this->file_pipelines_[pipeline];

    // Process control messages for this pipeline
    ControlMessage incoming_control;
    if (xQueueReceive(ctx.controls_queue, &incoming_control, 0)) {
      switch (incoming_control.control) {
        case SourceControls::START:
          ctx.current_file = incoming_control.new_file;
          ctx.decoding_state = FileDecodingState::START_TASK;
          break;
        case SourceControls::STOP:
          if (ctx.decoding_state == FileDecodingState::DECODING) {
            xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_STOP);
          }
          break;
        case SourceControls::PAUSE:
          if ((ctx.decoding_state == FileDecodingState::DECODING) &&
              (this->get_state(pipeline) == media_source::MediaSourceState::PLAYING)) {
            xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_PAUSE);
            this->set_state_(media_source::MediaSourceState::PAUSED, pipeline);
          }
          break;
        case SourceControls::RESUME:
          if ((ctx.decoding_state == FileDecodingState::DECODING) &&
              (this->get_state(pipeline) == media_source::MediaSourceState::PAUSED)) {
            // Clear the pause command bit to resume
            xEventGroupClearBits(ctx.event_group, EventGroupBits::COMMAND_PAUSE);
            this->set_state_(media_source::MediaSourceState::PLAYING, pipeline);
          }
          break;
      }
    }

    // Process pipeline state machine
    switch (ctx.decoding_state) {
      case FileDecodingState::START_TASK: {
        // Event group and queue already created in init_pipelines()
        // Start the task
        if (ctx.decode_task_handle == nullptr) {
          if (ctx.decode_task_stack_buffer == nullptr) {
            if (this->task_stack_in_psram_) {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
              ctx.decode_task_stack_buffer = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
            } else {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
              ctx.decode_task_stack_buffer = stack_allocator.allocate(DECODE_TASK_STACK_SIZE);
            }
          }
          if (ctx.decode_task_stack_buffer == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate decode task stack for pipeline %zu", pipeline);
            this->mark_failed();
            return;
          }

          char task_name[32];
          snprintf(task_name, sizeof(task_name), "FileDecode_%zu", pipeline);

          auto *params = new DecodeTaskParams{this, pipeline};
          ctx.decode_task_handle = xTaskCreateStatic(decode_task, task_name, DECODE_TASK_STACK_SIZE, params, 1,
                                                     ctx.decode_task_stack_buffer, &ctx.decode_task_stack);
          if (ctx.decode_task_handle == nullptr) {
            ESP_LOGE(TAG, "Failed to create decode task for pipeline %zu", pipeline);
            delete params;
            this->mark_failed();
            return;
          }
        }
        ESP_LOGD(TAG, "Started decode task for pipeline %zu", pipeline);
        ctx.decoding_state = FileDecodingState::DECODING;
        break;
      }
      case FileDecodingState::DECODING: {
        // Only state when we handle event group bits
        EventBits_t event_bits = xEventGroupGetBits(ctx.event_group);

        if (event_bits & TASK_STARTING) {
          ESP_LOGD(TAG, "Pipeline %zu starting", pipeline);
          xEventGroupClearBits(ctx.event_group, TASK_STARTING);
        }

        if (event_bits & TASK_RUNNING) {
          ESP_LOGD(TAG, "Pipeline %zu running", pipeline);
          xEventGroupClearBits(ctx.event_group, TASK_RUNNING);
          this->set_state_(media_source::MediaSourceState::PLAYING, pipeline);
        }

        if (event_bits & TASK_STOPPING) {
          ESP_LOGD(TAG, "Pipeline %zu stopping", pipeline);
          xEventGroupClearBits(ctx.event_group, TASK_STOPPING);
        }

        if (event_bits & TASK_STOPPED) {
          ESP_LOGD(TAG, "Pipeline %zu stopped", pipeline);
          xEventGroupClearBits(ctx.event_group, TASK_STOPPED | COMMAND_STOP | COMMAND_PAUSE);

          vTaskDelete(ctx.decode_task_handle);
          ctx.decode_task_handle = nullptr;
          if (ctx.decode_task_stack_buffer != nullptr) {
            if (this->task_stack_in_psram_) {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
              stack_allocator.deallocate(ctx.decode_task_stack_buffer, DECODE_TASK_STACK_SIZE);
            } else {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
              stack_allocator.deallocate(ctx.decode_task_stack_buffer, DECODE_TASK_STACK_SIZE);
            }
            ctx.decode_task_stack_buffer = nullptr;
          }
          this->set_state_(media_source::MediaSourceState::IDLE, pipeline);
          ctx.decoding_state = FileDecodingState::IDLE;
        }
        break;
      }
      case FileDecodingState::IDLE: {
        // Nothing to do when idle
        break;
      }
    }
  }

  // Check if we should disable loop when all pipelines are idle
  bool all_idle = true;
  for (const auto &p : this->file_pipelines_) {
    if (p.decoding_state != FileDecodingState::IDLE) {
      all_idle = false;
      break;
    }
  }
  if (all_idle) {
    this->disable_loop();
  }
}

void FileMediaSource::handle_command(media_source::MediaSourceCommand command, size_t pipeline) {
  if (pipeline >= this->file_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return;
  }

  auto &ctx = this->file_pipelines_[pipeline];
  if (ctx.controls_queue == nullptr) {
    return;
  }

  ControlMessage message;
  switch (command) {
    case media_source::MEDIA_SOURCE_COMMAND_END:
      // Intentional fallthrough
    case media_source::MEDIA_SOURCE_COMMAND_STOP: {
      if (ctx.decoding_state == FileDecodingState::DECODING) {
        message.control = SourceControls::STOP;
        xQueueSend(ctx.controls_queue, &message, 0);
      }
      break;
    }
    case media_source::MEDIA_SOURCE_COMMAND_PAUSE: {
      message.control = SourceControls::PAUSE;
      xQueueSend(ctx.controls_queue, &message, 0);
      break;
    }
    case media_source::MEDIA_SOURCE_COMMAND_PLAY: {
      message.control = SourceControls::RESUME;
      xQueueSend(ctx.controls_queue, &message, 0);
      break;
    }
    default:
      break;
  }
}

media_source::MediaSourceCapabilities FileMediaSource::get_capabilities() {
  media_source::MediaSourceCapabilities caps;
  caps.supports_pause = true;  // File playback can be paused
  return caps;
}

void FileMediaSource::decode_task(void *params) {
  auto *task_params = static_cast<DecodeTaskParams *>(params);
  FileMediaSource *this_source = task_params->source;
  size_t pipeline = task_params->pipeline;
  delete task_params;

  auto &ctx = this_source->file_pipelines_[pipeline];

  {
    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STARTING);

    // 0 bytes for input transfer buffer makes it an inplace buffer
    std::unique_ptr<audio::AudioDecoder> decoder = make_unique<audio::AudioDecoder>(0, 4096);

    esp_err_t err = decoder->start(ctx.current_file->file_type);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to start decoder on pipeline %zu: %s", pipeline, esp_err_to_name(err));
      xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPED);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    // Add the file as an inplace buffer source
    // BAD PRACTICE: removing const qualifier to match API
    decoder->add_source(const_cast<uint8_t *>(ctx.current_file->data), ctx.current_file->length);

    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_RUNNING);

    bool has_stream_info = false;

    while (true) {
      EventBits_t event_bits = xEventGroupGetBits(ctx.event_group);

      if (event_bits & EventGroupBits::COMMAND_STOP) {
        break;
      }

      decoder->set_pause_output_state(event_bits & EventGroupBits::COMMAND_PAUSE);

      // Will stop gracefully once finished with the current file
      audio::AudioDecoderState decoder_state = decoder->decode(true);

      if (decoder_state == audio::AudioDecoderState::FINISHED) {
        ESP_LOGD(TAG, "Pipeline %zu decoding finished", pipeline);
        break;
      } else if (decoder_state == audio::AudioDecoderState::FAILED) {
        ESP_LOGE(TAG, "Pipeline %zu decoding failed", pipeline);
        break;
      }

      if (!has_stream_info && decoder->get_audio_stream_info().has_value()) {
        ESP_LOGD(TAG, "Pipeline %zu got stream info from decoder", pipeline);
        has_stream_info = true;

        audio::AudioStreamInfo stream_info = decoder->get_audio_stream_info().value();

        if (stream_info.get_bits_per_sample() != 16) {
          // Error state, incompatible bits per sample
          ESP_LOGE(TAG, "Pipeline %zu: Incompatible bits per sample. Only 16 bits per sample is supported", pipeline);
          break;
        } else if ((stream_info.get_channels() > 2)) {
          // Error state, incompatible number of channels
          ESP_LOGE(TAG, "Pipeline %zu: Incompatible number of channels. Only 1 or 2 channel audio is supported.",
                   pipeline);
          break;
        } else {
          ESP_LOGD(TAG, "Pipeline %zu: Bits per sample: %d, Channels: %d, Sample rate: %d", pipeline,
                   stream_info.get_bits_per_sample(), stream_info.get_channels(), stream_info.get_sample_rate());

          // Check if callback is set before using it
          if (this_source->output_callback_) {
            // Wrap the output callback to include stream info and pipeline
            // The decoder's add_sink expects a 3-parameter callback, so we wrap it to add the 4th and 5th parameters
            std::function<size_t(uint8_t *, size_t, TickType_t)> decoder_callback =
                [this_source, stream_info, pipeline](uint8_t *data, size_t len, TickType_t ticks) {
                  return this_source->output_callback_(data, len, ticks, stream_info, pipeline);
                };
            esp_err_t err = decoder->add_sink(std::move(decoder_callback));
            if (err != ESP_OK) {
              ESP_LOGE(TAG, "Pipeline %zu: Failed to add sink to decoder: %s", pipeline, esp_err_to_name(err));
              break;
            }
            ESP_LOGD(TAG, "Pipeline %zu: Successfully added callback sink to decoder", pipeline);
          } else {
            ESP_LOGE(TAG,
                     "Pipeline %zu: Output callback is not set! Make sure the FileMediaSource is added to "
                     "media_sources in your YAML config",
                     pipeline);
            break;
          }
        }
      }
    }
    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPING);
  }
  xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPED);
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace file
}  // namespace esphome
