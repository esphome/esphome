#include "resonate_media_player.h"

#if defined(USE_ESP_IDF) && defined(USE_MEDIA_PLAYER) && defined(USE_RESONATE_AUDIO)

#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <memory>
#include <optional>

#include <esp_timer.h>

namespace esphome {
namespace resonate {

static const char *const TAG = "resonate.media_player";

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
  WARNING_ENCODED_CHUNK_FULL = (1 << 11),
};

void ResonateMediaPlayer::setup() {
  this->decoded_chunk_queue_ = ResonateChunkQueue::create(DECODED_CHUNK_QUEUE_SIZE);
  if (this->decoded_chunk_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create decoded chunk data queue.");
    this->mark_failed();
  }

  this->playback_progress_queue_ = xQueueCreateWithCaps(50, sizeof(PlaybackProgress), MALLOC_CAP_SPIRAM);
  if (this->playback_progress_queue_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create playback progress queue.");
    this->mark_failed();
  }

  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Couldn't create event group.");
    this->mark_failed();
  }

  this->speaker_->add_audio_output_callback([this](uint32_t frames_played, int64_t finish_timestamp) {
    PlaybackProgress playback_progress = {.frames_played = frames_played, .finish_timestamp = finish_timestamp};
    if (!xQueueSend(this->playback_progress_queue_, &playback_progress, 0)) {
      ESP_LOGE(TAG, "Playback info queue was full");
    }
  });

  this->parent_->add_audio_chunk_callback(
      [this](AudioChunk &audio_chunk, TickType_t ticks_to_wait, const audio::AudioStreamInfo &stream_info) {
        this->audio_stream_info_ = stream_info;

        return this->decoded_chunk_queue_->add_chunk(&audio_chunk, ticks_to_wait);
      });

  this->parent_->add_controls_callback([this](const ResonateControls &control_type) {
    switch (control_type) {
      case ResonateControls::START: {
        xEventGroupSetBits(this->event_group_, CONTROL_START);
        break;
      }
      case ResonateControls::STOP: {
        xEventGroupSetBits(this->event_group_, CONTROL_STOP);
        break;
      }
    }
  });

  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->publish_state();
}

void ResonateMediaPlayer::loop() {
  // Determine state of the media player
  media_player::MediaPlayerState old_state = this->state;

  if (xEventGroupGetBits(this->event_group_) & CONTROL_START) {
    if (this->sync_task_stack_buffer_ == nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        this->sync_task_stack_buffer_ = stack_allocator.allocate(SYNC_TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        this->sync_task_stack_buffer_ = stack_allocator.allocate(SYNC_TASK_STACK_SIZE);
      }
      this->sync_task_handle_ =
          xTaskCreateStatic(sync_task, "resonate_sync", SYNC_TASK_STACK_SIZE, (void *) this, SYNC_TASK_PRIORITY,
                            this->sync_task_stack_buffer_, &this->sync_task_stack_);
    }
  }

  if (this->volume_.has_value()) {
    this->volume = static_cast<float>(this->volume_.value()) / 100.0f;
    this->speaker_->set_volume(this->volume);

    this->publish_state();
    this->volume_.reset();
  }

#ifdef USE_RESONATE_SENSOR
  static int64_t last_sensor_update = esp_timer_get_time();
  if (esp_timer_get_time() - last_sensor_update > 5000000) {
    last_sensor_update = esp_timer_get_time();

    this->parent_->update_resonate_sensor({.type = ResonateSensorTypes::SINGLE_SYNC_FRAMES_ADDED,
                                           .value = static_cast<float>(this->single_frames_added_)});
    this->parent_->update_resonate_sensor({.type = ResonateSensorTypes::SINGLE_SYNC_FRAMES_REMOVED,
                                           .value = static_cast<float>(this->single_frames_removed_)});
    this->parent_->update_resonate_sensor({.type = ResonateSensorTypes::HARD_SYNC_FRAMES_ADDED,
                                           .value = static_cast<float>(this->hard_sync_added_frames_)});
    this->parent_->update_resonate_sensor({.type = ResonateSensorTypes::HARD_SYNC_FRAMES_REMOVED,
                                           .value = static_cast<float>(this->hard_sync_removed_frames_)});
    this->parent_->update_resonate_sensor(
        {.type = ResonateSensorTypes::AUDIBLE_SYNCS, .value = static_cast<float>(this->audible_syncs_)});
  }

  if ((this->state != old_state) || (this->force_publish_state_)) {
    this->force_publish_state_ = false;
    this->publish_state();
    ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
  }
#endif
}

