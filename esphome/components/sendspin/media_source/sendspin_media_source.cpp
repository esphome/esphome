#include "sendspin_media_source.h"

#if defined(USE_ESP32) && defined(USE_SENDSPIN_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "esphome/core/application.h"

#include <algorithm>
#include <cstdlib>

namespace esphome {
namespace sendspin {

namespace {
struct AudioSinkAdapter : public audio::AudioSinkCallback {
  media_source::MediaSource *source;
  audio::AudioStreamInfo stream_info;

  size_t audio_sink_write(uint8_t *data, size_t length, TickType_t ticks_to_wait) override {
    return this->source->write_output(data, length, pdTICKS_TO_MS(ticks_to_wait), this->stream_info);
  }
};
}  // namespace

// TODO: Remove this. Take out unnecessary logs and change useful ones to be VERBOSE level
//#define SENDSPIN_MEDIA_SOURCE_DEBUG

static const int64_t HARD_SYNC_THRESHOLD_US = 5000;
static const int64_t HARD_SYNC_SETTLE_THRESHOLD_US = 500;  // Tighter threshold used while settling after a hard sync
static const int64_t SOFT_SYNC_THRESHOLD_US = 100;

static const uint32_t INITIAL_SYNC_ZEROS_DURATION_MS = 25;

static const UBaseType_t SYNC_TASK_PRIORITY = 1;
static const size_t SYNC_TASK_STACK_SIZE = 6192;  // Opus uses more stack than FLAC

static const char *const TAG = "sendspin_media_source";

enum class SourceControls : uint8_t {
  START = 0,
  STOP = 1,
  COMMAND = 2,
};

struct ControlMessage {
  SourceControls control;
};

enum EventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  COMMAND_PAUSE = (1 << 1),
  TASK_STARTING = (1 << 7),
  TASK_RUNNING = (1 << 8),
  TASK_STOPPING = (1 << 9),
  TASK_STOPPED = (1 << 10),
  TASK_ERROR = (1 << 11),
};

void SendspinMediaSource::setup() {
  this->disable_loop();

  // Create event group and queue
  this->event_group_ = xEventGroupCreate();
  this->controls_queue_ = xQueueCreate(3, sizeof(ControlMessage));

  this->playback_progress_queue_ = xQueueCreateWithCaps(50, sizeof(PlaybackProgress), MALLOC_CAP_SPIRAM);
  if (this->playback_progress_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create playback progress queue.");
    this->mark_failed();
    return;
  }

  this->encoded_ring_buffer_ = SendspinAudioRingBuffer::create(this->parent_->get_buffer_size());
  if (this->encoded_ring_buffer_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create encoded audio ring buffer.");
    this->mark_failed();
    return;
  }

  // Register callbacks for volume related controls from the Sendspin
  this->parent_->add_controls_callback([this](const SendspinControls &control_type) {
    switch (control_type) {
      case SendspinControls::START:
        this->pending_start_ = true;
        this->enable_loop_soon_any_context();
        break;
      case SendspinControls::STOP:
        // Intentional fallthrough
      case SendspinControls::CLEAR:
        xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_STOP);
        // Ring buffer reset happens in TASK_STOPPED handler when consumer task is safely done
        break;
      case SendspinControls::VOLUME_UPDATE:
        this->request_volume_(this->parent_->get_volume() / 100.0f);
        break;
      case SendspinControls::MUTE_UPDATE:
        this->request_mute_(this->parent_->get_muted());
        break;
      default:
        break;
    }
  });

  this->parent_->set_audio_chunk_callback(
      [this](const uint8_t *data, size_t data_size, int64_t timestamp, ChunkType chunk_type, TickType_t ticks_to_wait) {
        return this->encoded_ring_buffer_->write_chunk(data, data_size, timestamp, chunk_type, ticks_to_wait);
      });
}

