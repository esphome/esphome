#include "sendspin_media_source.h"

#include "esphome/core/application.h"

#include <cstdlib>

namespace esphome {
namespace sendspin {

/*
 * General todo list
 *  - Reuse output transfer buffer instead of having sendspin_decoder allocate a new output each time
 *  - Clean up the stream infos, its duplicated in multiple places and is probably not necessary
 *  - Review all elements of SyncContext, I doubt we need all of them
 *  - Think about function signatures for the sync_task helpers, especially their return types
 *  - Improve hard sync behavior
 *    - We should probably throw away chunks if they are going to start in the next 500 ms (we can even measure how long
 *      it takes to get data through the pipeline, so this should just be an initial guess that we update for future
 *      starts)
 *    - When we get too far ahead, we get into a doom loop as we only send 1 sample at a time. At that point, the i2s
 *      speaker starts to disable and enable itself to stay in sync. This overhead makes it so that we never catch up
 *    - Can we do better than the current error when we synchronize? Right now we find our current error and account for
 *      the pending corrections. Is this a full prediction of when this next chunk will play out or is this something
 *      else? If it isn't, can we do a better prediction?
 *  - Review all commented out code, most of it can be thrown away
 *  - Think about how we should stop. Think of the difference between stopping from the sendspin server vs stopping from
 *    a user request. Does the media player need to be aware at all in the former case?
 *  - Review all TODOs in the code
 */

// TODO: Remove this. Take out unnecessary logs and change useful ones to be VERBOSE level
#define SENDSPIN_MEDIA_SOURCE_DEBUG

// TODO: Determine a default value - try seeing how many chunks of FLAC the server can send at the start
static const uint32_t ENCODED_CHUNK_QUEUE_SIZE = 100;

static const uint32_t READ_WRITE_TIMEOUT_MS = 20;

static const int GOOD_SYNCS_BEFORE_UNMUTE = 1;
static const int64_t HARD_SYNC_THRESHOLD_US = 5000;
static const int64_t HARD_RESYNC_THRESHOLD_US = 500;
static const int64_t SOFT_SYNC_THRESHOLD_US = 100;

static const uint32_t INITIAL_SYNC_ZEROS_DURATION_MS = 25;

static const UBaseType_t SYNC_TASK_PRIORITY = 5;
static const size_t SYNC_TASK_STACK_SIZE = 5632;  // Opus uses more stack than FLAC

static const char *const TAG = "sendspin_media_source";

enum class SourceControls : uint8_t {
  START = 0,
  STOP = 1,
  COMMAND = 2,
};

struct ControlMessage {
  SourceControls control;
  media_source::MediaSourceCommand command;
};

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  COMMAND_PAUSE = (1 << 1),
  TASK_STARTING = (1 << 7),
  TASK_RUNNING = (1 << 8),
  TASK_STOPPING = (1 << 9),
  TASK_STOPPED = (1 << 10),
};

void SendspinMediaSource::init_pipelines(size_t pipeline_count) {
  media_source::MediaSource::init_pipelines(pipeline_count);

  this->sendspin_pipelines_.init(pipeline_count);
  for (size_t i = 0; i < pipeline_count; i++) {
    SendspinMediaSourcePipeline ctx;

    // Create event group and queue upfront so they're available when play_uri is called
    ctx.event_group = xEventGroupCreate();
    ctx.controls_queue = xQueueCreate(3, sizeof(ControlMessage));

    ctx.playback_progress_queue = xQueueCreateWithCaps(50, sizeof(PlaybackProgress), MALLOC_CAP_SPIRAM);
    if (ctx.playback_progress_queue == nullptr) {
      // TODO: Need to actually make this function failable
      ESP_LOGE(TAG, "Couldn't create playback progress queue.");
      this->mark_failed();
    }

    // TODO: Get max buffer size from hub
    // (initial queue size, buffer size, dynamic growth enabled, max queue length)
    ctx.encoded_chunk_queue = audio::AudioChunkQueue::create(ENCODED_CHUNK_QUEUE_SIZE, 0, true, 0);
    if (ctx.encoded_chunk_queue == nullptr) {
      // TODO: Need to actually make this function failable
      ESP_LOGE(TAG, "Couldn't create chunk queue.");
      this->mark_failed();
    }

    this->sendspin_pipelines_.push_back(std::move(ctx));
  }
}

bool SendspinMediaSource::play_uri(const std::string &uri, size_t pipeline) {
  if (pipeline >= this->sendspin_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return false;
  }

  // Check if pipeline is already playing
  if (this->get_state(pipeline) != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s' on pipeline %zu: pipeline is busy", uri.c_str(), pipeline);
    return false;
  }

  // Validate URI starts with "sendspin://"
  if (uri.find("sendspin://") != 0) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  // Strip "sendspin://" prefix and find the file
  std::string sendspin_id = uri.substr(11);  // "sendspin://" is 11 characters

  if (!this->is_ready() || this->is_failed()) {
    return false;
  }

  auto &ctx = this->sendspin_pipelines_[pipeline];

  // Queue playback start
  ControlMessage message = {.control = SourceControls::START};
  xQueueSend(ctx.controls_queue, &message, 0);
  this->enable_loop_soon_any_context();

#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif

  return true;
}

void SendspinMediaSource::setup() {
  this->disable_loop();

  // Register callbacks for volume related controls from the Sendspin
  this->parent_->add_controls_callback([this](const SendspinControls &control_type) {
    switch (control_type) {
      case SendspinControls::START:  // Intentional fallthrough
        // TODO: This should be pipeline specific, not hardcoded to pipelone 0
        this->sendspin_pipelines_[0].pending_start = true;
        this->play_uri_request_callback_("sendspin://test", 0);
        break;
      case SendspinControls::STOP: {
        // TODO: Is there really a distinction here btween STOP and CLEAR? I guess clear assumes it will resume again
        // soon, but does that effect anything in the media source?
        // The original plan was to tell the media player to tell us to stop, but then we can't tell if the STOP command
        // originated from us (so just clear things) or if it came from the user (so tell the server to stop) For now,
        // we stop locally and the media player is aware through the state update. Not a huge deal, but this does mean
        // that we can't directly to the speaker to stop so they can clear there own buffers.

        // Intentional fallthrough
      }
      case SendspinControls::CLEAR: {
        // TODO: This should be pipeline specific, not hardcoded to pipelone 0
        auto &ctx = this->sendspin_pipelines_[0];
        xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_STOP);
        ctx.encoded_chunk_queue->reset();  // Reset the encoded chunk queue to clear it out
        break;
      }
      case SendspinControls::VOLUME_UPDATE: {
        this->volume_request_callback_(this->parent_->get_volume() / 100.0f);
        break;
      }
      case SendspinControls::MUTE_UPDATE: {
        this->mute_request_callback_(this->parent_->get_muted());
        break;
      }
      default:
        break;
    }
  });

