#include "http_media_source.h"

#include "esphome/components/audio/audio_decoder.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/core/log.h"

namespace esphome {
namespace http_request {

namespace {
struct AudioSinkAdapter : public audio::AudioSinkCallback {
  media_source::MediaSourceListener *listener;
  media_source::MediaSource *source;
  audio::AudioStreamInfo stream_info;
  size_t pipeline;

  size_t audio_sink_write(uint8_t *data, size_t length, TickType_t ticks_to_wait) override {
    return this->listener->on_media_output(this->source, data, length, ticks_to_wait, this->stream_info,
                                           this->pipeline);
  }
};
}  // namespace

static const uint32_t READ_TASK_STACK_SIZE = 5 * 1024;
static const uint32_t DECODE_TASK_STACK_SIZE = 3 * 1024;
static const size_t DEFAULT_TRANSFER_BUFFER_SIZE = 24 * 1024;

static const uint32_t READ_WRITE_TIMEOUT_MS = 20;
static const uint32_t CONNECTION_TIMEOUT_MS = 30000;  // 30 second timeout for no data
static const uint8_t MAX_CONNECTION_ATTEMPTS = 6;

static const char *const TAG = "http_media_source";

enum class SourceControls : uint8_t {
  START = 0,
  STOP = 1,
  PAUSE = 2,
  RESUME = 3,
};

struct ControlMessage {
  SourceControls control;
  std::string *uri{nullptr};  // Owned pointer, must delete after use
};

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  COMMAND_PAUSE = (1 << 1),
  READER_READY = (1 << 2),
  READER_FINISHED = (1 << 3),
  READER_ERROR = (1 << 4),
  DECODER_FINISHED = (1 << 5),
  DECODER_ERROR = (1 << 6),
  TASK_STARTING = (1 << 7),
  TASK_RUNNING = (1 << 8),
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

/// @brief Detect audio file type from Content-Type header or URL extension
/// @param content_type The Content-Type header value (can be empty)
/// @param url The URL to fallback to for extension detection
/// @return The detected AudioFileType, or NONE if unknown
static audio::AudioFileType detect_audio_type(const std::string &content_type, const std::string &url) {
  // Try to determine file type from Content-Type header first
  if (!content_type.empty()) {
    std::string ct_lower = str_lower_case(content_type);

#ifdef USE_AUDIO_MP3_SUPPORT
    if (ct_lower.find("audio/mp3") != std::string::npos || ct_lower.find("audio/mpeg") != std::string::npos) {
      return audio::AudioFileType::MP3;
    }
#endif
    if (ct_lower.find("audio/wav") != std::string::npos) {
      return audio::AudioFileType::WAV;
    }
#ifdef USE_AUDIO_FLAC_SUPPORT
    if (ct_lower.find("audio/flac") != std::string::npos || ct_lower.find("audio/x-flac") != std::string::npos) {
      return audio::AudioFileType::FLAC;
    }
#endif
  }

  // Fallback to URL extension
  std::string url_lower = str_lower_case(url);

  if (str_endswith(url_lower, ".wav")) {
    return audio::AudioFileType::WAV;
  }
#ifdef USE_AUDIO_MP3_SUPPORT
  if (str_endswith(url_lower, ".mp3")) {
    return audio::AudioFileType::MP3;
  }
#endif
#ifdef USE_AUDIO_FLAC_SUPPORT
  if (str_endswith(url_lower, ".flac")) {
    return audio::AudioFileType::FLAC;
  }
#endif

  return audio::AudioFileType::NONE;
}

void HTTPMediaSource::init_pipelines(size_t pipeline_count) {
  media_source::MediaSource::init_pipelines(pipeline_count);

  this->http_pipelines_.init(pipeline_count);
  for (size_t i = 0; i < pipeline_count; i++) {
    HTTPSourcePipeline ctx;

    // Create event group and queue upfront so they're available when play_uri is called
    ctx.event_group = xEventGroupCreate();
    ctx.controls_queue = xQueueCreate(3, sizeof(ControlMessage));

    this->http_pipelines_.push_back(ctx);
  }
}

bool HTTPMediaSource::play_uri(const std::string &uri, size_t pipeline) {
  if (pipeline >= this->http_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return false;
  }

  // Check if pipeline is already playing
  if (this->get_state(pipeline) != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s' on pipeline %zu: pipeline is busy", uri.c_str(), pipeline);
    return false;
  }

  // Validate URI starts with "http://" or "https://"
  if (!uri.starts_with("http://") && !uri.starts_with("https://")) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  if (!this->is_ready() || this->is_failed()) {
    return false;
  }

  auto &ctx = this->http_pipelines_[pipeline];

  // Queue playback start
  ControlMessage message = {.control = SourceControls::START, .uri = new std::string(uri)};
  xQueueSend(ctx.controls_queue, &message, 0);
  this->enable_loop_soon_any_context();
  return true;
}

void HTTPMediaSource::setup() {
  this->disable_loop();

  // Pipeline initialization happens via init_pipelines() called by MediaPlayer
  // Individual pipeline resources (event groups, queues, tasks) are created on-demand in loop()
}

void HTTPMediaSource::loop() {
  // Process each pipeline's state machine
  for (size_t pipeline = 0; pipeline < this->http_pipelines_.size(); pipeline++) {
    auto &ctx = this->http_pipelines_[pipeline];

    // Process control messages for this pipeline
    ControlMessage incoming_control;
    if (xQueueReceive(ctx.controls_queue, &incoming_control, 0)) {
      switch (incoming_control.control) {
        case SourceControls::START:
          if (incoming_control.uri != nullptr) {
            ctx.current_uri = *incoming_control.uri;
            delete incoming_control.uri;
          }
          ctx.decoding_state = HTTPDecodingState::START_TASKS;
          break;
        case SourceControls::STOP:
          if (ctx.decoding_state == HTTPDecodingState::DECODING) {
            xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_STOP);
          }
          break;
        case SourceControls::PAUSE:
          if ((ctx.decoding_state == HTTPDecodingState::DECODING) &&
              (this->get_state(pipeline) == media_source::MediaSourceState::PLAYING)) {
            xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_PAUSE);
            this->set_state_(media_source::MediaSourceState::PAUSED, pipeline);
          }
          break;
        case SourceControls::RESUME:
          if ((ctx.decoding_state == HTTPDecodingState::DECODING) &&
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
      case HTTPDecodingState::START_TASKS: {
        // Event group and queue already created in init_pipelines()
        // Start the read task
        if (ctx.read_task_handle == nullptr) {
          xEventGroupClearBits(ctx.event_group, ALL_BITS);
          if (ctx.read_task_stack_buffer == nullptr) {
            // Reader task uses HttpContainer which uses esp_http_client. This crashes on IDF 5.4 if the task
            // stack is in PSRAM. As a workaround, always allocate the read task in internal memory.
            RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
            ctx.read_task_stack_buffer = stack_allocator.allocate(READ_TASK_STACK_SIZE);
          }
          if (ctx.read_task_stack_buffer == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate read task stack for pipeline %zu", pipeline);
            this->mark_failed();
            return;
          }

          char task_name[32];
          snprintf(task_name, sizeof(task_name), "HTTPRead_%zu", pipeline);

          auto *params = new HTTPTaskParams{this, pipeline};
          ctx.read_task_handle = xTaskCreateStatic(read_task, task_name, READ_TASK_STACK_SIZE, params, 1,
                                                   ctx.read_task_stack_buffer, &ctx.read_task_stack);
          if (ctx.read_task_handle == nullptr) {
            ESP_LOGE(TAG, "Failed to create read task for pipeline %zu", pipeline);
            delete params;
            this->mark_failed();
            return;
          }
        }

        // Start the decode task
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
          snprintf(task_name, sizeof(task_name), "HTTPDecode_%zu", pipeline);

          auto *params = new HTTPTaskParams{this, pipeline};
          ctx.decode_task_handle = xTaskCreateStatic(decode_task, task_name, DECODE_TASK_STACK_SIZE, params, 1,
                                                     ctx.decode_task_stack_buffer, &ctx.decode_task_stack);
          if (ctx.decode_task_handle == nullptr) {
            ESP_LOGE(TAG, "Failed to create decode task for pipeline %zu", pipeline);
            delete params;
            this->mark_failed();
            return;
          }
        }

        ESP_LOGD(TAG, "Started read and decode tasks for pipeline %zu", pipeline);
        ctx.decoding_state = HTTPDecodingState::DECODING;
        break;
      }
      case HTTPDecodingState::DECODING: {
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

        if (event_bits & (READER_ERROR | DECODER_ERROR)) {
          ESP_LOGE(TAG, "Pipeline %zu error occurred during playback", pipeline);
          xEventGroupClearBits(ctx.event_group, READER_ERROR | DECODER_ERROR);
          this->set_state_(media_source::MediaSourceState::ERROR, pipeline);
        }

        // Check if both tasks are finished
        if ((event_bits & READER_FINISHED) && (event_bits & DECODER_FINISHED)) {
          ESP_LOGD(TAG, "Pipeline %zu both tasks finished", pipeline);

          // Delete tasks
          if (ctx.read_task_handle != nullptr) {
            vTaskDelete(ctx.read_task_handle);
            ctx.read_task_handle = nullptr;
            if (ctx.read_task_stack_buffer != nullptr) {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
              stack_allocator.deallocate(ctx.read_task_stack_buffer, READ_TASK_STACK_SIZE);
              ctx.read_task_stack_buffer = nullptr;
            }
          }

          if (ctx.decode_task_handle != nullptr) {
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
          }

          // Clear the finished and stop bits now that both tasks are cleaned up
          xEventGroupClearBits(ctx.event_group, READER_FINISHED | DECODER_FINISHED | COMMAND_STOP | COMMAND_PAUSE);

          this->set_state_(media_source::MediaSourceState::IDLE, pipeline);
          ctx.decoding_state = HTTPDecodingState::IDLE;
        }
        break;
      }
      case HTTPDecodingState::IDLE: {
        // Nothing to do when idle
        break;
      }
    }
  }

