#include "color_noise_media_source.h"

#include "esphome/components/audio/audio_transfer_buffer.h"

#include <cstdlib>

namespace esphome {
namespace color_noise {

static const uint32_t GENERATE_TASK_STACK_SIZE = 3 * 1024;
static const uint32_t READ_WRITE_TIMEOUT_MS = 20;

static const char *const TAG = "color_noise_media_source";

enum class SourceControls : uint8_t {
  START = 0,
  STOP = 1,
  PAUSE = 2,
  RESUME = 3,
};

struct ControlMessage {
  SourceControls control;
  uint32_t seed{0};
  size_t total_samples_to_generate{0};  // 0 = infinite playback
};

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  COMMAND_PAUSE = (1 << 1),
  TASK_STARTING = (1 << 7),
  TASK_RUNNING = (1 << 8),
  TASK_STOPPING = (1 << 9),
  TASK_STOPPED = (1 << 10),
};

void ColorNoiseMediaSource::init_pipelines(size_t pipeline_count) {
  media_source::MediaSource::init_pipelines(pipeline_count);

  this->color_noise_pipelines_.init(pipeline_count);
  for (size_t i = 0; i < pipeline_count; i++) {
    ColorNoiseSourcePipeline ctx;

    // Create event group and queue upfront so they're available when play_uri is called
    ctx.event_group = xEventGroupCreate();
    ctx.controls_queue = xQueueCreate(3, sizeof(ControlMessage));

    this->color_noise_pipelines_.push_back(ctx);
  }
}

bool ColorNoiseMediaSource::play_uri(const std::string &uri, size_t pipeline) {
  if (pipeline >= this->color_noise_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return false;
  }

  // Check if pipeline is already playing
  if (this->get_state(pipeline) != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s' on pipeline %zu: pipeline is busy", uri.c_str(), pipeline);
    return false;
  }

  // Validate URI starts with "color-noise://"
  if (!uri.starts_with("color-noise://")) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  // Parse noise type from host part (between "color-noise://" and "/")
  size_t host_start = 14;  // Length of "color-noise://"
  size_t host_end = uri.find('/', host_start);
  if (host_end == std::string::npos) {
    ESP_LOGE(TAG, "Invalid URI format: '%s' (missing '/' after host)", uri.c_str());
    return false;
  }

  std::string noise_type_str = uri.substr(host_start, host_end - host_start);
  NoiseType noise_type;

  if (noise_type_str == "white") {
    noise_type = NoiseType::WHITE;
  } else if (noise_type_str == "brown") {
    noise_type = NoiseType::BROWN;
  } else if (noise_type_str == "pink") {
    noise_type = NoiseType::PINK;
  } else {
    ESP_LOGE(TAG, "Invalid noise type: '%s'. Must be 'white', 'brown', or 'pink'", noise_type_str.c_str());
    return false;
  }

  if (!this->is_ready() || this->is_failed()) {
    return false;
  }

  auto &ctx = this->color_noise_pipelines_[pipeline];

  // Store the noise type in the context
  ctx.noise_type = noise_type;

  // Parse URI for optional seed and duration parameters
  // Format: color-noise://<type>/ or color-noise://<type>/?seed=12345&duration=10
  // where <type> is 'white', 'brown', or 'pink'
  uint32_t seed = this->default_seed_;
  uint32_t duration_seconds = 0;  // 0 = infinite playback

  // If default seed is 0, generate a random seed
  if (seed == 0) {
    seed = random_uint32();
  }

  size_t query_pos = uri.find('?');
  if (query_pos != std::string::npos) {
    std::string query = uri.substr(query_pos + 1);

    // Simple query parser for seed parameter
    size_t seed_pos = query.find("seed=");
    if (seed_pos != std::string::npos) {
      seed_pos += 5;  // Skip "seed="
      size_t end_pos = query.find('&', seed_pos);
      size_t len = (end_pos == std::string::npos) ? std::string::npos : end_pos - seed_pos;
      std::string seed_str = query.substr(seed_pos, len);
      seed = std::strtoul(seed_str.c_str(), nullptr, 10);
    }

    // Simple query parser for duration parameter
    size_t duration_pos = query.find("duration=");
    if (duration_pos != std::string::npos) {
      duration_pos += 9;  // Skip "duration="
      size_t end_pos = query.find('&', duration_pos);
      size_t len = (end_pos == std::string::npos) ? std::string::npos : end_pos - duration_pos;
      std::string duration_str = query.substr(duration_pos, len);
      duration_seconds = std::strtoul(duration_str.c_str(), nullptr, 10);
    }
  }

  // Calculate total samples to generate based on duration
  size_t total_samples = 0;
  if (duration_seconds > 0) {
    // Use AudioStreamInfo to convert duration to samples
    audio::AudioStreamInfo stream_info(16, 1, this->sample_rate_);
    total_samples = stream_info.ms_to_samples(duration_seconds * 1000);
  }

  const char *noise_type_name = (noise_type == NoiseType::WHITE)   ? "white"
                                : (noise_type == NoiseType::BROWN) ? "brown"
                                                                   : "pink";

  if (duration_seconds > 0) {
    ESP_LOGD(TAG, "Playing %s noise on pipeline %zu with seed: %u, duration: %u seconds (%zu samples)", noise_type_name,
             pipeline, seed, duration_seconds, total_samples);
  } else {
    ESP_LOGD(TAG, "Playing %s noise on pipeline %zu with seed: %u (infinite playback)", noise_type_name, pipeline,
             seed);
  }

  // Queue playback start
  ControlMessage message = {.control = SourceControls::START, .seed = seed, .total_samples_to_generate = total_samples};
  xQueueSend(ctx.controls_queue, &message, 0);
  this->enable_loop_soon_any_context();
  return true;
}

void ColorNoiseMediaSource::setup() {
  this->disable_loop();

  // Pipeline initialization happens via init_pipelines() called by MediaPlayer
  // Individual pipeline resources (event groups, queues, tasks) are created on-demand in loop()
}

void ColorNoiseMediaSource::loop() {
  // Process each pipeline's state machine
  for (size_t pipeline = 0; pipeline < this->color_noise_pipelines_.size(); pipeline++) {
    auto &ctx = this->color_noise_pipelines_[pipeline];

    // Process control messages for this pipeline
    ControlMessage incoming_control;
    if (xQueueReceive(ctx.controls_queue, &incoming_control, 0)) {
      switch (incoming_control.control) {
        case SourceControls::START:
          ctx.seed = incoming_control.seed;
          ctx.total_samples_to_generate = incoming_control.total_samples_to_generate;
          ctx.samples_generated = 0;  // Reset sample counter
          ctx.paused = false;
          ctx.generation_state = ColorNoiseGenerationState::START_TASK;
          break;
        case SourceControls::STOP:
          if (ctx.generation_state == ColorNoiseGenerationState::GENERATING) {
            xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_STOP);
          }
          break;
        case SourceControls::PAUSE:
          if ((ctx.generation_state == ColorNoiseGenerationState::GENERATING) &&
              (this->get_state(pipeline) == media_source::MediaSourceState::PLAYING)) {
            xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_PAUSE);
            this->set_state_(media_source::MediaSourceState::PAUSED, pipeline);
          }
          break;
        case SourceControls::RESUME:
          if ((ctx.generation_state == ColorNoiseGenerationState::GENERATING) &&
              (this->get_state(pipeline) == media_source::MediaSourceState::PAUSED)) {
            // Clear the pause command bit to resume
            xEventGroupClearBits(ctx.event_group, EventGroupBits::COMMAND_PAUSE);
            this->set_state_(media_source::MediaSourceState::PLAYING, pipeline);
          }
          break;
      }
    }