  // TODO: This always sends it to pipeline 0!
  this->parent_->add_audio_chunk_callback([this](std::shared_ptr<SendspinAudioChunk> audio_chunk,
                                                 TickType_t ticks_to_wait, const audio::AudioStreamInfo &stream_info) {
    this->sendspin_pipelines_[0].stream_info = stream_info;

    // AudioChunkQueue handles shared_ptr directly
    return this->sendspin_pipelines_[0].encoded_chunk_queue->add_chunk(
        std::static_pointer_cast<audio::AudioChunk>(audio_chunk), ticks_to_wait);
  });

  // Pipeline initialization happens via init_pipelines() called by MediaPlayer
  // Individual pipeline resources (event groups, queues, tasks) are created on-demand in loop()
}

void SendspinMediaSource::loop() {
  // Process each pipeline's state machine
  for (size_t pipeline = 0; pipeline < this->sendspin_pipelines_.size(); pipeline++) {
    auto &ctx = this->sendspin_pipelines_[pipeline];

    // Process control messages for this pipeline
    ControlMessage incoming_control;
    if (xQueueReceive(ctx.controls_queue, &incoming_control, 0)) {
      switch (incoming_control.control) {
        case SourceControls::START:
          ctx.generation_state = SendspinGenerationState::START_TASK;
          ctx.pending_start = false;
          break;
        case SourceControls::STOP:
          if (ctx.generation_state == SendspinGenerationState::GENERATING) {
            // this->parent_->send_client_command(SendspinCommandType::STOP, std::nullopt, std::nullopt);
            xEventGroupSetBits(ctx.event_group, EventGroupBits::COMMAND_STOP);
          }
          break;
      }
    }

    // Process pipeline state machine
    switch (ctx.generation_state) {
      case SendspinGenerationState::START_TASK: {
        // Event group and queue already created in init_pipelines()
        // Start the task
        if (ctx.sync_task_handle == nullptr) {
          xEventGroupClearBits(ctx.event_group, EventGroupBits::TASK_STARTING | EventGroupBits::TASK_RUNNING |
                                                    EventGroupBits::TASK_STOPPING | EventGroupBits::TASK_STOPPED |
                                                    EventGroupBits::COMMAND_STOP);
          this->parent_->update_state(SendspinClientState::SYNCHRONIZED);
          if (ctx.sync_task_stack_buffer == nullptr) {
            if (this->task_stack_in_psram_) {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
              ctx.sync_task_stack_buffer = stack_allocator.allocate(SYNC_TASK_STACK_SIZE);
            } else {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
              ctx.sync_task_stack_buffer = stack_allocator.allocate(SYNC_TASK_STACK_SIZE);
            }
          }
          if (ctx.sync_task_stack_buffer == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate generate task stack for pipeline %zu", pipeline);
            this->mark_failed();
            return;
          }

          char task_name[32];
          snprintf(task_name, sizeof(task_name), "Sendspin_%zu", pipeline);

          auto *params = new GenerateTaskParams{this, pipeline};
          ctx.sync_task_handle = xTaskCreateStatic(sync_task, task_name, SYNC_TASK_STACK_SIZE, params, 1,
                                                   ctx.sync_task_stack_buffer, &ctx.sync_task_stack);
          if (ctx.sync_task_handle == nullptr) {
            ESP_LOGE(TAG, "Failed to create generate task for pipeline %zu", pipeline);
            delete params;
            this->mark_failed();
            return;
          }
        }
        ESP_LOGD(TAG, "Started generate task for pipeline %zu", pipeline);
        ctx.generation_state = SendspinGenerationState::GENERATING;
        break;
      }
      case SendspinGenerationState::GENERATING: {
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
          xEventGroupClearBits(ctx.event_group, TASK_STOPPED | COMMAND_STOP);

          vTaskDelete(ctx.sync_task_handle);
          ctx.sync_task_handle = nullptr;
          if (ctx.sync_task_stack_buffer != nullptr) {
            if (this->task_stack_in_psram_) {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
              stack_allocator.deallocate(ctx.sync_task_stack_buffer, SYNC_TASK_STACK_SIZE);
            } else {
              RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
              stack_allocator.deallocate(ctx.sync_task_stack_buffer, SYNC_TASK_STACK_SIZE);
            }
            ctx.sync_task_stack_buffer = nullptr;
          }
          this->set_state_(media_source::MediaSourceState::IDLE, pipeline);
          ctx.generation_state = SendspinGenerationState::IDLE;
        }
        break;
      }
      case SendspinGenerationState::IDLE: {
        // Nothing to do when idle
        break;
      }
    }
  }

  // Check if we should disable loop when all pipelines are idle
  bool all_idle = true;
  for (const auto &p : this->sendspin_pipelines_) {
    if ((p.generation_state != SendspinGenerationState::IDLE) || uxQueueMessagesWaiting(p.controls_queue) > 0) {
      all_idle = false;
      break;
    }
  }
  if (all_idle) {
    this->disable_loop();
  }
}