bool SendspinMediaSource::play_uri(const std::string &uri) {
  if (!this->is_ready() || this->is_failed() || !this->has_listener()) {
    return false;
  }

  // Check if already playing
  if (this->get_state() != media_source::MediaSourceState::IDLE) {
    ESP_LOGE(TAG, "Cannot play '%s': source is busy", uri.c_str());
    return false;
  }

  // Validate URI starts with "sendspin://"
  if (!uri.starts_with("sendspin://")) {
    ESP_LOGE(TAG, "Invalid URI: '%s'", uri.c_str());
    return false;
  }

  // Strip "sendspin://" prefix
  std::string sendspin_id = uri.substr(11);  // "sendspin://" is 11 characters

  ESP_LOGD(TAG, "sendspin_id: %s", sendspin_id.c_str());
  if (sendspin_id != "current") {
    // This is now a new server we need to connect to as a websocket client
    this->parent_->connect_to_server("ws://" + sendspin_id);
  }

  // Queue playback start
  ControlMessage message = {.control = SourceControls::START};
  xQueueSend(this->controls_queue_, &message, 0);
  this->enable_loop_soon_any_context();
  return true;
}

void SendspinMediaSource::loop() {
  // Process control messages
  ControlMessage incoming_control;
  if (xQueueReceive(this->controls_queue_, &incoming_control, 0)) {
    switch (incoming_control.control) {
      case SourceControls::START:
        this->generation_state_ = SendspinGenerationState::START_TASK;
        this->pending_start_ = false;
        break;
      case SourceControls::STOP:
        if (this->generation_state_ == SendspinGenerationState::GENERATING) {
          xEventGroupSetBits(this->event_group_, EventGroupBits::COMMAND_STOP);
        }
        break;
      default:
        break;
    }
  }

  // Process state machine
  switch (this->generation_state_) {
    case SendspinGenerationState::START_TASK: {
      if (!this->sync_task_.is_created()) {
        xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STARTING | EventGroupBits::TASK_RUNNING |
                                                     EventGroupBits::TASK_STOPPING | EventGroupBits::TASK_STOPPED |
                                                     EventGroupBits::COMMAND_STOP);
        this->parent_->update_state(SendspinClientState::SYNCHRONIZED);
        if (!this->sync_task_.create(sync_task, "Sendspin", SYNC_TASK_STACK_SIZE, this, SYNC_TASK_PRIORITY,
                                     this->task_stack_in_psram_)) {
          ESP_LOGE(TAG, "Failed to create sync task");
          this->mark_failed();
          return;
        }
      }
      ESP_LOGD(TAG, "Started sync task");
      this->generation_state_ = SendspinGenerationState::GENERATING;
      break;
    }
    case SendspinGenerationState::GENERATING: {
      EventBits_t event_bits = xEventGroupGetBits(this->event_group_);

      if (event_bits & TASK_STARTING) {
        ESP_LOGD(TAG, "Starting");
        xEventGroupClearBits(this->event_group_, TASK_STARTING);
      }

      if (event_bits & TASK_RUNNING) {
        ESP_LOGD(TAG, "Running");
        xEventGroupClearBits(this->event_group_, TASK_RUNNING);
        this->set_state_(media_source::MediaSourceState::PLAYING);
      }

      if (event_bits & TASK_STOPPING) {
        ESP_LOGD(TAG, "Stopping");
        xEventGroupClearBits(this->event_group_, TASK_STOPPING);
      }

      if (event_bits & TASK_STOPPED) {
        ESP_LOGD(TAG, "Stopped");
        bool had_error = event_bits & TASK_ERROR;
        xEventGroupClearBits(this->event_group_, TASK_STOPPED | TASK_ERROR | COMMAND_STOP);

        this->sync_task_.deallocate();
        this->set_state_(had_error ? media_source::MediaSourceState::ERROR : media_source::MediaSourceState::IDLE);
        this->generation_state_ = SendspinGenerationState::IDLE;
      }
      break;
    }
    case SendspinGenerationState::IDLE: {
      if (this->pending_start_) {
        this->request_play_uri_("sendspin://current");
        this->pending_start_ = false;
      }
      break;
    }
  }

  // Disable loop when idle with no pending controls
  if ((this->generation_state_ == SendspinGenerationState::IDLE) &&
      (uxQueueMessagesWaiting(this->controls_queue_) == 0)) {
    this->disable_loop();
  }
}