    // Process pipeline state machine
    switch (ctx.generation_state) {
      case ColorNoiseGenerationState::START_TASK: {
        // Event group and queue already created in init_pipelines()
        // Start the task
        if (ctx.generate_task_handle == nullptr) {
          if (ctx.generate_task_stack_buffer == nullptr) {
            if (this->task_stack_in_psram_) {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
              ctx.generate_task_stack_buffer = stack_allocator.allocate(GENERATE_TASK_STACK_SIZE);
            } else {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
              ctx.generate_task_stack_buffer = stack_allocator.allocate(GENERATE_TASK_STACK_SIZE);
            }
          }
          if (ctx.generate_task_stack_buffer == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate generate task stack for pipeline %zu", pipeline);
            this->mark_failed();
            return;
          }

          char task_name[32];
          snprintf(task_name, sizeof(task_name), "NoiseGen_%zu", pipeline);

          auto *params = new GenerateTaskParams{this, pipeline};
          ctx.generate_task_handle = xTaskCreateStatic(generate_task, task_name, GENERATE_TASK_STACK_SIZE, params, 1,
                                                       ctx.generate_task_stack_buffer, &ctx.generate_task_stack);
          if (ctx.generate_task_handle == nullptr) {
            ESP_LOGE(TAG, "Failed to create generate task for pipeline %zu", pipeline);
            delete params;
            this->mark_failed();
            return;
          }
        }
        ESP_LOGD(TAG, "Started generate task for pipeline %zu", pipeline);
        ctx.generation_state = ColorNoiseGenerationState::GENERATING;
        break;
      }
      case ColorNoiseGenerationState::GENERATING: {
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

          vTaskDelete(ctx.generate_task_handle);
          ctx.generate_task_handle = nullptr;
          if (ctx.generate_task_stack_buffer != nullptr) {
            if (this->task_stack_in_psram_) {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
              stack_allocator.deallocate(ctx.generate_task_stack_buffer, GENERATE_TASK_STACK_SIZE);
            } else {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
              stack_allocator.deallocate(ctx.generate_task_stack_buffer, GENERATE_TASK_STACK_SIZE);
            }
            ctx.generate_task_stack_buffer = nullptr;
          }
          this->set_state_(media_source::MediaSourceState::IDLE, pipeline);
          ctx.generation_state = ColorNoiseGenerationState::IDLE;
        }
        break;
      }
      case ColorNoiseGenerationState::IDLE: {
        // Nothing to do when idle
        break;
      }
    }
  }

  // Check if we should disable loop when all pipelines are idle
  bool all_idle = true;
  for (const auto &p : this->color_noise_pipelines_) {
    if (p.generation_state != ColorNoiseGenerationState::IDLE) {
      all_idle = false;
      break;
    }
  }
  if (all_idle) {
    this->disable_loop();
  }
}

