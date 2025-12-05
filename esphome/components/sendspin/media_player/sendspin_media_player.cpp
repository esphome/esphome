#include "sendspin_media_player.h"

#if defined(USE_ESP_IDF) && defined(USE_MEDIA_PLAYER)

#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <memory>
#include <optional>

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.media_player";

#if defined(USE_SENDSPIN_PLAYER)
static const uint32_t DECODED_CHUNK_QUEUE_SIZE = 10;

static const size_t SYNC_TASK_STACK_SIZE = 5 * 1024;

static const int GOOD_SYNCS_BEFORE_UNMUTE = 1;
static const int64_t HARD_SYNC_THRESHOLD_US = 5000;
static const int64_t HARD_RESYNC_THRESHOLD_US = 500;
static const int64_t SOFT_SYNC_THRESHOLD_US = 100;

static const uint32_t INITIAL_SYNC_ZEROS_DURATION_MS = 25;

static const UBaseType_t SYNC_TASK_PRIORITY = 5;

enum EventGroupBits : uint32_t {
  CONTROL_START = (1 << 0),
  CONTROL_STOP = (1 << 1),

  TASK_STARTING = (1 << 10),
  TASK_RUNNING = (1 << 11),
  TASK_STOPPING = (1 << 12),
  TASK_STOPPED = (1 << 13),
};
#endif

void SendspinMediaPlayer::setup() {
  this->sendspin_controls_queue_ = xQueueCreate(3, sizeof(SendspinControls));
  if (this->sendspin_controls_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create sendspin controls queue.");
    this->mark_failed();
  }

  this->parent_->add_controls_callback([this](const SendspinControls &control_type) {
    switch (control_type) {
      case SendspinControls::START:  // Intentional fallthrough
      case SendspinControls::STOP: {
        // Add to queue to carefully process in loop() at the appropriate time
        xQueueSend(this->sendspin_controls_queue_, &control_type, 0);
        break;
      }
#if defined(USE_SENDSPIN_PLAYER)
      case SendspinControls::VOLUME_UPDATE: {
        // Process immediately
        this->volume_ = this->parent_->get_volume();
        break;
      }
      case SendspinControls::MUTE_UPDATE: {
        // Process immediately
        if (this->parent_->get_muted()) {
          this->make_call().set_command(media_player::MEDIA_PLAYER_COMMAND_MUTE).perform();
        } else {
          this->make_call().set_command(media_player::MEDIA_PLAYER_COMMAND_UNMUTE).perform();
        }
        break;
      }
#endif
      default:
        break;
    }
  });

#if defined(USE_SENDSPIN_PLAYER)
  // Apply inverse remap to get unbounded volume from speaker's bounded volume
  this->sync_speaker_volume();

  this->decoded_chunk_queue_ = audio::AudioChunkQueue::create(DECODED_CHUNK_QUEUE_SIZE);
  if (this->decoded_chunk_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create chunk queue.");
    this->mark_failed();
  }

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create event group.");
    this->mark_failed();
  }

  this->playback_progress_queue_ = xQueueCreateWithCaps(50, sizeof(PlaybackProgress), MALLOC_CAP_SPIRAM);
  if (this->playback_progress_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create playback progress queue.");
    this->mark_failed();
  }

  this->speaker_->add_audio_output_callback([this](uint32_t frames_played, int64_t finish_timestamp) {
    PlaybackProgress playback_progress = {.frames_played = frames_played, .finish_timestamp = finish_timestamp};
    if (!xQueueSend(this->playback_progress_queue_, &playback_progress, 0)) {
      ESP_LOGE(TAG, "Playback info queue was full");
    }
  });

  this->parent_->add_audio_chunk_callback([this](std::shared_ptr<SendspinAudioChunk> audio_chunk,
                                                 TickType_t ticks_to_wait, const audio::AudioStreamInfo &stream_info) {
    if (!this->task_processing_) {
      // Task isn't running, so don't add it to the queue
      return false;
    }

    this->audio_stream_info_ = stream_info;

    // AudioChunkQueue handles shared_ptr directly
    return this->decoded_chunk_queue_->add_chunk(std::static_pointer_cast<audio::AudioChunk>(audio_chunk),
                                                 ticks_to_wait);
  });