  // Check if we should disable loop when all pipelines are idle
  bool all_idle = true;
  for (const auto &p : this->http_pipelines_) {
    if (p.decoding_state != HTTPDecodingState::IDLE) {
      all_idle = false;
      break;
    }
  }
  if (all_idle) {
    this->disable_loop();
  }
}

void HTTPMediaSource::handle_command(media_source::MediaSourceCommand command, size_t pipeline) {
  if (pipeline >= this->http_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return;
  }

  auto &ctx = this->http_pipelines_[pipeline];
  if (ctx.controls_queue == nullptr) {
    return;
  }

  ControlMessage message;
  switch (command) {
    case media_source::MEDIA_SOURCE_COMMAND_END:
      // Intentional fallthrough
    case media_source::MEDIA_SOURCE_COMMAND_STOP: {
      if (ctx.decoding_state == HTTPDecodingState::DECODING) {
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

media_source::MediaSourceCapabilities HTTPMediaSource::get_capabilities() {
  media_source::MediaSourceCapabilities caps;
  caps.supports_pause = true;  // HTTP streaming can be paused
  return caps;
}

void HTTPMediaSource::read_task(void *params) {
  auto *task_params = static_cast<HTTPTaskParams *>(params);
  HTTPMediaSource *this_source = task_params->source;
  size_t pipeline = task_params->pipeline;
  delete task_params;

  auto &ctx = this_source->http_pipelines_[pipeline];

  {  // Ensure all C++ objects fall out of scope and deallocate
    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STARTING);

    // Get the parent HttpRequestComponent to make HTTP requests
    HttpRequestComponent *http_client = this_source->get_parent();

    // Request Content-Type header for file type detection
    std::set<std::string> collect_headers = {"content-type"};

    // Start HTTP request, retrying on transient failures (e.g., EAGAIN during header fetch)
    std::shared_ptr<HttpContainer> container;
    for (uint8_t attempt = 0; attempt < MAX_CONNECTION_ATTEMPTS; ++attempt) {
      if (xEventGroupGetBits(ctx.event_group) & EventGroupBits::COMMAND_STOP) {
        break;
      }

      container = http_client->get(ctx.current_uri, {}, collect_headers);

      if (container != nullptr && is_success(container->status_code)) {
        break;  // Success
      }

      // Clean up failed attempt
      if (container != nullptr) {
        ESP_LOGW(TAG, "Pipeline %zu: HTTP request attempt %u failed with status %d", pipeline, attempt + 1,
                 container->status_code);
        container->end();
        container.reset();
      } else {
        ESP_LOGW(TAG, "Pipeline %zu: HTTP request attempt %u failed to connect", pipeline, attempt + 1);
      }

      if (attempt + 1 < MAX_CONNECTION_ATTEMPTS) {
        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait before retry
      }
    }

    if (container == nullptr || !is_success(container->status_code)) {
      ESP_LOGE(TAG, "Pipeline %zu: HTTP request failed after %u attempts", pipeline, MAX_CONNECTION_ATTEMPTS);
      if (container != nullptr) {
        container->end();
      }
      xEventGroupSetBits(ctx.event_group,
                         EventGroupBits::READER_ERROR | EventGroupBits::READER_FINISHED | EventGroupBits::COMMAND_STOP);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    // Detect audio file type from Content-Type header or URL
    std::string content_type = container->get_response_header("content-type");
    ctx.current_audio_file_type = detect_audio_type(content_type, ctx.current_uri);

    if (ctx.current_audio_file_type == audio::AudioFileType::NONE) {
      ESP_LOGE(TAG, "Pipeline %zu: Unable to determine audio file type", pipeline);
      container->end();
      xEventGroupSetBits(ctx.event_group,
                         EventGroupBits::READER_ERROR | EventGroupBits::READER_FINISHED | EventGroupBits::COMMAND_STOP);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    // Create transfer buffer for efficient writes to ring buffer
    size_t transfer_buffer_size = std::min(this_source->buffer_size_ / 4, DEFAULT_TRANSFER_BUFFER_SIZE);
    std::unique_ptr<audio::AudioSinkTransferBuffer> transfer_buffer =
        audio::AudioSinkTransferBuffer::create(transfer_buffer_size);

    if (transfer_buffer == nullptr) {
      ESP_LOGE(TAG, "Pipeline %zu: Failed to create transfer buffer", pipeline);
      container->end();
      xEventGroupSetBits(ctx.event_group,
                         EventGroupBits::READER_ERROR | EventGroupBits::READER_FINISHED | EventGroupBits::COMMAND_STOP);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    {  // Ensures temp_ring_buffer falls out of scope and deallocates (transfer_buffer will still own it)
      // Create ring buffer for raw audio data
      std::shared_ptr<RingBuffer> temp_ring_buffer;
      if (!ctx.raw_file_ring_buffer.use_count()) {
        temp_ring_buffer = RingBuffer::create(this_source->buffer_size_);
        ctx.raw_file_ring_buffer = temp_ring_buffer;
      }

      if (!ctx.raw_file_ring_buffer.use_count()) {
        ESP_LOGE(TAG, "Pipeline %zu: Failed to create ring buffer", pipeline);
        container->end();
        xEventGroupSetBits(ctx.event_group, EventGroupBits::READER_ERROR | EventGroupBits::READER_FINISHED |
                                                EventGroupBits::COMMAND_STOP);
        while (true) {
          vTaskDelay(pdMS_TO_TICKS(1000));
        }
      }

      transfer_buffer->set_sink(ctx.raw_file_ring_buffer);
    }

    // Signal that reader is ready
    xEventGroupSetBits(ctx.event_group, EventGroupBits::READER_READY);

    uint32_t last_data_time = millis();

    // Main read loop
    while (true) {
      EventBits_t event_bits = xEventGroupGetBits(ctx.event_group);

      if (event_bits & EventGroupBits::COMMAND_STOP) {
        break;
      }

      // Transfer any buffered data to the ring buffer
      transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);

      if (transfer_buffer->free() > 0) {
        // Read directly from HttpContainer. We don't use http_read_loop_result() here because:
        // - We're on a dedicated FreeRTOS task, not the main loop
        // - esp_http_client_read() blocks, so tight-spinning isn't a concern
        // - Transient errors (e.g., -ESP_ERR_HTTP_EAGAIN) should retry, not fail immediately
        int received_len = container->read(transfer_buffer->get_buffer_end(), transfer_buffer->free());

        if (received_len > 0) {
          last_data_time = millis();
          transfer_buffer->increase_buffer_length(received_len);
        } else if (received_len == 0 && container->is_read_complete()) {
          // Flush remaining buffered data to the ring buffer, retrying until empty
          while (transfer_buffer->available() > 0) {
            if (xEventGroupGetBits(ctx.event_group) & EventGroupBits::COMMAND_STOP) {
              break;
            }
            transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);
          }
          ESP_LOGD(TAG, "Pipeline %zu: Reader finished", pipeline);
          break;
        } else if (millis() - last_data_time >= CONNECTION_TIMEOUT_MS) {
          ESP_LOGE(TAG, "Pipeline %zu: Reader timed out", pipeline);
          xEventGroupSetBits(ctx.event_group, EventGroupBits::READER_ERROR | EventGroupBits::COMMAND_STOP);
          break;
        }
        // else: no data yet or transient error (e.g., EAGAIN), loop continues
      }
    }

    // Clean up
    transfer_buffer.reset();
    container->end();
  }
  // Set READER_FINISHED bit to signal we're done
  xEventGroupSetBits(ctx.event_group, EventGroupBits::READER_FINISHED);

  // Wait a bit before releasing ownership of the ring buffer to ensure decode task has acquired it
  EventBits_t event_bits = xEventGroupGetBits(ctx.event_group);
  if ((event_bits & EventGroupBits::READER_READY) || (ctx.raw_file_ring_buffer.use_count() == 1)) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void HTTPMediaSource::decode_task(void *params) {
  auto *task_params = static_cast<HTTPTaskParams *>(params);
  HTTPMediaSource *this_source = task_params->source;
  size_t pipeline = task_params->pipeline;
  delete task_params;
  auto &ctx = this_source->http_pipelines_[pipeline];

  {  // Ensure all C++ objects fall out of scope and deallocate

    // Wait until the reader notifies us that it's ready or receive a stop command
    xEventGroupWaitBits(
        ctx.event_group,
        EventGroupBits::READER_READY | EventGroupBits::COMMAND_STOP,  // Bit message to read
        pdFALSE,                                                      // Don't clear the bit on exit
        pdFALSE,                                                      // Wait for any bit
        pdMS_TO_TICKS(
            CONNECTION_TIMEOUT_MS));  // Timeout to avoid indefinitely waiting for the reader task to get ready

    xEventGroupClearBits(ctx.event_group, EventGroupBits::READER_READY);

    EventBits_t event_bits = xEventGroupGetBits(ctx.event_group);
    if (event_bits & EventGroupBits::COMMAND_STOP) {
      xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_FINISHED);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    size_t transfer_buffer_size = std::min(this_source->buffer_size_ / 4, DEFAULT_TRANSFER_BUFFER_SIZE);
    std::unique_ptr<audio::AudioDecoder> decoder =
        make_unique<audio::AudioDecoder>(transfer_buffer_size, transfer_buffer_size);

    esp_err_t err = decoder->start(ctx.current_audio_file_type);
    decoder->add_source(ctx.raw_file_ring_buffer);

    if (err != ESP_OK) {
      decoder.reset();
      ESP_LOGE(TAG, "Pipeline %zu: Failed to start decoder: %s", pipeline, esp_err_to_name(err));
      xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_ERROR | EventGroupBits::DECODER_FINISHED |
                                              EventGroupBits::COMMAND_STOP);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_RUNNING);

    AudioSinkAdapter audio_sink;
    bool has_stream_info = false;

    while (true) {
      event_bits = xEventGroupGetBits(ctx.event_group);

      if (event_bits & EventGroupBits::COMMAND_STOP) {
        break;
      }

      decoder->set_pause_output_state(event_bits & EventGroupBits::COMMAND_PAUSE);

      // Will stop gracefully once reader finished
      audio::AudioDecoderState decoder_state = decoder->decode(event_bits & EventGroupBits::READER_FINISHED);

      if (decoder_state == audio::AudioDecoderState::FINISHED) {
        ESP_LOGD(TAG, "Pipeline %zu: Decoding finished", pipeline);
        break;
      } else if (decoder_state == audio::AudioDecoderState::FAILED) {
        ESP_LOGE(TAG, "Pipeline %zu: Decoding failed", pipeline);
        xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
        break;
      }

      if (!has_stream_info && decoder->get_audio_stream_info().has_value()) {
        ESP_LOGD(TAG, "Pipeline %zu: Got stream info from decoder", pipeline);
        has_stream_info = true;

        audio::AudioStreamInfo stream_info = decoder->get_audio_stream_info().value();

        if (stream_info.get_bits_per_sample() != 16) {
          // Error state, incompatible bits per sample
          ESP_LOGE(TAG, "Pipeline %zu: Incompatible bits per sample. Only 16 bits per sample is supported", pipeline);
          xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
          break;
        } else if ((stream_info.get_channels() > 2)) {
          // Error state, incompatible number of channels
          ESP_LOGE(TAG, "Pipeline %zu: Incompatible number of channels. Only 1 or 2 channel audio is supported.",
                   pipeline);
          xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
          break;
        } else {
          ESP_LOGD(TAG, "Pipeline %zu: Bits per sample: %d, Channels: %d, Sample rate: %d", pipeline,
                   stream_info.get_bits_per_sample(), stream_info.get_channels(), stream_info.get_sample_rate());

          if (this_source->get_listener() != nullptr) {
            audio_sink.listener = this_source->get_listener();
            audio_sink.source = this_source;
            audio_sink.stream_info = stream_info;
            audio_sink.pipeline = pipeline;
            esp_err_t err = decoder->add_sink(&audio_sink);
            if (err != ESP_OK) {
              ESP_LOGE(TAG, "Pipeline %zu: Failed to add sink to decoder: %s", pipeline, esp_err_to_name(err));
              xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
              break;
            }
            ESP_LOGD(TAG, "Pipeline %zu: Successfully added callback sink to decoder", pipeline);
          } else {
            ESP_LOGE(TAG,
                     "Pipeline %zu: Listener is not set! Make sure the HTTPMediaSource is added to media_sources "
                     "in your YAML config",
                     pipeline);
            xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_ERROR | EventGroupBits::COMMAND_STOP);
            break;
          }
        }
      }
    }

    decoder.reset();
  }
  // Set DECODER_FINISHED bit to signal we're done
  xEventGroupSetBits(ctx.event_group, EventGroupBits::DECODER_FINISHED);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace http_request
}  // namespace esphome