void ColorNoiseMediaSource::handle_command(media_source::MediaSourceCommand command, size_t pipeline) {
  if (pipeline >= this->color_noise_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return;
  }

  auto &ctx = this->color_noise_pipelines_[pipeline];
  if (ctx.controls_queue == nullptr) {
    return;
  }

  ControlMessage message;
  switch (command) {
    case media_source::MEDIA_SOURCE_COMMAND_END:
      // Intentional fallthrough
    case media_source::MEDIA_SOURCE_COMMAND_STOP: {
      if (ctx.generation_state == ColorNoiseGenerationState::GENERATING) {
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

media_source::MediaSourceCapabilities ColorNoiseMediaSource::get_capabilities() {
  media_source::MediaSourceCapabilities caps;
  caps.supports_pause = true;  // Noise generation can be paused
  return caps;
}

void ColorNoiseMediaSource::generate_white_noise_samples(int16_t *samples, size_t sample_count, uint32_t &prng_state,
                                                         int32_t amplitude) {
  for (size_t i = 0; i < sample_count; i++) {
    uint32_t random = xorshift32(prng_state);
    samples[i] = static_cast<int16_t>((static_cast<int32_t>(random) * amplitude) >> 15);
  }
}

void ColorNoiseMediaSource::generate_brown_noise_samples(int16_t *samples, size_t sample_count, uint32_t &prng_state,
                                                         int32_t &y_accumulator, int32_t leakage, int32_t scaling,
                                                         int32_t amplitude) {
  for (size_t i = 0; i < sample_count; i++) {
    // Generate white noise
    int32_t white = static_cast<int16_t>(xorshift32(prng_state) >> 16);

    // z = leakage * y + white * scaling (all Q15)
    int32_t z = ((leakage * y_accumulator) >> 15) + ((white * scaling) >> 15);

    // Check if |z| > 1.0 (in Q15, that's > 32767)
    int32_t abs_z = (z < 0) ? -z : z;

    if (abs_z > 32767) {
      // Reflection: reverse direction to prevent clipping
      y_accumulator = ((leakage * y_accumulator) >> 15) - ((white * scaling) >> 15);
    } else {
      y_accumulator = z;
    }

    // Apply amplitude and clamp
    int32_t result = (y_accumulator * amplitude) >> 15;
    samples[i] =
        static_cast<int16_t>(std::clamp(result, static_cast<int32_t>(INT16_MIN), static_cast<int32_t>(INT16_MAX)));
  }
}

void ColorNoiseMediaSource::initialize_brown_coefficients(uint32_t sample_rate, int32_t &leakage, int32_t &scaling) {
  // Double precision is unnecessary, but avoids single precision so the calling task isn't locked to its current CPU
  // core on an ESP32

  // Calculate leakage coefficient (high-pass filter to prevent DC drift)
  double leakage_f = (sample_rate - 144.0) / sample_rate;
  if (leakage_f >= 0.9999) {
    leakage_f = 0.9999;
  }
  leakage = static_cast<int32_t>(std::round(leakage_f * 32768.0));

  // Calculate scaling coefficient (compensates for sample rate)
  double scaling_f = 9.0 / sqrt(static_cast<double>(sample_rate));
  if (scaling_f <= 0.01) {
    scaling_f = 0.01;
  }
  scaling = static_cast<int32_t>(std::round(scaling_f * 32768.0));
}

void ColorNoiseMediaSource::generate_pink_noise_samples(int16_t *samples, size_t sample_count, uint32_t &prng_state,
                                                        std::array<int32_t, 7> &buffers, int32_t amplitude) {
  // scale by normalization factor 0.129f in Q15
  amplitude = (amplitude * 4227) >> 15;

  for (size_t i = 0; i < sample_count; i++) {
    // Generate white noise in Q15 format
    int32_t white = static_cast<int16_t>(xorshift32(prng_state) >> 16);

    // Update Paul Kellett's 6 filters (all in Q15)
    buffers[0] = ((buffers[0] * 32730) >> 15) + ((white * 1820) >> 15);
    buffers[1] = ((buffers[1] * 32552) >> 15) + ((white * 2460) >> 15);
    buffers[2] = ((buffers[2] * 31752) >> 15) + ((white * 5038) >> 15);
    buffers[3] = ((buffers[3] * 28393) >> 15) + ((white * 10175) >> 15);
    buffers[4] = ((buffers[4] * 18022) >> 15) + ((white * 17464) >> 15);
    buffers[5] = ((buffers[5] * -24961) >> 15) + ((white * -553) >> 15);

    // Sum all filter outputs + differentiator + scaled white
    int32_t pink = buffers[0] + buffers[1] + buffers[2] + buffers[3] + buffers[4] + buffers[5] + buffers[6] +
                   ((white * 17569) >> 15);

    // Update differentiator for next iteration
    buffers[6] = (white * 3798) >> 15;

    // Apply amplitude and clamp
    int32_t result = (pink * amplitude) >> 15;
    // Clamp to int16_t range
    samples[i] =
        static_cast<int16_t>(std::clamp(result, static_cast<int32_t>(INT16_MIN), static_cast<int32_t>(INT16_MAX)));
  }
}

void ColorNoiseMediaSource::generate_task(void *params) {
  auto *task_params = static_cast<GenerateTaskParams *>(params);
  ColorNoiseMediaSource *this_source = task_params->source;
  size_t pipeline = task_params->pipeline;
  delete task_params;

  auto &ctx = this_source->color_noise_pipelines_[pipeline];

  {
    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STARTING);

    // Create AudioStreamInfo with 1 channel (mono)
    audio::AudioStreamInfo stream_info(16, 1, this_source->sample_rate_);

    ESP_LOGD(TAG, "Pipeline %zu: Bits per sample: %d, Channels: %d, Sample rate: %u", pipeline,
             stream_info.get_bits_per_sample(), stream_info.get_channels(), stream_info.get_sample_rate());

    // Check if callback is set before using it
    if (!this_source->output_callback_) {
      ESP_LOGE(TAG,
               "Pipeline %zu: Output callback is not set! Make sure the ColorNoiseMediaSource is added to "
               "media_sources in your YAML config",
               pipeline);
      xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPED);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    // Create output transfer buffer sized to store READ_WRITE_TIMEOUT_MS of audio
    size_t buffer_size = stream_info.ms_to_bytes(READ_WRITE_TIMEOUT_MS);
    std::unique_ptr<audio::AudioSinkTransferBuffer> output_buffer = audio::AudioSinkTransferBuffer::create(buffer_size);
    if (!output_buffer) {
      ESP_LOGE(TAG, "Pipeline %zu: Failed to allocate output transfer buffer", pipeline);
      xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPED);
      while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    // Wrap the output callback to include stream info and pipeline
    std::function<size_t(uint8_t *, size_t, TickType_t)> wrapped_callback =
        [this_source, stream_info, pipeline](uint8_t *data, size_t len, TickType_t ticks) {
          return this_source->output_callback_(data, len, ticks, stream_info, pipeline);
        };
    output_buffer->set_sink(std::move(wrapped_callback));

    // Initialize PRNG state with seed
    uint32_t prng_state = ctx.seed;
    if (prng_state == 0) {
      // Ensure we don't have a zero state (xorshift32 doesn't work with zero)
      prng_state = 0xDEADBEEF;
    }

    // Initialize noise-specific state
    if (ctx.noise_type == NoiseType::BROWN) {
      ctx.brown_y_accumulator = 0;
      initialize_brown_coefficients(stream_info.get_sample_rate(), ctx.brown_leakage, ctx.brown_scaling);
    } else if (ctx.noise_type == NoiseType::PINK) {
      for (size_t i = 0; i < 7; i++) {
        ctx.pink_buffers[i] = 0;
      }
    }

    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_RUNNING);

    // Main generation loop
    while (true) {
      EventBits_t event_bits = xEventGroupGetBits(ctx.event_group);

      if (event_bits & EventGroupBits::COMMAND_STOP) {
        break;
      }

      // Check if we've generated enough samples for the requested duration
      bool generation_complete =
          (ctx.total_samples_to_generate > 0) && (ctx.samples_generated >= ctx.total_samples_to_generate);

      if (generation_complete && output_buffer->available() == 0) {
        // All samples generated and buffer is empty, stop cleanly
        ESP_LOGD(TAG, "Pipeline %zu: Duration complete, %zu samples generated", pipeline, ctx.samples_generated);
        break;
      }

      // Skip generation when paused but continue running
      if (!(event_bits & EventGroupBits::COMMAND_PAUSE)) {
        // Only generate more samples if we haven't reached the duration limit
        if (!generation_complete) {
          // Fill the output buffer with noise samples
          size_t bytes_to_generate = output_buffer->free();

          // Limit bytes to generate if we're close to the duration limit
          if (ctx.total_samples_to_generate > 0) {
            size_t samples_remaining = ctx.total_samples_to_generate - ctx.samples_generated;
            size_t bytes_remaining = stream_info.samples_to_bytes(samples_remaining);
            if (bytes_to_generate > bytes_remaining) {
              bytes_to_generate = bytes_remaining;
            }
          }

          if (bytes_to_generate > 0) {
            // Generate noise samples directly into the transfer buffer based on noise type
            int16_t *samples = reinterpret_cast<int16_t *>(output_buffer->get_buffer_end());
            size_t sample_count = bytes_to_generate / sizeof(int16_t);

            switch (ctx.noise_type) {
              case NoiseType::WHITE:
                generate_white_noise_samples(samples, sample_count, prng_state, ctx.amplitude_q15);
                break;
              case NoiseType::BROWN:
                generate_brown_noise_samples(samples, sample_count, prng_state, ctx.brown_y_accumulator,
                                             ctx.brown_leakage, ctx.brown_scaling, ctx.amplitude_q15);
                break;
              case NoiseType::PINK:
                generate_pink_noise_samples(samples, sample_count, prng_state, ctx.pink_buffers, ctx.amplitude_q15);
                break;
            }
            output_buffer->increase_buffer_length(bytes_to_generate);

            // Track the number of samples generated
            ctx.samples_generated += stream_info.bytes_to_samples(bytes_to_generate);
          }
        }

        // Transfer data from buffer to sink (never shift to avoid unnecessary data moves)
        output_buffer->transfer_data_to_sink(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS), false);
      } else {
        // Paused - sleep to avoid busy waiting
        vTaskDelay(pdMS_TO_TICKS(READ_WRITE_TIMEOUT_MS));
      }
    }
    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPING);
  }
  xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPED);
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace color_noise
}  // namespace esphome