media_player::MediaPlayerTraits ResonateMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();

  traits.set_supports_pause(true);

  return traits;
}

void ResonateMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (!this->is_ready() || this->is_failed()) {
    // Ignore any commands sent before the media player is setup
    return;
  }

  if (call.get_volume().has_value()) {
    this->volume_ = std::roundf(call.get_volume().value() * 100.0f);
  }

  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:        // intentional fallthrough
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:       // intentional fallthrough
      case media_player::MEDIA_PLAYER_COMMAND_STOP:        // intentional fallthrough
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF:  // intentional fallthrough
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE:  // intentional fallthrough
      case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST:
        this->parent_->send_stream_command(call);  // Forward commands to the resonate server
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->speaker_->set_mute_state(true);
        this->is_muted_ = true;
        this->force_publish_state_ = true;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->speaker_->set_mute_state(false);
        this->is_muted_ = false;
        this->force_publish_state_ = true;
        break;
      default:
        break;
    }
  }
}

void ResonateMediaPlayer::sync_task(void *params) {
  /* This is the magic for playing synced audio. We push audio through the stack keeping careful track of the amount and
   * timing. We process the speaker callbacks to determine when audio will actually play.
   * TODO: Generalize this and put it in the audio component so that other synced audio protocols can use it
   */
  ResonateMediaPlayer *this_resonate = (ResonateMediaPlayer *) params;

  AudioChunk decoded_chunk;
  decoded_chunk.data = nullptr;

  audio::AudioStreamInfo current_stream_info;
  size_t bytes_per_frame = current_stream_info.frames_to_bytes(1);

  std::unique_ptr<audio::AudioSinkTransferBuffer> output_transfer_buffer =
      audio::AudioSinkTransferBuffer::create_inplace();
  std::unique_ptr<audio::AudioSinkTransferBuffer> interpolation_transfer_buffer =
      audio::AudioSinkTransferBuffer::create(current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS));
  output_transfer_buffer->set_sink(this_resonate->speaker_);
  interpolation_transfer_buffer->set_sink(this_resonate->speaker_);

  bool receive_chunk = true;
  bool initial_decode = true;

  int64_t pending_frame_corrections = 0;

  int synced_chunks = 0;

  std::deque<InternalAudioTiming> chunk_timings;

  this_resonate->pending_frames_ = 0;

  std::optional<int64_t> last_error;

  int64_t temporary_hard_sync_threshold = HARD_SYNC_THRESHOLD_US;

  while (true) {
    EventBits_t event_bits = xEventGroupGetBits(this_resonate->event_group_);
    if (event_bits & CONTROL_STOP) {
      // Processes the stop command by stopping the speaker and resetting all states
      this_resonate->speaker_->stop();

      while (!this_resonate->speaker_->is_stopped()) {
        vTaskDelay(pdMS_TO_TICKS(15));
      }

      interpolation_transfer_buffer->decrease_buffer_length(interpolation_transfer_buffer->available());
      output_transfer_buffer->decrease_buffer_length(output_transfer_buffer->available());

      last_error.reset();
      pending_frame_corrections = 0;
      chunk_timings.clear();
      xQueueReset(this_resonate->playback_progress_queue_);

      // Clear the current chunk, if any is pending
      this_resonate->decoded_chunk_queue_->receive_chunk(true);
      decoded_chunk.data = nullptr;
      receive_chunk = false;

      this_resonate->decoded_chunk_queue_->reset();

      this_resonate->pending_frames_ = 0;

      this_resonate->single_frames_added_ = 0;
      this_resonate->single_frames_removed_ = 0;
      this_resonate->hard_sync_added_frames_ = 0;
      this_resonate->hard_sync_removed_frames_ = 0;

      initial_decode = true;

      // Ensure we restore the proper mute state in case the stream was out of sync at the end
      this_resonate->speaker_->set_mute_state(this_resonate->is_muted_);

      xEventGroupClearBits(this_resonate->event_group_, CONTROL_STOP);
    }

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

    if ((output_transfer_buffer->available() == 0) && (decoded_chunk.data != nullptr) && receive_chunk) {
      if (this_resonate->decoded_chunk_queue_->receive_chunk(true)) {
        // Deallocated the current chunk
        decoded_chunk.data = nullptr;
        receive_chunk = false;
      } else {
        // Failed to remove the chunk from the queue, try again
        continue;
      }
    }

    if (interpolation_transfer_buffer->available() + output_transfer_buffer->available() > 0) {
      // Some audio still needs to be sent, loop back around
      continue;
    }

    if (!this_resonate->decoded_chunk_queue_->peek_chunk(&decoded_chunk, pdMS_TO_TICKS(15))) {
      // No new chunks available to process
      continue;
    }

    // Loaded a new chunk of audio into decoded_chunk

    if (current_stream_info != this_resonate->audio_stream_info_) {
      // This shouldn't change in the middle of a session...
      current_stream_info = this_resonate->audio_stream_info_;
      this_resonate->speaker_->set_audio_stream_info(current_stream_info);

      bytes_per_frame = current_stream_info.frames_to_bytes(1);
      if (interpolation_transfer_buffer->capacity() < current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS)) {
        interpolation_transfer_buffer->reallocate(current_stream_info.ms_to_bytes(INITIAL_SYNC_ZEROS_DURATION_MS));
      }
    }

    if (esp_timer_get_time() - decoded_chunk.server_timestamp > 0) {
      // Chunk was already supposed to play, skip it!
      if (this_resonate->decoded_chunk_queue_->receive_chunk(true)) {
        ESP_LOGE(TAG, "Chunk was already supposed to play, skipping it");
        decoded_chunk.data = nullptr;
        receive_chunk = false;
        continue;
      }
    }

    /*
     * Determine the current sync error using the playback information from the speaker.
     */
    PlaybackProgress playback_progress;

    if (this_resonate->decoded_chunk_queue_->size() == 1) {
      ESP_LOGW(TAG, "Potential buffer underflow incoming");
    }

    int64_t finish_timestamp = 0;
    while (!chunk_timings.empty() &&
           (xQueueReceive(this_resonate->playback_progress_queue_, &playback_progress, 0) == pdTRUE)) {
      if (initial_decode) {
        // Some sent audio chunks have now been played by the speaker
        initial_decode = false;
      }

      uint32_t frames_played = playback_progress.frames_played;
      finish_timestamp = playback_progress.finish_timestamp;
      // this_resonate->pending_frames_ -= std::max(frames_played, this_resonate->pending_frames_);
      InternalAudioTiming *front_chunk = &chunk_timings.front();

      pending_frame_corrections -= front_chunk->frame_corrections;
      front_chunk->frame_corrections = 0;

      while (front_chunk->total_frames < frames_played) {
        frames_played -= front_chunk->total_frames;

        chunk_timings.pop_front();
        front_chunk = &chunk_timings.front();

        pending_frame_corrections -= front_chunk->frame_corrections;
        front_chunk->frame_corrections = 0;
      }

      // Now we are in the middle of the current audio chunk

      chunk_timings.front().total_frames -= frames_played;
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
    if ((synced_chunks < GOOD_SYNCS_BEFORE_UNMUTE) && (!this_resonate->speaker_->get_mute_state())) {
      ESP_LOGD(TAG, "Out of sync, muting output until corrected");
      ++this_resonate->audible_syncs_;
      this_resonate->speaker_->set_mute_state(true);
    } else if ((synced_chunks >= GOOD_SYNCS_BEFORE_UNMUTE) &&
               (this_resonate->is_muted_ != this_resonate->speaker_->get_mute_state())) {
      ESP_LOGD(TAG, "In sync with server, setting mute state to existing setting");
      this_resonate->speaker_->set_mute_state(this_resonate->is_muted_);
    }

    InternalAudioTiming timings;
    int32_t frame_corrections = 0;

    if (initial_decode || (recent_error_us > temporary_hard_sync_threshold)) {
      // Audio hasn't started or we are too far ahead, so insert many zeros

      decoded_chunk.data = nullptr;  // Ensure we do not deallocate the chunk's data
      receive_chunk = false;

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
      ++this_resonate->hard_sync_added_frames_;

      timings.timestamp = decoded_chunk.server_timestamp;
      timings.total_frames = frame_corrections;
      timings.frame_corrections = frame_corrections;
      pending_frame_corrections += frame_corrections;

      this_resonate->pending_frames_ += timings.total_frames;

      chunk_timings.push_back(timings);
      last_error.reset();  // We're accounted for the most recent error
      continue;
    }

    receive_chunk = true;
    output_transfer_buffer->change_inplace_buffer(decoded_chunk.data + decoded_chunk.offset, decoded_chunk.size);

    if (recent_error_us < -temporary_hard_sync_threshold) {
      // Hard sync because we have gotten ahead and need to skip some audio to get in sync
      // Removes newly decoded frames (but will always leave a minimum of 1 frame)

      size_t bytes_to_remove = current_stream_info.ms_to_bytes(abs(recent_error_us) / 1000);
      if (bytes_to_remove < decoded_chunk.size - bytes_per_frame) {
        // Trimming this chunk will get us precisely in sync, so correct in microseconds
        const uint32_t frames_to_remove = (abs(recent_error_us) * current_stream_info.get_sample_rate()) / 1000000;
        bytes_to_remove = current_stream_info.frames_to_bytes(frames_to_remove);
      }

      size_t actual_bytes_to_remove = std::min(bytes_to_remove, decoded_chunk.size - bytes_per_frame);

      output_transfer_buffer->decrease_buffer_length(actual_bytes_to_remove);

      // TODO: Is this right? Coudln't I just use get_buffer_start?
      size_t bytes_to_silence = decoded_chunk.size - actual_bytes_to_remove;
      std::memset((void *) (output_transfer_buffer->get_buffer_end() - bytes_to_silence), 0, bytes_to_silence);

      frame_corrections = -current_stream_info.bytes_to_frames(actual_bytes_to_remove);

      uint32_t total_frames_kept = current_stream_info.bytes_to_frames(decoded_chunk.size) + frame_corrections;

      ESP_LOGD(TAG,
               "Hard sync: removing %" PRId32 " frames and keeping %" PRIu32 " frames. Current error is %" PRId64
               "us. There are %" PRId64 " pending frames for correction, and the deque has %zu entries",
               frame_corrections, total_frames_kept, recent_error_us, pending_frame_corrections, chunk_timings.size());
      ++this_resonate->hard_sync_removed_frames_;
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
        ++this_resonate->single_frames_removed_;
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
              output_transfer_buffer->get_buffer_start() + chan * bytes_per_sample + bytes_per_frame, bytes_per_sample);
          int32_t new_sample = (first_sample + second_sample) / 2;
          audio::pack_q31_as_audio_sample(
              new_sample, output_transfer_buffer->get_buffer_start() + chan * bytes_per_sample, bytes_per_sample);
          audio::pack_q31_as_audio_sample(first_sample,
                                          interpolation_transfer_buffer->get_buffer_start() + chan * bytes_per_sample,
                                          bytes_per_sample);
        }
        interpolation_transfer_buffer->increase_buffer_length(bytes_per_frame);
        frame_corrections = 1;
        ++this_resonate->single_frames_added_;
      }
    }
    uint32_t new_frames = decoded_chunk.frame_count;
    const int64_t new_duration_ms = current_stream_info.frames_to_milliseconds_with_remainder(&new_frames);
    const int64_t new_duration_us = new_duration_ms * 1000LL + current_stream_info.frames_to_microseconds(new_frames);

    timings.timestamp = decoded_chunk.server_timestamp + new_duration_us;
    timings.total_frames = decoded_chunk.frame_count + frame_corrections;
    timings.frame_corrections = frame_corrections;
    pending_frame_corrections += frame_corrections;

    this_resonate->pending_frames_ += timings.total_frames;

    chunk_timings.push_back(timings);
    last_error.reset();  // We're accounted for the most recent error

    static uint32_t high_water_mark = 8192;
    uint32_t new_high_water_mark = uxTaskGetStackHighWaterMark(nullptr);
    if (new_high_water_mark < high_water_mark) {
      ESP_LOGD(TAG, "Sync task - High water mark changed from %d to %d.", high_water_mark, new_high_water_mark);
      high_water_mark = new_high_water_mark;
    }
  }
}

}  // namespace resonate
}  // namespace esphome
#endif