#endif
  this->parent_->add_controls_callback([this](const SendspinControls &control_type) {
    switch (control_type) {
      case SendspinControls::START:  // Intentional fallthrough
      case SendspinControls::STOP: {
        // Add to queue to carefully process in loop() at the appropriate time
        xQueueSend(this->sendspin_controls_queue_, &control_type, 0);
        break;
      }
#if defined(USE_SENDSPIN_PLAYER)
      case SendspinControls::VOLUME_UPDATE: {
        // Process immediately
        this->volume_ = this->parent_->get_volume();
        break;
      }
      case SendspinControls::MUTE_UPDATE: {
        // Process immediately
        if (this->parent_->get_muted()) {
          this->make_call().set_command(media_player::MEDIA_PLAYER_COMMAND_MUTE).perform();
        } else {
          this->make_call().set_command(media_player::MEDIA_PLAYER_COMMAND_UNMUTE).perform();
        }
        break;
      }
#endif
      default:
        break;
    }
  });

  // Register for group updates to sync playback state
  this->parent_->add_group_update_callback([this](const GroupUpdateObject &group_obj) {
    if (group_obj.playback_state.has_value()) {
      media_player::MediaPlayerState new_state;
      switch (group_obj.playback_state.value()) {
        case SendspinPlaybackState::PLAYING:
          new_state = media_player::MEDIA_PLAYER_STATE_PLAYING;
          break;
        case SendspinPlaybackState::PAUSED:
          new_state = media_player::MEDIA_PLAYER_STATE_PAUSED;
          break;
        case SendspinPlaybackState::STOPPED:
        default:
          new_state = media_player::MEDIA_PLAYER_STATE_IDLE;
          break;
      }
      if (this->state != new_state) {
        this->state = new_state;
        this->force_publish_state_ = true;
      }
    }
  });

  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->publish_state();
}