void SendspinMediaSource::handle_command(media_source::MediaSourceCommand command) {
  switch (command) {
    case media_source::MediaSourceCommand::STOP: {
      // Stop local generation
      if (this->generation_state_ == SendspinGenerationState::GENERATING) {
        ControlMessage message = {.control = SourceControls::STOP};
        xQueueSend(this->controls_queue_, &message, 0);
        this->enable_loop_soon_any_context();
      }
      this->parent_->update_state(SendspinClientState::EXTERNAL_SOURCE);
      break;
    }
    case media_source::MediaSourceCommand::PLAY:  // NOLINT(bugprone-branch-clone)
      this->parent_->send_client_command(SendspinControllerCommand::PLAY, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::PAUSE:
      this->parent_->send_client_command(SendspinControllerCommand::PAUSE, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::NEXT:
      this->parent_->send_client_command(SendspinControllerCommand::NEXT, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::PREVIOUS:
      this->parent_->send_client_command(SendspinControllerCommand::PREVIOUS, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::REPEAT_ALL:
      this->parent_->send_client_command(SendspinControllerCommand::REPEAT_ALL, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::REPEAT_ONE:
      this->parent_->send_client_command(SendspinControllerCommand::REPEAT_ONE, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::REPEAT_OFF:
      this->parent_->send_client_command(SendspinControllerCommand::REPEAT_OFF, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::SHUFFLE:
      this->parent_->send_client_command(SendspinControllerCommand::SHUFFLE, std::nullopt, std::nullopt);
      break;
    case media_source::MediaSourceCommand::UNSHUFFLE:
      this->parent_->send_client_command(SendspinControllerCommand::UNSHUFFLE, std::nullopt, std::nullopt);
      break;
    default:
      break;
  }
}

void SendspinMediaSource::notify_volume_changed(float volume) {
  this->parent_->update_volume(std::roundf(volume * 100.0f));
}

void SendspinMediaSource::notify_mute_changed(bool is_muted) { this->parent_->update_muted(is_muted); }

void SendspinMediaSource::notify_audio_played(uint32_t frames, int64_t timestamp) {
  PlaybackProgress playback_progress = {.frames_played = frames, .finish_timestamp = timestamp};
  if (!xQueueSend(this->playback_progress_queue_, &playback_progress, 0)) {
    ESP_LOGE(TAG, "Playback info queue was full");
  }
}

void SendspinMediaSource::sync_track_sent_audio_(SyncContext &sync_context, size_t bytes_sent) {
  uint32_t frames_sent = sync_context.current_stream_info.bytes_to_frames(bytes_sent);
  sync_context.buffered_frames += frames_sent;
  uint32_t remainder = frames_sent;
  int64_t ms = sync_context.current_stream_info.frames_to_milliseconds_with_remainder(&remainder);
  sync_context.new_audio_client_playtime +=
      1000LL * ms + static_cast<int64_t>(sync_context.current_stream_info.frames_to_microseconds(remainder));
}

bool SendspinMediaSource::sync_transfer_audio_(SyncContext &sync_context) {
  size_t decode_available = sync_context.release_chunk ? sync_context.decode_buffer->available() : 0;
  const uint32_t duration_in_transfer_buffers = sync_context.current_stream_info.bytes_to_ms(
      decode_available + sync_context.interpolation_transfer_buffer->available());

  size_t bytes_written = sync_context.interpolation_transfer_buffer->transfer_data_to_sink(
      pdMS_TO_TICKS(duration_in_transfer_buffers / 2), false);
  this->sync_track_sent_audio_(sync_context, bytes_written);

  if ((bytes_written > 0) && sync_context.initial_decode) {
    // Sent initial zeros, delay slightly to give it some time to work through the audio stack
    vTaskDelay(pdMS_TO_TICKS(sync_context.current_stream_info.bytes_to_ms(bytes_written) / 2));
  }

  if (sync_context.interpolation_transfer_buffer->available() == 0 && sync_context.release_chunk) {
    // No interpolation bytes available, send main audio data
    size_t decode_bytes_written =
        sync_context.decode_buffer->transfer_data_to_sink(pdMS_TO_TICKS(3 * duration_in_transfer_buffers / 2), false);
    this->sync_track_sent_audio_(sync_context, decode_bytes_written);
  }

  // When decode buffer fully consumed and released, mark done
  if (sync_context.decode_buffer->available() == 0 && sync_context.release_chunk) {
    sync_context.release_chunk = false;
  }

  // Keep transferring if there's still data to send
  if (sync_context.interpolation_transfer_buffer->available() > 0) {
    return false;
  }
  if (sync_context.release_chunk && sync_context.decode_buffer->available() > 0) {
    return false;
  }

  return true;
}

bool SendspinMediaSource::sync_load_next_chunk_(SyncContext &sync_context) {
  if (sync_context.encoded_entry == nullptr) {
    sync_context.encoded_entry = this->encoded_ring_buffer_->receive_chunk(pdMS_TO_TICKS(15));
    if (sync_context.encoded_entry == nullptr) {
      // No chunk available to process
      return false;
    }
  }

  return true;
}

void SendspinMediaSource::sync_process_playback_progress_(SyncContext &sync_context) {
  PlaybackProgress playback_progress;
  bool received = false;
  while (xQueueReceive(this->playback_progress_queue_, &playback_progress, 0) == pdTRUE) {
    received = true;
    uint32_t frames_played = playback_progress.frames_played;

    if (sync_context.initial_decode && frames_played) {
      sync_context.initial_decode = false;
    }

    if (frames_played > sync_context.buffered_frames) {
#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
      ESP_LOGW(TAG, "Buffered frames underflow: played %" PRIu32 " but only %" PRIu32 " buffered", frames_played,
               sync_context.buffered_frames);
#endif
      sync_context.buffered_frames = 0;
    } else {
      sync_context.buffered_frames -= frames_played;
    }
  }
  if (received) {
    uint32_t unplayed_frames = sync_context.buffered_frames;
    int64_t unplayed_ms = sync_context.current_stream_info.frames_to_milliseconds_with_remainder(&unplayed_frames);
    int64_t unplayed_us =
        1000LL * unplayed_ms +
        static_cast<int64_t>(sync_context.current_stream_info.frames_to_microseconds(unplayed_frames));
    sync_context.new_audio_client_playtime = playback_progress.finish_timestamp + unplayed_us;
  }
}

int32_t SendspinMediaSource::sync_soft_sync_remove_audio_(SyncContext &sync_context) {
  // Small sync adjustment after getting slightly ahead.
  // Removes the last frame in the chunk to get in sync. The second to last frame is replaced with the average
  // of it and the removed frame to minimize audible glitches.

  const uint32_t num_channels = sync_context.current_stream_info.get_channels();
  const uint32_t bytes_per_sample = sync_context.bytes_per_frame / num_channels;

  if (sync_context.decode_buffer->available() >= 2 * sync_context.bytes_per_frame) {
    for (uint32_t chan = 0; chan < num_channels; ++chan) {
      const int32_t first_sample = audio::unpack_audio_sample_to_q31(
          sync_context.decode_buffer->get_buffer_end() - 2 * sync_context.bytes_per_frame + chan * bytes_per_sample,
          bytes_per_sample);
      const int32_t second_sample = audio::unpack_audio_sample_to_q31(
          sync_context.decode_buffer->get_buffer_end() - sync_context.bytes_per_frame + chan * bytes_per_sample,
          bytes_per_sample);
      int32_t replacement_sample = first_sample / 2 + second_sample / 2;
      audio::pack_q31_as_audio_sample(
          replacement_sample,
          sync_context.decode_buffer->get_buffer_end() - 2 * sync_context.bytes_per_frame + chan * bytes_per_sample,
          bytes_per_sample);
    }

    sync_context.decode_buffer->decrease_buffer_length(sync_context.bytes_per_frame);
    return -1;
  }
  return 0;
}

int32_t SendspinMediaSource::sync_soft_sync_add_audio_(SyncContext &sync_context) {
  // Small sync adjustment after getting slightly behind.
  // Adds one new frame to get in sync. The new frame is inserted between the first and second frames.
  // The new frame is the average of the first two frames in the chunk to minimize audible glitches.

  if ((sync_context.interpolation_transfer_buffer->free() >= sync_context.bytes_per_frame) &&
      (sync_context.decode_buffer->available() >= 2 * sync_context.bytes_per_frame)) {
    const uint32_t num_channels = sync_context.current_stream_info.get_channels();
    const uint32_t bytes_per_sample = sync_context.bytes_per_frame / num_channels;

    for (uint32_t chan = 0; chan < num_channels; ++chan) {
      const int32_t first_sample = audio::unpack_audio_sample_to_q31(
          sync_context.decode_buffer->get_buffer_start() + chan * bytes_per_sample, bytes_per_sample);
      const int32_t second_sample = audio::unpack_audio_sample_to_q31(
          sync_context.decode_buffer->get_buffer_start() + chan * bytes_per_sample + sync_context.bytes_per_frame,
          bytes_per_sample);
      int32_t new_sample = first_sample / 2 + second_sample / 2;
      audio::pack_q31_as_audio_sample(
          new_sample, sync_context.decode_buffer->get_buffer_start() + chan * bytes_per_sample, bytes_per_sample);
      audio::pack_q31_as_audio_sample(
          first_sample, sync_context.interpolation_transfer_buffer->get_buffer_start() + chan * bytes_per_sample,
          bytes_per_sample);
    }
    sync_context.interpolation_transfer_buffer->increase_buffer_length(sync_context.bytes_per_frame);
    return 1;
  }
  return 0;
}

void SendspinMediaSource::sync_drain_until_codec_header_(SyncContext &sync_context) {
  // Drain any stale audio from the ring buffer until we find a codec header.
  // This prevents a race condition where a rapid stop/start causes the new codec header
  // to be preceded by leftover encoded audio from the previous stream.
  while (!(xEventGroupGetBits(this->event_group_) & COMMAND_STOP)) {
    auto *entry = this->encoded_ring_buffer_->receive_chunk(pdMS_TO_TICKS(100));
    if (entry == nullptr) {
      continue;  // Nothing available yet, keep waiting for the hub to send the header
    }
    if (entry->chunk_type != CHUNK_TYPE_ENCODED_AUDIO && entry->chunk_type != CHUNK_TYPE_DECODED_AUDIO) {
      // Found a codec header - hand it off for normal processing
      sync_context.encoded_entry = entry;
      sync_context.release_chunk = false;
      return;
    }
    // Stale audio data from previous stream, discard it
    this->encoded_ring_buffer_->return_chunk(entry);
  }
}

void SendspinMediaSource::sync_drain_stale_audio_() {
  // Drain stale audio from the ring buffer to free space for the next stream's burst.
  // If we encounter a codec header for the next stream, hold onto it (don't return it)
  // so the next task can use it immediately without re-draining.
  while (true) {
    auto *entry = this->encoded_ring_buffer_->receive_chunk(0);
    if (entry == nullptr) {
      break;
    }
    if (entry->chunk_type != CHUNK_TYPE_ENCODED_AUDIO && entry->chunk_type != CHUNK_TYPE_DECODED_AUDIO) {
      // Codec header for the next stream -- keep it checked out
      this->pending_codec_header_ = entry;
      break;
    }
    this->encoded_ring_buffer_->return_chunk(entry);
  }
}

DecodeResult SendspinMediaSource::sync_decode_audio_(SyncContext &sync_context) {
  if (sync_context.decode_buffer != nullptr && sync_context.decode_buffer->available() > 0) {
    // Already have decoded audio
    return DecodeResult::SUCCESS;
  }

  if ((sync_context.encoded_entry->chunk_type != CHUNK_TYPE_ENCODED_AUDIO) &&
      (sync_context.encoded_entry->chunk_type != CHUNK_TYPE_DECODED_AUDIO)) {
    // New codec header
    sync_context.decoder->reset_decoders();
    audio::AudioStreamInfo decoded_stream_info;
    if (!sync_context.decoder->process_header(sync_context.encoded_entry->data(), sync_context.encoded_entry->data_size,
                                              sync_context.encoded_entry->chunk_type, &decoded_stream_info)) {
      ESP_LOGE(TAG, "Failed to process audio codec header");
    } else {
      ESP_LOGI(TAG, "Processed new codec header");
      // Verify it matches what we expect
      if (decoded_stream_info != sync_context.current_stream_info) {
        ESP_LOGW(TAG, "Decoded stream info doesn't match expected!");
      }

      // Create or resize the decode buffer now that we know the maximum decoded size
      size_t needed = sync_context.decoder->get_maximum_decoded_size();
      if (sync_context.decode_buffer == nullptr) {
        sync_context.decode_buffer = audio::AudioSinkTransferBuffer::create(needed);
        if (sync_context.decode_buffer == nullptr) {
          ESP_LOGE(TAG, "Failed to allocate decode buffer");
          this->encoded_ring_buffer_->return_chunk(sync_context.encoded_entry);
          sync_context.encoded_entry = nullptr;
          return DecodeResult::ALLOCATION_FAILED;
        }
        if (sync_context.audio_sink != nullptr) {
          sync_context.decode_buffer->set_sink(sync_context.audio_sink);
        }
      } else if (needed > sync_context.decode_buffer->capacity()) {
        if (!sync_context.decode_buffer->reallocate(needed)) {
          ESP_LOGE(TAG, "Failed to reallocate decode buffer");
          this->encoded_ring_buffer_->return_chunk(sync_context.encoded_entry);
          sync_context.encoded_entry = nullptr;
          return DecodeResult::ALLOCATION_FAILED;
        }
      }
    }
  } else if ((sync_context.decoder->get_current_codec() != SendspinCodecFormat::UNSUPPORTED) &&
             (sync_context.encoded_entry->chunk_type == CHUNK_TYPE_ENCODED_AUDIO)) {
    int64_t client_timestamp = this->parent_->get_client_time(sync_context.encoded_entry->timestamp) -
                               static_cast<int64_t>(this->parent_->get_static_delay_ms()) * 1000 -
                               this->parent_->get_fixed_delay_us();

    if (client_timestamp < sync_context.new_audio_client_playtime - HARD_SYNC_THRESHOLD_US) {
      // This chunk will arrive too late to be played, skip it!
      this->encoded_ring_buffer_->return_chunk(sync_context.encoded_entry);
      sync_context.encoded_entry = nullptr;
      return DecodeResult::SKIPPED;
    }

    size_t decoded_size = 0;
    if (!sync_context.decoder->decode_audio_chunk(
            sync_context.encoded_entry->data(), sync_context.encoded_entry->data_size,
            sync_context.decode_buffer->get_buffer_end(), sync_context.decode_buffer->free(), &decoded_size)) {
      ESP_LOGE(TAG, "Failed to decode audio chunk");
      this->encoded_ring_buffer_->return_chunk(sync_context.encoded_entry);
      sync_context.encoded_entry = nullptr;
      return DecodeResult::FAILED;
    } else {
      sync_context.decode_buffer->increase_buffer_length(decoded_size);
      sync_context.decoded_timestamp = client_timestamp;
    }
  }

  // Return the encoded entry to the ring buffer
  this->encoded_ring_buffer_->return_chunk(sync_context.encoded_entry);
  sync_context.encoded_entry = nullptr;

  return DecodeResult::SUCCESS;
}

SyncTaskState SendspinMediaSource::sync_handle_initial_sync_(SyncContext &sync_context) {
  if (!sync_context.initial_decode) {
    return SyncTaskState::LOAD_CHUNK;
  }

  if (sync_context.interpolation_transfer_buffer->available() > 0) {
    const uint32_t duration_in_transfer_buffers =
        sync_context.current_stream_info.bytes_to_ms(sync_context.interpolation_transfer_buffer->available());
    size_t bytes_written = sync_context.interpolation_transfer_buffer->transfer_data_to_sink(
        pdMS_TO_TICKS(duration_in_transfer_buffers / 2), false);
    this->sync_track_sent_audio_(sync_context, bytes_written);
    if ((bytes_written > 0) && sync_context.initial_decode) {
      // Sent initial zeros, delay slightly to give it some time to work through the audio stack
      vTaskDelay(pdMS_TO_TICKS(sync_context.current_stream_info.bytes_to_ms(bytes_written) / 2));
    }
  } else {
    const size_t zeroed_bytes = sync_context.interpolation_transfer_buffer->free();
    std::memset((void *) sync_context.interpolation_transfer_buffer->get_buffer_end(), 0, zeroed_bytes);
    sync_context.interpolation_transfer_buffer->increase_buffer_length(
        std::min(zeroed_bytes, sync_context.current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS)));
  }

  return SyncTaskState::INITIAL_SYNC;
}

SyncTaskState SendspinMediaSource::sync_handle_load_chunk_(SyncContext &sync_context) {
  if (!this->parent_->is_time_synced()) {
    // Wait for the time filter to receive its first measurement before processing audio chunks.
    // Without a valid time offset, server timestamps can't be correctly converted to client timestamps.
    vTaskDelay(pdMS_TO_TICKS(15));
    return SyncTaskState::LOAD_CHUNK;
  }
  if (!this->sync_load_next_chunk_(sync_context)) {
    return SyncTaskState::LOAD_CHUNK;
  }
  DecodeResult decode_result = this->sync_decode_audio_(sync_context);
  if ((decode_result == DecodeResult::SKIPPED) || (decode_result == DecodeResult::FAILED)) {
    return SyncTaskState::LOAD_CHUNK;
  } else if (decode_result == DecodeResult::ALLOCATION_FAILED) {
    xEventGroupSetBits(this->event_group_, EventGroupBits::TASK_ERROR | EventGroupBits::COMMAND_STOP);
    return SyncTaskState::LOAD_CHUNK;
  }
  if (sync_context.decode_buffer == nullptr || sync_context.decode_buffer->available() == 0) {
    // No decoded audio available yet, try again (probably just processed a header)
    return SyncTaskState::LOAD_CHUNK;
  }
  return SyncTaskState::SYNCHRONIZE_AUDIO;
}

SyncTaskState SendspinMediaSource::sync_handle_synchronize_audio_(SyncContext &sync_context) {
  // Predicted error: positive means chunk should play later than our current buffer endpoint
  int64_t raw_error = sync_context.decoded_timestamp - sync_context.new_audio_client_playtime;

  // Use tighter threshold while settling after a hard sync (or during initial sync) to ensure precise alignment.
  // The normal threshold detects when hard sync is needed; the settle threshold keeps hard-syncing until well-aligned.
  const int64_t active_threshold = sync_context.hard_syncing ? HARD_SYNC_SETTLE_THRESHOLD_US : HARD_SYNC_THRESHOLD_US;

  if (raw_error > active_threshold) {
    // Buffer will run out before this chunk is supposed to play - insert silence to fill the gap
    sync_context.hard_syncing = true;

    // Clear any stale interpolation data
    sync_context.interpolation_transfer_buffer->decrease_buffer_length(
        sync_context.interpolation_transfer_buffer->available());

    // Compute silence directly in frames from microseconds (avoids ms truncation)
    uint32_t silence_frames =
        (static_cast<uint64_t>(raw_error) * sync_context.current_stream_info.get_sample_rate()) / 1000000;
    size_t silence_bytes = sync_context.current_stream_info.frames_to_bytes(silence_frames);

    // Cap at buffer capacity
    const size_t buffer_free = sync_context.interpolation_transfer_buffer->free();
    size_t actual_bytes = std::min(silence_bytes, buffer_free);

    std::memset((void *) sync_context.interpolation_transfer_buffer->get_buffer_end(), 0, actual_bytes);
    sync_context.interpolation_transfer_buffer->increase_buffer_length(actual_bytes);

    // Playtime estimate is advanced by sync_transfer_audio_() when the silence is actually sent
    sync_context.release_chunk = false;  // Keep decoded audio for after the silence

#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
    uint32_t frames_added = sync_context.current_stream_info.bytes_to_frames(actual_bytes);
    ESP_LOGD(TAG, "Hard sync: adding %" PRIu32 " frames of silence for %" PRId64 "us future error", frames_added,
             raw_error);
#endif
  } else if (raw_error < -active_threshold) {
    // Chunk should have played already - we're behind, drop it
    // The skip logic in sync_decode_audio_ will keep dropping until we catch up
    sync_context.hard_syncing = true;
    sync_context.decode_buffer->decrease_buffer_length(sync_context.decode_buffer->available());
#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
    ESP_LOGD(TAG, "Hard sync: dropping decoded chunk, %" PRId64 "us behind", -raw_error);
#endif
    return SyncTaskState::LOAD_CHUNK;
  } else {
    // Within tolerance - exit hard sync mode and use sample insertion/deletion for fine corrections
    sync_context.hard_syncing = false;

    if (raw_error > SOFT_SYNC_THRESHOLD_US) {
      // Slightly behind - add one interpolated frame between the first two decoded frames
      // Playtime estimate is advanced by sync_transfer_audio_() when the extra frame is sent
      this->sync_soft_sync_add_audio_(sync_context);
    } else if (raw_error < -SOFT_SYNC_THRESHOLD_US) {
      // Slightly ahead - remove last frame, blend into second-to-last
      // Playtime estimate naturally reflects the removed frame: sync_transfer_audio_() sends fewer bytes
      this->sync_soft_sync_remove_audio_(sync_context);
    }
    // else: Dead zone - pass decoded audio through directly
    sync_context.release_chunk = true;
  }
  return SyncTaskState::TRANSFER_AUDIO;
}

SyncTaskState SendspinMediaSource::sync_handle_transfer_audio_(SyncContext &sync_context) {
  if (!this->sync_transfer_audio_(sync_context)) {
    return SyncTaskState::TRANSFER_AUDIO;  // Not done transferring yet
  }
  if (sync_context.decode_buffer != nullptr && sync_context.decode_buffer->available() > 0) {
    // Decoded audio still waiting (was held back while silence was sent) - re-sync it
    return SyncTaskState::SYNCHRONIZE_AUDIO;
  }
  return SyncTaskState::LOAD_CHUNK;
}

void SendspinMediaSource::sync_task(void *params) {
  /* This is the magic for playing synced audio. We push audio through the stack keeping careful track of the amount and
   * timing. We process the speaker callbacks to determine when audio will actually play.
   */

  SendspinMediaSource *this_source = static_cast<SendspinMediaSource *>(params);

  xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_STARTING);
  do {  // do-while(false) ensures RAII objects are destroyed on all exit paths via break
    SyncContext sync_context;
    // Set stream_info from hub's current stream parameters
    auto &stream_params = this_source->parent_->get_current_stream_params();
    if (stream_params.bit_depth.has_value() && stream_params.channels.has_value() &&
        stream_params.sample_rate.has_value()) {
      sync_context.current_stream_info = audio::AudioStreamInfo(
          stream_params.bit_depth.value(), stream_params.channels.value(), stream_params.sample_rate.value());
    }
    sync_context.bytes_per_frame = sync_context.current_stream_info.frames_to_bytes(1);

    sync_context.interpolation_transfer_buffer = audio::AudioSinkTransferBuffer::create(
        sync_context.current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS));
    if (sync_context.interpolation_transfer_buffer == nullptr) {
      ESP_LOGE(TAG, "Failed to allocate interpolation transfer buffer");
      xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_ERROR);
      break;
    }

    AudioSinkAdapter audio_sink;
    if (this_source->has_listener()) {
      audio_sink.source = this_source;
      audio_sink.stream_info = sync_context.current_stream_info;
      sync_context.audio_sink = &audio_sink;
      sync_context.interpolation_transfer_buffer->set_sink(&audio_sink);
    }
    sync_context.decoder = std::make_unique<SendspinDecoder>();

    sync_context.release_chunk = true;
    sync_context.initial_decode = true;
    sync_context.buffered_frames = 0;

    xQueueReset(this_source->playback_progress_queue_);

    SyncTaskState sync_state = SyncTaskState::INITIAL_SYNC;

    xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_RUNNING);

    if (this_source->pending_codec_header_ != nullptr) {
      // Use the codec header saved from the previous task's drain
      sync_context.encoded_entry = this_source->pending_codec_header_;
      this_source->pending_codec_header_ = nullptr;
    } else {
      this_source->sync_drain_until_codec_header_(sync_context);
    }
    if (sync_context.encoded_entry != nullptr) {
      this_source->sync_decode_audio_(sync_context);
    }

    while (!(xEventGroupGetBits(this_source->event_group_) & COMMAND_STOP)) {
      this_source->sync_process_playback_progress_(sync_context);

      switch (sync_state) {
        case SyncTaskState::INITIAL_SYNC:
          sync_state = this_source->sync_handle_initial_sync_(sync_context);
          break;
        case SyncTaskState::LOAD_CHUNK:
          sync_state = this_source->sync_handle_load_chunk_(sync_context);
          break;
        case SyncTaskState::SYNCHRONIZE_AUDIO:
          sync_state = this_source->sync_handle_synchronize_audio_(sync_context);
          break;
        case SyncTaskState::TRANSFER_AUDIO:
          sync_state = this_source->sync_handle_transfer_audio_(sync_context);
          break;
      }

#ifdef SENDSPIN_MEDIA_SOURCE_DEBUG
      static uint32_t high_water_mark = SYNC_TASK_STACK_SIZE;
      uint32_t new_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
      if (new_high_water_mark < high_water_mark) {
        ESP_LOGD(TAG, "Sync task - High water mark changed from %d to %d.", high_water_mark, new_high_water_mark);
        high_water_mark = new_high_water_mark;
      }
#endif
    }

    // Return any borrowed ring buffer entry before leaving scope
    if (sync_context.encoded_entry != nullptr) {
      this_source->encoded_ring_buffer_->return_chunk(sync_context.encoded_entry);
      sync_context.encoded_entry = nullptr;
    }

  } while (false);

  xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_STOPPING);
  this_source->sync_drain_stale_audio_();
  xEventGroupSetBits(this_source->event_group_, EventGroupBits::TASK_STOPPED);

  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace sendspin
}  // namespace esphome

#endif  // USE_ESP32 && USE_SENDSPIN_PLAYER && USE_SENDSPIN_CONTROLLER