void SendspinMediaSource::handle_command(media_source::MediaSourceCommand command, size_t pipeline) {
  if (pipeline >= this->sendspin_pipelines_.size()) {
    ESP_LOGE(TAG, "Invalid pipeline index: %zu", pipeline);
    return;
  }

  auto &ctx = this->sendspin_pipelines_[pipeline];
  if (ctx.controls_queue == nullptr) {
    return;
  }

  ControlMessage message;
  switch (command) {
    case media_source::MEDIA_SOURCE_COMMAND_END: {
      if (ctx.generation_state == SendspinGenerationState::GENERATING) {
        message.control = SourceControls::STOP;
        xQueueSend(ctx.controls_queue, &message, 0);
      }
      if (!ctx.pending_start) {
        // TODO: This is a hack to stop us from disconnecting from the server because the media player asked us to stop
        // only for us to start again
        this->parent_->update_state(SendspinClientState::EXTERNAL_SOURCE);
        // this->parent_->disconnect_from_server(SendspinGoodbyeReason::ANOTHER_SERVER);
      }
      break;
    }
    case media_source::MEDIA_SOURCE_COMMAND_PLAY:
      this->parent_->send_client_command(SendspinCommandType::PLAY, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_PAUSE:
      this->parent_->send_client_command(SendspinCommandType::PAUSE, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_STOP:
      this->parent_->send_client_command(SendspinCommandType::STOP, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_NEXT:
      this->parent_->send_client_command(SendspinCommandType::NEXT, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_PREVIOUS:
      this->parent_->send_client_command(SendspinCommandType::PREVIOUS, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_REPEAT_ALL:
      this->parent_->send_client_command(SendspinCommandType::REPEAT_ALL, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_REPEAT_ONE:
      this->parent_->send_client_command(SendspinCommandType::REPEAT_ONE, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_REPEAT_OFF:
      this->parent_->send_client_command(SendspinCommandType::REPEAT_OFF, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_SHUFFLE:
      this->parent_->send_client_command(SendspinCommandType::SHUFFLE, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_UNSHUFFLE:
      this->parent_->send_client_command(SendspinCommandType::UNSHUFFLE, std::nullopt, std::nullopt);
      break;
    case media_source::MEDIA_SOURCE_COMMAND_GROUP_JOIN:
      this->parent_->send_client_command(SendspinCommandType::SWITCH, std::nullopt, std::nullopt);
      break;
    default: {
      // message.control = SourceControls::COMMAND;
      // message.command = command;
      // xQueueSend(ctx.controls_queue, &message, 0);
      // break;
      break;
    }
  }

#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif
}

void SendspinMediaSource::notify_volume_changed(float volume) {
  this->parent_->update_volume(std::roundf(volume * 100.0f));
}

void SendspinMediaSource::notify_mute_changed(bool is_muted) { this->parent_->update_muted(is_muted); }

void SendspinMediaSource::notify_audio_played(uint32_t frames, int64_t timestamp, size_t pipeline) {
  PlaybackProgress playback_progress = {.frames_played = frames, .finish_timestamp = timestamp};
  if (!xQueueSend(this->sendspin_pipelines_[pipeline].playback_progress_queue, &playback_progress, 0)) {
    ESP_LOGE(TAG, "Playback info queue was full");
  }
}

media_source::MediaSourceCapabilities SendspinMediaSource::get_capabilities() {
  media_source::MediaSourceCapabilities caps;
  caps.supports_pause = true;
  caps.supports_volume_control = true;
  caps.has_internal_playlist = true;

  return caps;
}

bool SendspinMediaSource::sync_transfer_audio_(SyncContext &sync_context) {
  const uint32_t duration_in_transfer_buffers = sync_context.current_stream_info.bytes_to_ms(
      sync_context.output_transfer_buffer->available() + sync_context.interpolation_transfer_buffer->available());

  size_t bytes_written = sync_context.interpolation_transfer_buffer->transfer_data_to_sink(
      pdMS_TO_TICKS(duration_in_transfer_buffers / 2), false);

  if ((bytes_written > 0) && sync_context.initial_decode) {
    // Sent initial zeros, delay slightly to give it some time to work through the audio stack
    vTaskDelay(pdMS_TO_TICKS(sync_context.current_stream_info.bytes_to_ms(bytes_written) / 2));
  }

  if (sync_context.interpolation_transfer_buffer->available() == 0) {
    // No interpolation bytes available, send main audio data
    sync_context.output_transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(3 * duration_in_transfer_buffers / 2),
                                                               false);
  }

  if ((sync_context.output_transfer_buffer->available() == 0) && (sync_context.decoded_chunk != nullptr) &&
      sync_context.release_chunk) {
    sync_context.decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
    sync_context.release_chunk = false;
    // printf("released chunk\n");
  }

  if (sync_context.interpolation_transfer_buffer->available() + sync_context.output_transfer_buffer->available() > 0) {
    // Some audio still needs to be sent, loop back around
    return false;
  }

  return true;
}

bool SendspinMediaSource::sync_load_next_chunk_(SyncContext &sync_context,
                                                SendspinMediaSourcePipeline &pipeline_context) {
  if (sync_context.encoded_chunk == nullptr) {
    auto chunk = pipeline_context.encoded_chunk_queue->receive_chunk(pdMS_TO_TICKS(15));
    if (!chunk) {
      // No chunk available to process
      return false;
    }
    sync_context.encoded_chunk = std::static_pointer_cast<SendspinAudioChunk>(chunk);
  }

  // if (esp_timer_get_time() - sync_context.encoded_chunk->timestamp > 0) {
  //   // Chunk was already supposed to play, skip it!
  //   ESP_LOGE(TAG, "Chunk was already supposed to play at %" PRId64 " and its %" PRId64 ", so skipping it",
  //            sync_context.encoded_chunk->timestamp, esp_timer_get_time());
  //   sync_context.encoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
  //   sync_context.release_chunk = false;
  //   pipeline_context.encoded_chunk_queue->reset();  // We are way behind, so drop any pending audio
  //   return false;
  // }

  return true;
}

bool SendspinMediaSource::sync_determine_raw_sync_error_(SyncContext &sync_context,
                                                         SendspinMediaSourcePipeline &pipeline_context) {
  PlaybackProgress playback_progress;

  int64_t finish_timestamp = 0;
  while (!sync_context.chunk_timings.empty() &&
         (xQueueReceive(pipeline_context.playback_progress_queue, &playback_progress, 0) == pdTRUE)) {
    uint32_t frames_played = playback_progress.frames_played;

    if (frames_played && sync_context.initial_decode) {
      // Some sent audio chunks have now been played by the speaker
      sync_context.initial_decode = false;
    }

    finish_timestamp = playback_progress.finish_timestamp;
    InternalAudioTiming *front_chunk = &sync_context.chunk_timings.front();

    sync_context.pending_frame_corrections -= front_chunk->frame_corrections;
    front_chunk->frame_corrections = 0;

    while (front_chunk->total_frames < frames_played) {
      frames_played -= front_chunk->total_frames;

      sync_context.chunk_timings.pop_front();
      if (sync_context.chunk_timings.empty()) {
        // This should never happen if the output speaker was fully stopped with all audio
        break;
      }
      front_chunk = &sync_context.chunk_timings.front();

      sync_context.pending_frame_corrections -= front_chunk->frame_corrections;
      front_chunk->frame_corrections = 0;
    }

    // Now we are in the middle of the current audio chunk
    if (sync_context.chunk_timings.empty()) {
      // Catastrophic error, queue a stop and start control
      SendspinControls control_type = SendspinControls::STOP;
      xQueueSend(pipeline_context.controls_queue, &control_type, portMAX_DELAY);
      control_type = SendspinControls::START;
      xQueueSend(pipeline_context.controls_queue, &control_type, portMAX_DELAY);
      ESP_LOGE(TAG, "Catastrophic sync error. Restarting sync task");
    } else {
      sync_context.chunk_timings.front().total_frames -= frames_played;
    }
  }
  if (!sync_context.chunk_timings.empty() && (finish_timestamp != 0)) {
    uint32_t unplayed_frames = sync_context.chunk_timings.front().total_frames;

    int64_t unplayed_ms = sync_context.current_stream_info.frames_to_milliseconds_with_remainder(&unplayed_frames);
    int64_t unplayed_us =
        1000LL * unplayed_ms +
        static_cast<int64_t>(sync_context.current_stream_info.frames_to_microseconds(unplayed_frames));

    int64_t timestamp_finished = sync_context.chunk_timings.front().timestamp - unplayed_us;

    sync_context.last_error = timestamp_finished - finish_timestamp;
  }

  return sync_context.last_error.has_value() || sync_context.initial_decode;
}

bool SendspinMediaSource::sync_determine_predicted_error_(SyncContext &sync_context) {
  int64_t signed_pending_duration_corrections =
      (sync_context.pending_frame_corrections * 1000000LL) /
      static_cast<int64_t>(sync_context.current_stream_info.get_sample_rate());

  // Takes into account the pending error
  sync_context.recent_error_us = sync_context.last_error.value_or(0) - signed_pending_duration_corrections;

  if (abs(sync_context.last_error.value_or(0)) < HARD_SYNC_THRESHOLD_US) {
    sync_context.synced_chunks = std::min(sync_context.synced_chunks + 1, GOOD_SYNCS_BEFORE_UNMUTE);
    sync_context.temporary_hard_sync_threshold = HARD_SYNC_THRESHOLD_US;  // go back to large threshold
  } else if (abs(sync_context.recent_error_us) > HARD_SYNC_THRESHOLD_US) {
    // Even with the upcoming adjustments we are out of sync, reset the count
    sync_context.synced_chunks = 0;
    sync_context.temporary_hard_sync_threshold = HARD_RESYNC_THRESHOLD_US;
  }

  return true;
}

void SendspinMediaSource::sync_hard_sync_add_silence_(SyncContext &sync_context,
                                                      SendspinMediaSourcePipeline &pipeline_context) {
  // Audio hasn't started or we are too far ahead, so insert many zeros

  // Keep the chunk for later processing (don't release yet)
  sync_context.release_chunk = false;

  // Remove any new chunk data from the transfer buffer and zero out the transfer buffer
  sync_context.interpolation_transfer_buffer->decrease_buffer_length(
      sync_context.interpolation_transfer_buffer->available());
  const size_t zeroed_bytes = sync_context.interpolation_transfer_buffer->free();
  std::memset((void *) sync_context.interpolation_transfer_buffer->get_buffer_end(), 0, zeroed_bytes);

  size_t silence_bytes_for_correction =
      sync_context.current_stream_info.ms_to_bytes(static_cast<uint32_t>(abs(sync_context.recent_error_us)) / 1000);

  if (silence_bytes_for_correction < zeroed_bytes) {
    // Silencing this chunk will get us precisely in sync, so correct in microseconds
    const uint32_t frames_to_silence =
        (abs(sync_context.recent_error_us) * sync_context.current_stream_info.get_sample_rate()) / 1000000;
    silence_bytes_for_correction = sync_context.current_stream_info.frames_to_bytes(frames_to_silence);
  }

  size_t actual_bytes_of_silence = std::min(silence_bytes_for_correction, zeroed_bytes);
  if (sync_context.initial_decode) {
    // Always send a full set of zeros when starting a new stream
    actual_bytes_of_silence = zeroed_bytes;
  }
  sync_context.interpolation_transfer_buffer->increase_buffer_length(actual_bytes_of_silence);
  int32_t frame_corrections = sync_context.current_stream_info.bytes_to_frames(actual_bytes_of_silence);
#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
  ESP_LOGD(TAG,
           "Hard sync: adding %" PRId32 " frames of silence. Current error is %" PRId64 "us. There are %" PRId64
           " pending frames for correction, and the deque has %zu entries",
           frame_corrections, sync_context.recent_error_us, sync_context.pending_frame_corrections,
           sync_context.chunk_timings.size());
#endif
  ++pipeline_context.hard_sync_added_frames_;

  InternalAudioTiming timings;

  timings.timestamp = sync_context.decoded_chunk->timestamp;
  timings.total_frames = frame_corrections;
  timings.frame_corrections = frame_corrections;
  sync_context.pending_frame_corrections += frame_corrections;

  sync_context.chunk_timings.push_back(timings);
  sync_context.last_error.reset();  // We're accounted for the most recent error
}

void SendspinMediaSource::sync_hard_sync_remove_audio_(SyncContext &sync_context,
                                                       SendspinMediaSourcePipeline &pipeline_context,
                                                       InternalAudioTiming &timings, int32_t &frame_corrections) {
  // Hard sync because we have gotten ahead and need to skip some audio to get in sync
  // Removes newly decoded frames (but will always leave a minimum of 1 frame)

  size_t bytes_to_remove = sync_context.current_stream_info.ms_to_bytes(abs(sync_context.recent_error_us) / 1000);
  if (bytes_to_remove < sync_context.decoded_chunk->get_usable_size() - sync_context.bytes_per_frame) {
    // Trimming this chunk will get us precisely in sync, so correct in microseconds
    const uint32_t frames_to_remove =
        (abs(sync_context.recent_error_us) * sync_context.current_stream_info.get_sample_rate()) / 1000000;
    bytes_to_remove = sync_context.current_stream_info.frames_to_bytes(frames_to_remove);
  }

  size_t actual_bytes_to_remove =
      std::min(bytes_to_remove, sync_context.decoded_chunk->get_usable_size() - sync_context.bytes_per_frame);

  sync_context.output_transfer_buffer->decrease_buffer_length(actual_bytes_to_remove);

  // TODO: Is this right? Coudln't I just use get_buffer_start?
  size_t bytes_to_silence = sync_context.decoded_chunk->get_usable_size() - actual_bytes_to_remove;
  std::memset((void *) (sync_context.output_transfer_buffer->get_buffer_end() - bytes_to_silence), 0, bytes_to_silence);

  frame_corrections = -sync_context.current_stream_info.bytes_to_frames(actual_bytes_to_remove);

  uint32_t total_frames_kept =
      sync_context.current_stream_info.bytes_to_frames(sync_context.decoded_chunk->get_usable_size()) +
      frame_corrections;
#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
  ESP_LOGD(TAG,
           "Hard sync: removing %" PRId32 " frames and keeping %" PRIu32 " frames. Current error is %" PRId64
           "us. There are %" PRId64 " pending frames for correction, and the deque has %zu entries",
           frame_corrections, total_frames_kept, sync_context.recent_error_us, sync_context.pending_frame_corrections,
           sync_context.chunk_timings.size());
#endif
  ++pipeline_context.hard_sync_removed_frames_;
}

void SendspinMediaSource::sync_soft_sync_remove_audio_(SyncContext &sync_context,
                                                       SendspinMediaSourcePipeline &pipeline_context,
                                                       InternalAudioTiming &timings, int32_t &frame_corrections) {
  // Small sync adjustment after getting slightly ahead.
  // Removes the last frame in the chunk to get in sync. The second to last frame is replaced with the average
  // of it and the removed frame to minimize audible glitches.

  const uint32_t num_channels = sync_context.current_stream_info.get_channels();
  const uint32_t bytes_per_sample = sync_context.bytes_per_frame / num_channels;

  if (sync_context.output_transfer_buffer->available() >= 2 * sync_context.bytes_per_frame) {
    for (uint32_t chan = 0; chan < num_channels; ++chan) {
      const int32_t first_sample =
          audio::unpack_audio_sample_to_q31(sync_context.output_transfer_buffer->get_buffer_end() -
                                                2 * sync_context.bytes_per_frame + chan * bytes_per_sample,
                                            bytes_per_sample);
      const int32_t second_sample =
          audio::unpack_audio_sample_to_q31(sync_context.output_transfer_buffer->get_buffer_end() -
                                                sync_context.bytes_per_frame + chan * bytes_per_sample,
                                            bytes_per_sample);
      int32_t replacement_sample = (first_sample + second_sample) / 2;
      audio::pack_q31_as_audio_sample(replacement_sample,
                                      sync_context.output_transfer_buffer->get_buffer_end() -
                                          2 * sync_context.bytes_per_frame + chan * bytes_per_sample,
                                      bytes_per_sample);
    }

    sync_context.output_transfer_buffer->decrease_buffer_length(sync_context.bytes_per_frame);
    frame_corrections = -1;
    ++pipeline_context.single_frames_removed_;
  }
}

void SendspinMediaSource::sync_soft_sync_add_audio_(SyncContext &sync_context,
                                                    SendspinMediaSourcePipeline &pipeline_context,
                                                    InternalAudioTiming &timings, int32_t &frame_corrections) {
  // Small sync adjustment after getting slightly behind.
  // Adds one new frame to get in sync. The new frame is inserted between the first and second frames.
  // The new frame is the average of the first two frames in the chunk to minimize audible glitches.

  if ((sync_context.interpolation_transfer_buffer->free() >= sync_context.bytes_per_frame) &&
      (sync_context.output_transfer_buffer->available() >= 2 * sync_context.bytes_per_frame)) {
    const uint32_t num_channels = sync_context.current_stream_info.get_channels();
    const uint32_t bytes_per_sample = sync_context.bytes_per_frame / num_channels;

    for (uint32_t chan = 0; chan < num_channels; ++chan) {
      const int32_t first_sample = audio::unpack_audio_sample_to_q31(
          sync_context.output_transfer_buffer->get_buffer_start() + chan * bytes_per_sample, bytes_per_sample);
      const int32_t second_sample =
          audio::unpack_audio_sample_to_q31(sync_context.output_transfer_buffer->get_buffer_start() +
                                                chan * bytes_per_sample + sync_context.bytes_per_frame,
                                            bytes_per_sample);
      int32_t new_sample = (first_sample + second_sample) / 2;
      audio::pack_q31_as_audio_sample(new_sample,
                                      sync_context.output_transfer_buffer->get_buffer_start() + chan * bytes_per_sample,
                                      bytes_per_sample);
      audio::pack_q31_as_audio_sample(
          first_sample, sync_context.interpolation_transfer_buffer->get_buffer_start() + chan * bytes_per_sample,
          bytes_per_sample);
    }
    sync_context.interpolation_transfer_buffer->increase_buffer_length(sync_context.bytes_per_frame);
    frame_corrections = 1;
    ++pipeline_context.single_frames_added_;
  }
}

bool SendspinMediaSource::sync_decode_audio_(SyncContext &sync_context, SendspinMediaSourcePipeline &pipeline_context) {
  if (sync_context.decoded_chunk != nullptr) {
    // Already have a decoded chunk
    return true;
  }

  if ((sync_context.encoded_chunk->chunk_type != CHUNK_TYPE_ENCODED_AUDIO) &&
      (sync_context.encoded_chunk->chunk_type != CHUNK_TYPE_DECODED_AUDIO)) {
    // New codec header
    sync_context.decoder->reset_decoders();
    audio::AudioStreamInfo decoded_stream_info;
    if (!sync_context.decoder->process_header(sync_context.encoded_chunk, &decoded_stream_info)) {
      ESP_LOGE(TAG, "Failed to process audio codec header");
    } else {
      ESP_LOGI(TAG, "Processed new codec header");
      // Verify it matches what we expect
      if (decoded_stream_info != sync_context.current_stream_info) {
        ESP_LOGW(TAG, "Decoded stream info doesn't match expected!");
      }
    }
  } else if ((sync_context.decoder->get_current_codec() != SendspinCodecFormat::UNSUPPORTED) &&
             (sync_context.encoded_chunk->chunk_type == CHUNK_TYPE_ENCODED_AUDIO)) {
    int64_t client_timestamp = this->parent_->get_client_time(sync_context.encoded_chunk->timestamp);
    int64_t time_until_playback_us = client_timestamp - esp_timer_get_time();
    // TODO: Don't hardcode 400 ms margin when first starting up, we can measure latency for future runs
    // We should also track how much audio we have buffered after the initial decode has happened, and use that to skip
    // some chunks if needed.
    if (time_until_playback_us < sync_context.initial_decode * 400000) {
      // Chunk was already supposed to play, skip it!
      sync_context.encoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
      return false;
    }

    if (!sync_context.decoder->decode_audio_chunk(sync_context.encoded_chunk, sync_context.decoded_chunk)) {
      ESP_LOGE(TAG, "Failed to decode audio chunk");
    } else {
      sync_context.decoded_chunk->timestamp = client_timestamp;
    }
  }

  // Clear the encoded chunk. Note, for PCM, decoded_chunk is the same data as encoded_chunk but has its own
  // shared_ptr reference
  sync_context.encoded_chunk = nullptr;

  return true;
}

bool SendspinMediaSource::sync_synchronize_audio_(SyncContext &sync_context,
                                                  SendspinMediaSourcePipeline &pipeline_context) {
  if (sync_context.initial_decode || (sync_context.recent_error_us > sync_context.temporary_hard_sync_threshold)) {
    this->sync_hard_sync_add_silence_(sync_context, pipeline_context);
    return true;
  }

  sync_context.release_chunk = true;
  sync_context.output_transfer_buffer->change_inplace_buffer(sync_context.decoded_chunk->get_data(),
                                                             sync_context.decoded_chunk->size);

  InternalAudioTiming timings;
  int32_t frame_corrections = 0;

  if (sync_context.recent_error_us < -sync_context.temporary_hard_sync_threshold) {
    this->sync_hard_sync_remove_audio_(sync_context, pipeline_context, timings, frame_corrections);
  } else if (sync_context.recent_error_us < -SOFT_SYNC_THRESHOLD_US) {
    this->sync_soft_sync_remove_audio_(sync_context, pipeline_context, timings, frame_corrections);
  } else if (sync_context.recent_error_us > SOFT_SYNC_THRESHOLD_US) {
    this->sync_soft_sync_add_audio_(sync_context, pipeline_context, timings, frame_corrections);
  }

  uint32_t chunk_frame_count =
      sync_context.current_stream_info.bytes_to_frames(sync_context.decoded_chunk->get_usable_size());
  uint32_t new_frames = chunk_frame_count;
  const int64_t new_duration_ms = sync_context.current_stream_info.frames_to_milliseconds_with_remainder(&new_frames);
  const int64_t new_duration_us =
      new_duration_ms * 1000LL + sync_context.current_stream_info.frames_to_microseconds(new_frames);

  timings.timestamp = sync_context.decoded_chunk->timestamp + new_duration_us;
  timings.total_frames = chunk_frame_count + frame_corrections;
  timings.frame_corrections = frame_corrections;
  sync_context.pending_frame_corrections += frame_corrections;

  sync_context.chunk_timings.push_back(timings);
  sync_context.last_error.reset();  // We're accounted for the most recent error

  return true;
}

void SendspinMediaSource::set_transfer_callbacks_(SyncContext &sync_context, int pipeline) {
  std::function<size_t(uint8_t *, size_t, TickType_t)> wrapped_callback =
      [this, &sync_context, pipeline](uint8_t *data, size_t len, TickType_t ticks) {
        return this->output_callback_(data, len, ticks, sync_context.current_stream_info, pipeline);
      };
  sync_context.output_transfer_buffer->set_sink(std::move(wrapped_callback));
  std::function<size_t(uint8_t *, size_t, TickType_t)> wrapped_callback2 =
      [this, &sync_context, pipeline](uint8_t *data, size_t len, TickType_t ticks) {
        return this->output_callback_(data, len, ticks, sync_context.current_stream_info, pipeline);
      };
  sync_context.interpolation_transfer_buffer->set_sink(std::move(wrapped_callback2));
}

enum class SyncTaskState : uint8_t {
  LOAD_CHUNK,
  SYNCHRONIZE_AUDIO,
  TRANSFER_AUDIO,
};

void SendspinMediaSource::sync_task(void *params) {
  /* This is the magic for playing synced audio. We push audio through the stack keeping careful track of the amount and
   * timing. We process the speaker callbacks to determine when audio will actually play.
   * TODO: Generalize this and put it in the audio component so that other synced audio protocols can use it
   */

  auto *task_params = static_cast<GenerateTaskParams *>(params);
  SendspinMediaSource *this_source = task_params->source;
  size_t pipeline = task_params->pipeline;
  delete task_params;

  auto &ctx = this_source->sendspin_pipelines_[pipeline];

  xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STARTING);
  {  // Ensures all C++ objects deallocate when they fall out of scope
    SyncContext sync_context;
    sync_context.current_stream_info = ctx.stream_info;
    sync_context.bytes_per_frame = sync_context.current_stream_info.frames_to_bytes(1);

    sync_context.output_transfer_buffer = audio::AudioSinkTransferBuffer::create_inplace();

    // TODO: Verify this allocation succeeds
    sync_context.interpolation_transfer_buffer = audio::AudioSinkTransferBuffer::create(
        sync_context.current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS));

    this_source->set_transfer_callbacks_(sync_context, pipeline);
    sync_context.decoder = std::make_unique<SendspinDecoder>();

    sync_context.release_chunk = true;
    sync_context.initial_decode = true;
    sync_context.pending_frame_corrections = 0;
    sync_context.synced_chunks = 0;
    sync_context.temporary_hard_sync_threshold = HARD_SYNC_THRESHOLD_US;

    ctx.single_frames_added_ = 0;
    ctx.single_frames_removed_ = 0;
    ctx.hard_sync_added_frames_ = 0;
    ctx.hard_sync_removed_frames_ = 0;

    xQueueReset(ctx.playback_progress_queue);

    SyncTaskState sync_state = SyncTaskState::LOAD_CHUNK;

    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_RUNNING);
    while (!(xEventGroupGetBits(ctx.event_group) & COMMAND_STOP)) {
      // if (sync_context.current_stream_info != ctx.stream_info) {
      //   // This shouldn't change in the middle of a session. The hub should stop/warn us ahead of time
      //   // TODO: Probably safe to remove
      //   SendspinControls control_type = SendspinControls::STOP;
      //   xQueueSend(ctx.controls_queue, &control_type, portMAX_DELAY);
      //   control_type = SendspinControls::START;
      //   xQueueSend(ctx.controls_queue, &control_type, portMAX_DELAY);
      //   ESP_LOGE(TAG, "Stream settings changed mid-stream, restarting task");
      //   break;
      // }

      this_source->sync_determine_raw_sync_error_(sync_context, ctx);

      switch (sync_state) {
        case SyncTaskState::LOAD_CHUNK:
          /*
           * Moves to the next state if achunk is available and it is supposed to play in the future.
           * Function yields while waiting to receive the next chunk.
           */
          if (!this_source->sync_load_next_chunk_(sync_context, ctx)) {
            continue;
          }
          if (!this_source->sync_decode_audio_(sync_context, ctx)) {
            // Failed to decode audio (or skipped), try again
#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
            ESP_LOGW(TAG, "Failed to decode audio chunk");
#endif
            continue;
          }
          if (sync_context.decoded_chunk == nullptr) {
            // No decoded audio available yet, try again (probably just processed a header)
            continue;
          }
          sync_state = SyncTaskState::SYNCHRONIZE_AUDIO;
          // Intentional fallthrough
        case SyncTaskState::SYNCHRONIZE_AUDIO:
          /*
           * Moves to the next state (after adding audio) if we have determined an error from speaker messages or it is
           * the initial sync. Manually yields if this can't proceed.
           */
          if (!this_source->sync_determine_raw_sync_error_(sync_context, ctx)) {
            // No error available
            vTaskDelay(pdMS_TO_TICKS(15));
            continue;
          }
          this_source->sync_determine_predicted_error_(sync_context);  // Account for pending corrections
          this_source->sync_synchronize_audio_(sync_context, ctx);     // Adjust audio to correct for the error
          sync_state = SyncTaskState::TRANSFER_AUDIO;
          // Intentional fallthrough
        case SyncTaskState::TRANSFER_AUDIO:
          /*
           * Moves to the next state if all audio is sent from both the interpolation transfer buffer and output
           * transfer buffer. Function yields while waiting to send the audio.
           */
          if (!this_source->sync_transfer_audio_(sync_context)) {
            continue;
          }
          sync_state = SyncTaskState::LOAD_CHUNK;
          break;
      }
#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
      // TODO: Remove these debug checks/logging
      if (ctx.encoded_chunk_queue->size() == 1) {
        ESP_LOGW(TAG, "Potential buffer underflow incoming");
      }

      static uint32_t high_water_mark = SYNC_TASK_STACK_SIZE;
      uint32_t new_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
      if (new_high_water_mark < high_water_mark) {
        ESP_LOGD(TAG, "Sync task - High water mark changed from %d to %d.", high_water_mark, new_high_water_mark);
        high_water_mark = new_high_water_mark;
      }
#endif
    }

    xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPING);
  }

  xEventGroupSetBits(ctx.event_group, EventGroupBits::TASK_STOPPED);

#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif

  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace sendspin
}  // namespace esphome