void SendspinMediaPlayer::loop() {
  // Determine state of the media player
  media_player::MediaPlayerState old_state = this->state;

  SendspinControls incoming_control;
  if (xQueuePeek(this->sendspin_controls_queue_, &incoming_control, 0)) {
    switch (incoming_control) {
      case SendspinControls::START: {
#if defined(USE_SENDSPIN_PLAYER)
        if (this->sync_task_handle_ == nullptr) {
          // The task is not running in any state
          if (xQueueReceive(this->sendspin_controls_queue_, &incoming_control, 0)) {
            xEventGroupSetBits(this->event_group_, EventGroupBits::CONTROL_START);
          }
        } else if ((this->task_processing_) &&
                   !(xEventGroupGetBits(this->event_group_) & EventGroupBits::CONTROL_STOP)) {
          // Already fully running and processing audio or the task hasn't processed a pending stop control, discard the
          // control message
          xQueueReceive(this->sendspin_controls_queue_, &incoming_control, 0);
        }
// If neither of these conditions hold, then we must be starting, as we set task_procesing_ to false once we
// start stopping. Leave this command in the queue. If we're starting, eventually the task_processing_ variable
// will be true and we'll discard the command then.
#else
        // Received start control so group is playing, directly set state
        xQueueReceive(this->sendspin_controls_queue_, &incoming_control, 0);
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
#endif
        break;
      }
      case SendspinControls::STOP: {
#if defined(USE_SENDSPIN_PLAYER)
        if (this->sync_task_handle_ == nullptr) {
          // The task is not running in any state - discard the control message
          xQueueReceive(this->sendspin_controls_queue_, &incoming_control, 0);
        } else if (!this->task_processing_) {
          // Task is transitioning (starting or stopping), leave command in queue
          // If we're already stopping, eventually sync_task_handle_ will be null and we'll discard it
          // If we're starting, eventually task_processing_ will be true and we'll process it
        } else {
          // Task is fully running and processing audio, process the stop command
          if (xQueueReceive(this->sendspin_controls_queue_, &incoming_control, 0)) {
            xEventGroupSetBits(this->event_group_, EventGroupBits::CONTROL_STOP);
          }
        }
#else
        // Received stop control so group is idle, directly set state
        xQueueReceive(this->sendspin_controls_queue_, &incoming_control, 0);
        this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
#endif
        break;
      }
      default:
        break;
    }
  }

#if defined(USE_SENDSPIN_PLAYER)
  // Handle the task's state
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);
  if (event_group_bits & EventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Starting");
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STARTING);
  }
  if (event_group_bits & EventGroupBits::TASK_RUNNING) {
    ESP_LOGD(TAG, "Started");
    this->parent_->update_state(SendspinPlayerState::SYNCHRONIZED);
    this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
    this->task_processing_ = true;
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_RUNNING);
  }
  if (event_group_bits & EventGroupBits::TASK_STOPPING) {
    ESP_LOGD(TAG, "Stopping");
    this->task_processing_ = false;
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STOPPING);
  }
  if (event_group_bits & EventGroupBits::TASK_STOPPED) {
    ESP_LOGD(TAG, "Stopped");
    this->parent_->update_state(SendspinPlayerState::SYNCHRONIZED);
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;

    vTaskDelete(this->sync_task_handle_);
    this->sync_task_handle_ = nullptr;

    xEventGroupClearBits(this->event_group_, EventGroupBits::CONTROL_STOP);
    xEventGroupClearBits(this->event_group_, EventGroupBits::TASK_STOPPED);
  }

  if (xEventGroupGetBits(this->event_group_) & CONTROL_START) {
    if (this->sync_task_stack_buffer_ == nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        this->sync_task_stack_buffer_ = stack_allocator.allocate(SYNC_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        this->sync_task_stack_buffer_ = stack_allocator.allocate(SYNC_TASK_STACK_SIZE);
      }
    }
    if (this->sync_task_handle_ == nullptr) {
      // Reset the relevant queues
      xQueueReset(this->playback_progress_queue_);
      this->decoded_chunk_queue_->reset();

      this->sync_task_handle_ =
          xTaskCreateStatic(sync_task, "sendspin_sync", SYNC_TASK_STACK_SIZE, (void *) this, SYNC_TASK_PRIORITY,
                            this->sync_task_stack_buffer_, &this->sync_task_stack_);
      xEventGroupClearBits(this->event_group_, EventGroupBits::CONTROL_START);
    }
  }

  if (this->volume_.has_value()) {
    // TODO: If USE_SENDSPIN_PLAYER isn't defined, the volume command should be sent to the server
    float unbounded_volume = static_cast<float>(this->volume_.value()) / 100.0f;
    // Apply forward remap to convert unbounded volume to bounded volume for the speaker
    float bounded_volume = this->volume_min_ + unbounded_volume * (this->volume_max_ - this->volume_min_);
    this->volume = unbounded_volume;
    this->speaker_->set_volume(bounded_volume);

    this->publish_state();
    this->volume_.reset();
  }
#endif

#ifdef USE_SENDSPIN_SENSOR
  static int64_t last_sensor_update = esp_timer_get_time();
  if (esp_timer_get_time() - last_sensor_update > 5000000) {
    last_sensor_update = esp_timer_get_time();

    this->parent_->update_sendspin_sensor({.type = SendspinSensorTypes::SINGLE_SYNC_FRAMES_ADDED,
                                           .value = static_cast<float>(this->single_frames_added_)});
    this->parent_->update_sendspin_sensor({.type = SendspinSensorTypes::SINGLE_SYNC_FRAMES_REMOVED,
                                           .value = static_cast<float>(this->single_frames_removed_)});
    this->parent_->update_sendspin_sensor({.type = SendspinSensorTypes::HARD_SYNC_FRAMES_ADDED,
                                           .value = static_cast<float>(this->hard_sync_added_frames_)});
    this->parent_->update_sendspin_sensor({.type = SendspinSensorTypes::HARD_SYNC_FRAMES_REMOVED,
                                           .value = static_cast<float>(this->hard_sync_removed_frames_)});
    this->parent_->update_sendspin_sensor(
        {.type = SendspinSensorTypes::AUDIBLE_SYNCS, .value = static_cast<float>(this->audible_syncs_)});
  }
#endif

  if ((this->state != old_state) || (this->force_publish_state_)) {
    this->force_publish_state_ = false;
    this->publish_state();
    ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
  }
}

media_player::MediaPlayerTraits SendspinMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();

  traits.set_supports_pause(true);
  // aioesphomeapi doesn't know about these commands, so we shouldn't advertise them
  // traits.set_supports_next_previous(true);
  // traits.set_supports_repeat(true);
  // traits.set_supports_shuffle(true);

  return traits;
}

void SendspinMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (!this->is_ready() || this->is_failed()) {
    // Ignore any commands sent before the media player is setup
    return;
  }

#if defined(USE_SENDSPIN_PLAYER)
  if (call.get_volume().has_value()) {
    this->volume_ = std::roundf(call.get_volume().value() * 100.0f);
    this->parent_->update_volume(this->volume_.value());
  }
#endif

  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        if (this->state == media_player::MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING) {
          this->parent_->send_client_command(SendspinCommandType::PAUSE);
        } else {
          this->parent_->send_client_command(SendspinCommandType::PLAY);
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:
        this->parent_->send_client_command(SendspinCommandType::PLAY);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        this->parent_->send_client_command(SendspinCommandType::PAUSE);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        this->parent_->send_client_command(SendspinCommandType::STOP);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
        this->parent_->send_client_command(SendspinCommandType::REPEAT_OFF);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
        this->parent_->send_client_command(SendspinCommandType::REPEAT_ONE);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ALL:
        this->parent_->send_client_command(SendspinCommandType::REPEAT_ALL);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_SHUFFLE:
        this->parent_->send_client_command(SendspinCommandType::SHUFFLE);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNSHUFFLE:
        this->parent_->send_client_command(SendspinCommandType::UNSHUFFLE);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_NEXT:
        this->parent_->send_client_command(SendspinCommandType::NEXT);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PREVIOUS:
        this->parent_->send_client_command(SendspinCommandType::PREVIOUS);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_GROUP_JOIN:
        this->parent_->send_client_command(SendspinCommandType::SWITCH);
        break;
#if defined(USE_SENDSPIN_PLAYER)
      // TODO: Send volume commands to server if we aren't a player
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->is_muted_ = true;
        this->speaker_->set_mute_state(this->is_muted_);
        this->force_publish_state_ = true;
        this->parent_->update_muted(this->is_muted_);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->is_muted_ = false;
        this->speaker_->set_mute_state(this->is_muted_);
        this->force_publish_state_ = true;
        this->parent_->update_muted(this->is_muted_);
        break;
#endif
      default:
        break;
    }
  }
}

#if defined(USE_SENDSPIN_PLAYER)
void SendspinMediaPlayer::sync_speaker_volume() {
  float bounded_volume = this->speaker_->get_volume();
  float unbounded_volume = (bounded_volume - this->volume_min_) / (this->volume_max_ - this->volume_min_);
  this->volume = unbounded_volume;
  this->parent_->update_volume(static_cast<uint8_t>(unbounded_volume * 100.0f));
  this->publish_state();
}

void SendspinMediaPlayer::sync_task(void *params) {
  /* This is the magic for playing synced audio. We push audio through the stack keeping careful track of the amount and
   * timing. We process the speaker callbacks to determine when audio will actually play.
   * TODO: Generalize this and put it in the audio component so that other synced audio protocols can use it
   */
  SendspinMediaPlayer *this_sendspin = (SendspinMediaPlayer *) params;

  xEventGroupSetBits(this_sendspin->event_group_, EventGroupBits::TASK_STARTING);
  {
    std::shared_ptr<SendspinAudioChunk> decoded_chunk = nullptr;

    audio::AudioStreamInfo current_stream_info;
    size_t bytes_per_frame = current_stream_info.frames_to_bytes(1);

    std::unique_ptr<audio::AudioSinkTransferBuffer> output_transfer_buffer =
        audio::AudioSinkTransferBuffer::create_inplace();
    std::unique_ptr<audio::AudioSinkTransferBuffer> interpolation_transfer_buffer =
        audio::AudioSinkTransferBuffer::create(current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS));
    output_transfer_buffer->set_sink(this_sendspin->speaker_);
    interpolation_transfer_buffer->set_sink(this_sendspin->speaker_);

    bool release_chunk = true;
    bool initial_decode = true;

    int64_t pending_frame_corrections = 0;

    int synced_chunks = 0;

    std::deque<InternalAudioTiming> chunk_timings;

    this_sendspin->pending_frames_ = 0;

    std::optional<int64_t> last_error;

    int64_t temporary_hard_sync_threshold = HARD_SYNC_THRESHOLD_US;

    this_sendspin->pending_frames_ = 0;

    this_sendspin->single_frames_added_ = 0;
    this_sendspin->single_frames_removed_ = 0;
    this_sendspin->hard_sync_added_frames_ = 0;
    this_sendspin->hard_sync_removed_frames_ = 0;

    xEventGroupSetBits(this_sendspin->event_group_, EventGroupBits::TASK_RUNNING);
    while (!(xEventGroupGetBits(this_sendspin->event_group_) & CONTROL_STOP)) {
      const uint32_t duration_in_transfer_buffers = current_stream_info.bytes_to_ms(
          output_transfer_buffer->available() + interpolation_transfer_buffer->available());

      size_t bytes_written =
          interpolation_transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(duration_in_transfer_buffers / 2), false);

      if ((bytes_written > 0) && initial_decode) {
        // Sent initial zeros, delay slightly to give it some time to work through the audio stack
        vTaskDelay(pdMS_TO_TICKS(current_stream_info.bytes_to_ms(bytes_written) / 2));
      }

      if (interpolation_transfer_buffer->available() == 0) {
        // No interpolation bytes available, send main audio data
        output_transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(3 * duration_in_transfer_buffers / 2), false);
      }

      if ((output_transfer_buffer->available() == 0) && (decoded_chunk != nullptr) && release_chunk) {
        decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
        release_chunk = false;
      }

      if (interpolation_transfer_buffer->available() + output_transfer_buffer->available() > 0) {
        // Some audio still needs to be sent, loop back around
        continue;
      }

      if (decoded_chunk == nullptr) {
        // auto chunk = this_sendspin->decoded_chunk_queue_->pop();
        auto chunk = this_sendspin->decoded_chunk_queue_->receive_chunk(pdMS_TO_TICKS(15));
        if (!chunk) {
          // No chunk available to process
          // vTaskDelay(pdMS_TO_TICKS(15));
          continue;
        }
        decoded_chunk = std::static_pointer_cast<SendspinAudioChunk>(chunk);
      }

      // Loaded a new chunk of audio into decoded_chunk

      if (current_stream_info != this_sendspin->audio_stream_info_) {
        // This shouldn't change in the middle of a session...
        current_stream_info = this_sendspin->audio_stream_info_;
        this_sendspin->speaker_->set_audio_stream_info(current_stream_info);

        bytes_per_frame = current_stream_info.frames_to_bytes(1);
        if (interpolation_transfer_buffer->capacity() <
            current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS)) {
          interpolation_transfer_buffer->reallocate(current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS));
        }
      }

      if (esp_timer_get_time() - decoded_chunk->timestamp > 0) {
        // Chunk was already supposed to play, skip it!
        ESP_LOGE(TAG, "Chunk was already supposed to play at %" PRId64 " and its %" PRId64 ", so skipping it",
                 decoded_chunk->timestamp, esp_timer_get_time());
        decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
        release_chunk = false;
        this_sendspin->decoded_chunk_queue_->reset();  // We are way behind, so drop any pending audio
        continue;
      }

      uint32_t chunk_frame_count = current_stream_info.bytes_to_frames(decoded_chunk->get_usable_size());
      /*
       * Determine the current sync error using the playback information from the speaker.
       */
      PlaybackProgress playback_progress;

      if (this_sendspin->decoded_chunk_queue_->size() == 1) {
        ESP_LOGW(TAG, "Potential buffer underflow incoming");
      }

      int64_t finish_timestamp = 0;
      while (!chunk_timings.empty() &&
             (xQueueReceive(this_sendspin->playback_progress_queue_, &playback_progress, 0) == pdTRUE)) {
        uint32_t frames_played = playback_progress.frames_played;

        if (frames_played && initial_decode) {
          // Some sent audio chunks have now been played by the speaker
          initial_decode = false;
        }

        finish_timestamp = playback_progress.finish_timestamp;
        // this_sendspin->pending_frames_ -= std::max(frames_played, this_sendspin->pending_frames_);
        InternalAudioTiming *front_chunk = &chunk_timings.front();

        pending_frame_corrections -= front_chunk->frame_corrections;
        front_chunk->frame_corrections = 0;

        while (front_chunk->total_frames < frames_played) {
          frames_played -= front_chunk->total_frames;

          chunk_timings.pop_front();
          if (chunk_timings.empty()) {
            // This should never happen if the output speaker was fully stopped with all audio
            break;
          }
          front_chunk = &chunk_timings.front();

          pending_frame_corrections -= front_chunk->frame_corrections;
          front_chunk->frame_corrections = 0;
        }

        // Now we are in the middle of the current audio chunk
        if (chunk_timings.empty()) {
          // Catastrophic error, queue a stop and start control
          SendspinControls control_type = SendspinControls::STOP;
          xQueueSend(this_sendspin->sendspin_controls_queue_, &control_type, portMAX_DELAY);
          control_type = SendspinControls::START;
          xQueueSend(this_sendspin->sendspin_controls_queue_, &control_type, portMAX_DELAY);
          ESP_LOGE(TAG, "Catastrophic sync error. Restarting sync task");
        } else {
          chunk_timings.front().total_frames -= frames_played;
        }
      }
      if (!chunk_timings.empty() && (finish_timestamp != 0)) {
        uint32_t unplayed_frames = chunk_timings.front().total_frames;

        int64_t unplayed_ms = current_stream_info.frames_to_milliseconds_with_remainder(&unplayed_frames);
        int64_t unplayed_us =
            1000LL * unplayed_ms + static_cast<int64_t>(current_stream_info.frames_to_microseconds(unplayed_frames));

        int64_t timestamp_finished = chunk_timings.front().timestamp - unplayed_us;

        last_error = timestamp_finished - finish_timestamp;
      }

      if (!last_error.has_value() && !initial_decode) {
        // Unless we are just starting (initial_decode), always wait until a new error measurement is available
        // before trying to process more audio. This way we don't correct for an error twice
        vTaskDelay(pdMS_TO_TICKS(15));
        continue;
      }

      int64_t signed_pending_duration_corrections =
          (pending_frame_corrections * 1000000LL) / static_cast<int64_t>(current_stream_info.get_sample_rate());

      // Takes into account the pending error
      int64_t recent_error_us = last_error.value_or(0) - signed_pending_duration_corrections;

      if (abs(last_error.value_or(0)) < HARD_SYNC_THRESHOLD_US) {
        synced_chunks = std::min(synced_chunks + 1, GOOD_SYNCS_BEFORE_UNMUTE);
        temporary_hard_sync_threshold = HARD_SYNC_THRESHOLD_US;  // go back to large threshold
      } else if (abs(recent_error_us) > HARD_SYNC_THRESHOLD_US) {
        // Even with the upcoming adjustments we are out of sync, reset the count
        synced_chunks = 0;
        temporary_hard_sync_threshold = HARD_RESYNC_THRESHOLD_US;
      }

      // Only mute/unmute for out of sync if we are receiving audio data
      if ((synced_chunks < GOOD_SYNCS_BEFORE_UNMUTE) && (!this_sendspin->speaker_->get_mute_state())) {
        ESP_LOGD(TAG, "Out of sync, muting output until corrected");
        ++this_sendspin->audible_syncs_;
        this_sendspin->speaker_->set_mute_state(true);
      } else if ((synced_chunks >= GOOD_SYNCS_BEFORE_UNMUTE) &&
                 (this_sendspin->is_muted_ != this_sendspin->speaker_->get_mute_state())) {
        ESP_LOGD(TAG, "In sync with server, setting mute state to existing setting");
        this_sendspin->speaker_->set_mute_state(this_sendspin->is_muted_);
      }

      InternalAudioTiming timings;
      int32_t frame_corrections = 0;

      if (initial_decode || (recent_error_us > temporary_hard_sync_threshold)) {
        // Audio hasn't started or we are too far ahead, so insert many zeros

        // Keep the chunk for later processing (don't release yet)
        release_chunk = false;

        // Remove any new chunk data from the transfer buffer and zero out the transfer buffer
        interpolation_transfer_buffer->decrease_buffer_length(interpolation_transfer_buffer->available());
        const size_t zeroed_bytes = interpolation_transfer_buffer->free();
        std::memset((void *) interpolation_transfer_buffer->get_buffer_end(), 0, zeroed_bytes);

        size_t silence_bytes_for_correction =
            current_stream_info.ms_to_bytes(static_cast<uint32_t>(abs(recent_error_us)) / 1000);

        if (silence_bytes_for_correction < zeroed_bytes) {
          // Silencing this chunk will get us precisely in sync, so correct in microseconds
          const uint32_t frames_to_silence = (abs(recent_error_us) * current_stream_info.get_sample_rate()) / 1000000;
          silence_bytes_for_correction = current_stream_info.frames_to_bytes(frames_to_silence);
        }

        size_t actual_bytes_of_silence = std::min(silence_bytes_for_correction, zeroed_bytes);
        if (initial_decode) {
          // Always send a full set of zeros when starting a new stream
          actual_bytes_of_silence = zeroed_bytes;
        }
        interpolation_transfer_buffer->increase_buffer_length(actual_bytes_of_silence);
        frame_corrections = current_stream_info.bytes_to_frames(actual_bytes_of_silence);

        ESP_LOGD(TAG,
                 "Hard sync: adding %" PRId32 " frames of silence. Current error is %" PRId64 "us. There are %" PRId64
                 " pending frames for correction, and the deque has %zu entries",
                 frame_corrections, recent_error_us, pending_frame_corrections, chunk_timings.size());
        ++this_sendspin->hard_sync_added_frames_;

        timings.timestamp = decoded_chunk->timestamp;
        timings.total_frames = frame_corrections;
        timings.frame_corrections = frame_corrections;
        pending_frame_corrections += frame_corrections;

        this_sendspin->pending_frames_ += timings.total_frames;

        chunk_timings.push_back(timings);
        last_error.reset();  // We're accounted for the most recent error
        continue;
      }

      release_chunk = true;
      output_transfer_buffer->change_inplace_buffer(decoded_chunk->get_data(), decoded_chunk->size);

      if (recent_error_us < -temporary_hard_sync_threshold) {
        // Hard sync because we have gotten ahead and need to skip some audio to get in sync
        // Removes newly decoded frames (but will always leave a minimum of 1 frame)

        size_t bytes_to_remove = current_stream_info.ms_to_bytes(abs(recent_error_us) / 1000);
        if (bytes_to_remove < decoded_chunk->get_usable_size() - bytes_per_frame) {
          // Trimming this chunk will get us precisely in sync, so correct in microseconds
          const uint32_t frames_to_remove = (abs(recent_error_us) * current_stream_info.get_sample_rate()) / 1000000;
          bytes_to_remove = current_stream_info.frames_to_bytes(frames_to_remove);
        }

        size_t actual_bytes_to_remove = std::min(bytes_to_remove, decoded_chunk->get_usable_size() - bytes_per_frame);

        output_transfer_buffer->decrease_buffer_length(actual_bytes_to_remove);

        // TODO: Is this right? Coudln't I just use get_buffer_start?
        size_t bytes_to_silence = decoded_chunk->get_usable_size() - actual_bytes_to_remove;
        std::memset((void *) (output_transfer_buffer->get_buffer_end() - bytes_to_silence), 0, bytes_to_silence);

        frame_corrections = -current_stream_info.bytes_to_frames(actual_bytes_to_remove);

        uint32_t total_frames_kept =
            current_stream_info.bytes_to_frames(decoded_chunk->get_usable_size()) + frame_corrections;

        ESP_LOGD(TAG,
                 "Hard sync: removing %" PRId32 " frames and keeping %" PRIu32 " frames. Current error is %" PRId64
                 "us. There are %" PRId64 " pending frames for correction, and the deque has %zu entries",
                 frame_corrections, total_frames_kept, recent_error_us, pending_frame_corrections,
                 chunk_timings.size());
        ++this_sendspin->hard_sync_removed_frames_;
      } else if (recent_error_us < -SOFT_SYNC_THRESHOLD_US) {
        // Small sync adjustment after getting slightly ahead.
        // Removes the last frame in the chunk to get in sync. The second to last frame is replaced with the average
        // of it and the removed frame to minimize audible glitches.

        const uint32_t num_channels = current_stream_info.get_channels();
        const uint32_t bytes_per_sample = bytes_per_frame / num_channels;

        if (output_transfer_buffer->available() >= 2 * bytes_per_frame) {
          for (uint32_t chan = 0; chan < num_channels; ++chan) {
            const int32_t first_sample = audio::unpack_audio_sample_to_q31(
                output_transfer_buffer->get_buffer_end() - 2 * bytes_per_frame + chan * bytes_per_sample,
                bytes_per_sample);
            const int32_t second_sample = audio::unpack_audio_sample_to_q31(
                output_transfer_buffer->get_buffer_end() - bytes_per_frame + chan * bytes_per_sample, bytes_per_sample);
            int32_t replacement_sample = (first_sample + second_sample) / 2;
            audio::pack_q31_as_audio_sample(
                replacement_sample,
                output_transfer_buffer->get_buffer_end() - 2 * bytes_per_frame + chan * bytes_per_sample,
                bytes_per_sample);
          }

          output_transfer_buffer->decrease_buffer_length(bytes_per_frame);
          frame_corrections = -1;
          ++this_sendspin->single_frames_removed_;
        }
      } else if (recent_error_us > SOFT_SYNC_THRESHOLD_US) {
        // Small sync adjustment after getting slightly behind.
        // Adds one new frame to get in sync. The new frame is inserted between the first and second frames.
        // The new frame is the average of the first two frames in the chunk to minimize audible glitches.

        if ((interpolation_transfer_buffer->free() >= bytes_per_frame) &&
            (output_transfer_buffer->available() >= 2 * bytes_per_frame)) {
          const uint32_t num_channels = current_stream_info.get_channels();
          const uint32_t bytes_per_sample = bytes_per_frame / num_channels;

          for (uint32_t chan = 0; chan < num_channels; ++chan) {
            const int32_t first_sample = audio::unpack_audio_sample_to_q31(
                output_transfer_buffer->get_buffer_start() + chan * bytes_per_sample, bytes_per_sample);
            const int32_t second_sample = audio::unpack_audio_sample_to_q31(
                output_transfer_buffer->get_buffer_start() + chan * bytes_per_sample + bytes_per_frame,
                bytes_per_sample);
            int32_t new_sample = (first_sample + second_sample) / 2;
            audio::pack_q31_as_audio_sample(
                new_sample, output_transfer_buffer->get_buffer_start() + chan * bytes_per_sample, bytes_per_sample);
            audio::pack_q31_as_audio_sample(first_sample,
                                            interpolation_transfer_buffer->get_buffer_start() + chan * bytes_per_sample,
                                            bytes_per_sample);
          }
          interpolation_transfer_buffer->increase_buffer_length(bytes_per_frame);
          frame_corrections = 1;
          ++this_sendspin->single_frames_added_;
        }
      }
      uint32_t new_frames = chunk_frame_count;
      const int64_t new_duration_ms = current_stream_info.frames_to_milliseconds_with_remainder(&new_frames);
      const int64_t new_duration_us = new_duration_ms * 1000LL + current_stream_info.frames_to_microseconds(new_frames);

      timings.timestamp = decoded_chunk->timestamp + new_duration_us;
      timings.total_frames = chunk_frame_count + frame_corrections;
      timings.frame_corrections = frame_corrections;
      pending_frame_corrections += frame_corrections;

      this_sendspin->pending_frames_ += timings.total_frames;

      chunk_timings.push_back(timings);
      last_error.reset();  // We're accounted for the most recent error

      static uint32_t high_water_mark = 8192;
      uint32_t new_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
      if (new_high_water_mark < high_water_mark) {
        ESP_LOGD(TAG, "Sync task - High water mark changed from %d to %d.", high_water_mark, new_high_water_mark);
        high_water_mark = new_high_water_mark;
      }
    }

    xEventGroupSetBits(this_sendspin->event_group_, EventGroupBits::TASK_STOPPING);
    if (decoded_chunk != nullptr) {
      decoded_chunk = nullptr;  // shared_ptr automatically handles cleanup
    }
  }

  // Processes the stop command by stopping the speaker and resetting all states
  this_sendspin->speaker_->stop();

  while (!this_sendspin->speaker_->is_stopped()) {
    vTaskDelay(pdMS_TO_TICKS(15));
  }

  // Ensure we restore the proper mute state in case the stream was out of sync at the end
  this_sendspin->speaker_->set_mute_state(this_sendspin->is_muted_);

  xEventGroupSetBits(this_sendspin->event_group_, EventGroupBits::TASK_STOPPED);

  while (true) {
    // Continuously delay until the loop method deletes the task
    vTaskDelay(pdMS_TO_TICKS(25));
  }
}
#endif

}  // namespace sendspin
}  // namespace esphome
#endif
