#include "speaker_source_media_player.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"

#include "esphome/components/audio/audio.h"

#include <algorithm>

namespace esphome {
namespace speaker_source {

static const uint32_t MEDIA_CONTROLS_QUEUE_LENGTH = 20;

static const char *const TAG = "speaker_source_media_player";

void SpeakerSourceMediaPlayer::setup() {
  state = media_player::MEDIA_PLAYER_STATE_IDLE;

  this->media_control_command_queue_ = xQueueCreate(MEDIA_CONTROLS_QUEUE_LENGTH, sizeof(MediaPlayerControlCommand));

  this->pref_ = global_preferences->make_preference<VolumeRestoreState>(this->get_preference_hash());

  VolumeRestoreState volume_restore_state;
  if (this->pref_.load(&volume_restore_state)) {
    this->set_volume_(volume_restore_state.volume);
    this->set_mute_state_(volume_restore_state.is_muted);
  } else {
    this->set_volume_(this->volume_initial_);
    this->set_mute_state_(false);
  }

  // Determine pipeline count: 1 if only media_speaker, 2 if both speakers configured
  size_t pipeline_count = 1;
  if (this->announcement_speaker_ != nullptr) {
    pipeline_count = 2;
  }

  // Initialize all sources with the pipeline count
  for (auto *media_source : this->media_sources_) {
    media_source->init_pipelines(pipeline_count);

    media_source->set_output_callback([this](media_source::MediaSource *src, uint8_t *data, size_t len,
                                             TickType_t ticks, audio::AudioStreamInfo stream_info, size_t pipeline) {
      return this->handle_media_output_callback_(src, data, len, ticks, stream_info, pipeline);
    });
    media_source->set_state_callback(
        [this](media_source::MediaSource *src, media_source::MediaSourceState state, size_t pipeline) {
          this->handle_media_state_callback_(src, state, pipeline);
        });

    // Set volume and mute request callbacks for sources that support volume control
    if (media_source->get_capabilities().supports_volume_control) {
      media_source->set_volume_request_callback(
          [this](media_source::MediaSource *src, float volume) { this->handle_volume_request_(src, volume); });
      media_source->set_mute_request_callback(
          [this](media_source::MediaSource *src, bool is_muted) { this->handle_mute_request_(src, is_muted); });
    }

    media_source->set_play_uri_request_callback(
        [this](media_source::MediaSource *src, const std::string &uri, size_t pipeline) {
          this->handle_play_uri_request_(src, uri, pipeline);
        });
  }

  // Register callbacks to receive playback notifications from speakers
  if (this->media_speaker_ != nullptr) {
    this->media_speaker_->add_audio_output_callback([this](uint32_t frames, int64_t timestamp) {
      this->handle_speaker_playback_callback_(frames, timestamp, MEDIA_PIPELINE);
    });
  }
  if (this->announcement_speaker_ != nullptr) {
    this->announcement_speaker_->add_audio_output_callback([this](uint32_t frames, int64_t timestamp) {
      this->handle_speaker_playback_callback_(frames, timestamp, ANNOUNCEMENT_PIPELINE);
    });
  }

  ESP_LOGI(TAG, "Set up speaker media player with %zu pipeline(s)", pipeline_count);
}

void SpeakerSourceMediaPlayer::set_playlist_delay_ms(size_t pipeline, uint32_t delay_ms) {
  if (pipeline == MEDIA_PIPELINE) {
    this->media_playlist_delay_ms_ = delay_ms;
  } else if (pipeline == ANNOUNCEMENT_PIPELINE) {
    this->announcement_playlist_delay_ms_ = delay_ms;
  }
}

void SpeakerSourceMediaPlayer::handle_speaker_playback_callback_(uint32_t frames, int64_t timestamp, size_t pipeline) {
  // Copy pointer to local variable to avoid TOCTOU race
  media_source::MediaSource *active_source =
      (pipeline == MEDIA_PIPELINE) ? this->media_active_source_ : this->announcement_active_source_;
  uint32_t *pending_frames =
      (pipeline == MEDIA_PIPELINE) ? &this->media_pending_frames_ : &this->announcement_pending_frames_;

  // Check once - if null after this, we've avoided the race
  if (active_source == nullptr) {
    return;
  }

  // Calculate how many frames belong to this source
  uint32_t source_frames = std::min(frames, *pending_frames);
  *pending_frames -= source_frames;

  if (source_frames > 0) {
    // Notify the source about the played audio
    // Safe: we've already checked active_source is not null and copied it locally
    active_source->notify_audio_played(source_frames, timestamp, pipeline);
  }
}

void SpeakerSourceMediaPlayer::handle_volume_request_(media_source::MediaSource *source, float volume) {
  // Update the media player's volume
  this->set_volume_(volume);
  this->publish_state();
}

void SpeakerSourceMediaPlayer::handle_mute_request_(media_source::MediaSource *source, bool is_muted) {
  // Update the media player's mute state
  this->set_mute_state_(is_muted);
  this->publish_state();
}

void SpeakerSourceMediaPlayer::handle_play_uri_request_(media_source::MediaSource *source, const std::string &uri,
                                                        size_t pipeline) {
  // Smart source is requesting the player to play a different URI
  // This may route to a different source based on URI prefix
  auto call = this->make_call();
  call.set_media_url(uri);
  call.set_announcement(pipeline == ANNOUNCEMENT_PIPELINE);
  call.perform();
}

void SpeakerSourceMediaPlayer::handle_media_state_callback_(media_source::MediaSource *source,
                                                            media_source::MediaSourceState state, size_t pipeline) {
  media_source::MediaSource **active_source_ptr =
      (pipeline == MEDIA_PIPELINE) ? &this->media_active_source_ : &this->announcement_active_source_;
  media_source::MediaSource **last_source_ptr =
      (pipeline == MEDIA_PIPELINE) ? &this->media_last_source_ : &this->announcement_last_source_;
  media_source::MediaSource **stopping_source_ptr =
      (pipeline == MEDIA_PIPELINE) ? &this->media_stopping_source_ : &this->announcement_stopping_source_;

  speaker::Speaker *target_speaker = (pipeline == MEDIA_PIPELINE) ? this->media_speaker_ : this->announcement_speaker_;

  if (state == media_source::MediaSourceState::IDLE) {
    // Source went idle - clear stopping flag if this was the source we asked to stop
    if (*stopping_source_ptr == source) {
      *stopping_source_ptr = nullptr;
    }

    // Source went idle - clear it if it's the active source
    if (*active_source_ptr == source) {
      *last_source_ptr = *active_source_ptr;
      *active_source_ptr = nullptr;

      // Finish the speaker to ensure it's ready for the next playback
      if (target_speaker != nullptr) {
        target_speaker->finish();
      }

      // Queue PLAYLIST_ADVANCE to handle track completion - all playlist logic is in process_control_queue_
      this->queue_command_(MediaPlayerControlCommand::PLAYLIST_ADVANCE, pipeline);
    }
  } else if (state == media_source::MediaSourceState::PLAYING) {
    // Source started playing - make it the active source if no one else is active
    if (*active_source_ptr == nullptr) {
      *active_source_ptr = source;
      *last_source_ptr = nullptr;
    }
  }
}

size_t SpeakerSourceMediaPlayer::handle_media_output_callback_(media_source::MediaSource *source, uint8_t *data,
                                                               size_t length, TickType_t ticks,
                                                               audio::AudioStreamInfo stream_info, size_t pipeline) {
  media_source::MediaSource *active_source =
      (pipeline == MEDIA_PIPELINE) ? this->media_active_source_ : this->announcement_active_source_;
  speaker::Speaker *target_speaker = (pipeline == MEDIA_PIPELINE) ? this->media_speaker_ : this->announcement_speaker_;
  uint32_t *pending_frames =
      (pipeline == MEDIA_PIPELINE) ? &this->media_pending_frames_ : &this->announcement_pending_frames_;

  if (target_speaker == nullptr) {
    vTaskDelay(ticks);
    return 0;
  }

  if (active_source == source) {
    // This source is active - play the audio
    if (target_speaker->get_audio_stream_info() != stream_info) {
      target_speaker->set_audio_stream_info(stream_info);
      vTaskDelay(ticks);
      return 0;
    }
    size_t bytes_written = target_speaker->play(data, length, ticks);
    if (bytes_written > 0) {
      // Track frames sent to speaker for this source
      *pending_frames += stream_info.bytes_to_frames(bytes_written);
    }
    return bytes_written;
  }

  // Not the active source - wait for state callback to set us as active when we transition to PLAYING
  vTaskDelay(ticks);
  return 0;
}

void SpeakerSourceMediaPlayer::loop() {
  // Process queued control commands
  this->process_control_queue_();

  // Update state based on active sources - announcement pipeline takes priority
  media_player::MediaPlayerState old_state = this->state;

  // Check playlist state to detect transitions between items
  bool announcement_has_next_item =
      (this->announcement_playlist_index_ < this->announcement_playlist_.size()) ||
      (this->announcement_repeat_mode_ != REPEAT_OFF && !this->announcement_playlist_.empty());
  bool media_has_next_item = (this->media_playlist_index_ < this->media_playlist_.size()) ||
                             (this->media_repeat_mode_ != REPEAT_OFF && !this->media_playlist_.empty());

  // Check announcement pipeline first
  // Copy pointer to local variable to avoid TOCTOU race
  media_source::MediaSource *announcement_source = this->announcement_active_source_;
  if (announcement_source != nullptr) {
    // Get state once and store it to avoid multiple dereferences
    media_source::MediaSourceState announcement_state = announcement_source->get_state(ANNOUNCEMENT_PIPELINE);
    if (announcement_state != media_source::MediaSourceState::IDLE) {
      // Announcement is active - announcements take priority and never report PAUSED
      switch (announcement_state) {
        case media_source::MediaSourceState::PLAYING:
        case media_source::MediaSourceState::PAUSED:  // Treat paused announcements as announcing
        case media_source::MediaSourceState::BUFFERING:
          this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
          break;
        case media_source::MediaSourceState::ERROR:
          this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
          ESP_LOGE(TAG, "Announcement source is in error state");
          break;
        default:
          break;
      }
    } else {
      // Announcement pipeline is idle, check media pipeline
      // Copy pointer to local variable to avoid TOCTOU race
      media_source::MediaSource *media_source = this->media_active_source_;
      if (media_source != nullptr) {
        // Get state once and store it to avoid multiple dereferences
        media_source::MediaSourceState media_state = media_source->get_state(MEDIA_PIPELINE);
        switch (media_state) {
          case media_source::MediaSourceState::IDLE:
            this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
            break;
          case media_source::MediaSourceState::PAUSED:
            this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
            break;
          case media_source::MediaSourceState::PLAYING:
            this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
            break;
          case media_source::MediaSourceState::BUFFERING:
            this->state = media_player::MEDIA_PLAYER_STATE_IDLE;  // Map buffering to idle for now
            break;
          case media_source::MediaSourceState::ERROR:
            this->state = media_player::MEDIA_PLAYER_STATE_IDLE;  // Map error to idle
            ESP_LOGE(TAG, "Media source is in error state");
            break;
        }
      } else {
        // No active media source - check if transitioning to next media item
        if (media_has_next_item && old_state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
          this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        } else {
          this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
        }
      }
    }
  } else if (announcement_has_next_item && old_state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING) {
    // No active announcement source, but transitioning to next announcement item
    this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
  } else {
    // No active announcement, check media pipeline
    // Copy pointer to local variable to avoid TOCTOU race
    media_source::MediaSource *media_source = this->media_active_source_;
    if (media_source != nullptr) {
      // Get state once and store it to avoid multiple dereferences
      media_source::MediaSourceState media_state = media_source->get_state(MEDIA_PIPELINE);
      switch (media_state) {
        case media_source::MediaSourceState::IDLE:
          this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
          break;
        case media_source::MediaSourceState::PAUSED:
          this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
          break;
        case media_source::MediaSourceState::PLAYING:
          this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
          break;
        case media_source::MediaSourceState::BUFFERING:
          this->state = media_player::MEDIA_PLAYER_STATE_IDLE;  // Map buffering to idle for now
          break;
        case media_source::MediaSourceState::ERROR:
          this->state = media_player::MEDIA_PLAYER_STATE_IDLE;  // Map error to idle
          ESP_LOGE(TAG, "Media source is in error state");
          break;
      }
    } else {
      // No active media source - check if transitioning to next media item
      if (media_has_next_item && old_state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
      } else {
        this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
      }
    }
  }

  if (this->state != old_state) {
    this->publish_state();
    ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
  }
}

media_source::MediaSource *SpeakerSourceMediaPlayer::find_source_for_uri_(const std::string &uri, size_t pipeline) {
  for (auto &source : this->media_sources_) {
    const std::string &prefix = source->get_uri_prefix();
    if (uri.starts_with(prefix)) {
      // Check if this source can handle this pipeline (is it idle on this pipeline?)
      if (source->get_state(pipeline) == media_source::MediaSourceState::IDLE) {
        return source;  // First idle match wins
      }
    }
  }
  // If no idle source found, try again without checking state (will be stopped by try_execute_play_uri_)
  for (auto &source : this->media_sources_) {
    const std::string &prefix = source->get_uri_prefix();
    if (uri.starts_with(prefix)) {
      return source;  // First match wins
    }
  }
  return nullptr;
}

bool SpeakerSourceMediaPlayer::try_execute_play_uri_(const std::string &uri, size_t pipeline) {
  // Find target source
  media_source::MediaSource *target_source = this->find_source_for_uri_(uri, pipeline);
  if (target_source == nullptr) {
    ESP_LOGW(TAG, "No source found for URI: %s", uri.c_str());
    return true;  // Remove from queue (unrecoverable)
  }

  // Get the active source for this pipeline
  // Copy pointer to local variable to avoid TOCTOU race
  media_source::MediaSource *active_source =
      (pipeline == MEDIA_PIPELINE) ? this->media_active_source_ : this->announcement_active_source_;
  speaker::Speaker *target_speaker = (pipeline == MEDIA_PIPELINE) ? this->media_speaker_ : this->announcement_speaker_;

  // Get the stopping source for this pipeline
  media_source::MediaSource **stopping_source_ptr =
      (pipeline == MEDIA_PIPELINE) ? &this->media_stopping_source_ : &this->announcement_stopping_source_;

  // If active source exists and is not IDLE, stop it and wait
  if (active_source != nullptr) {
    // Get state once and store it to avoid multiple dereferences
    media_source::MediaSourceState active_state = active_source->get_state(pipeline);
    if (active_state != media_source::MediaSourceState::IDLE) {
      // Only send END command once per source - check if we've already asked this source to stop
      if (*stopping_source_ptr != active_source) {
        ESP_LOGD(TAG, "Pipeline %zu: Stopping active source before playing: %s", pipeline, uri.c_str());
        active_source->handle_command(media_source::MEDIA_SOURCE_COMMAND_END, pipeline);
        if (target_speaker != nullptr) {
          target_speaker->stop();
        }
        *stopping_source_ptr = active_source;
      }
      return false;  // Leave in queue, retry next loop
    }
  }

  // Also check target source directly - handles case where source errored before PLAYING state
  // (active_source_ was never set, but source is still in ERROR/non-IDLE state)
  media_source::MediaSourceState target_state = target_source->get_state(pipeline);
  if (target_state != media_source::MediaSourceState::IDLE) {
    // Only send END command once per source
    if (*stopping_source_ptr != target_source) {
      ESP_LOGD(TAG, "Pipeline %zu: Target source busy (state=%d), stopping before playing: %s", pipeline,
               static_cast<int>(target_state), uri.c_str());
      target_source->handle_command(media_source::MEDIA_SOURCE_COMMAND_END, pipeline);
      if (target_speaker != nullptr) {
        target_speaker->stop();
      }
      *stopping_source_ptr = target_source;
    }
    return false;  // Leave in queue, retry next loop
  }

  // Clear stopping flag since we're past the stopping phase
  *stopping_source_ptr = nullptr;

  // Check if speaker is ready
  if (!target_speaker->is_stopped()) {
    return false;  // Speaker not ready yet, retry later
  }

  // Speaker is ready, try to play
  if (!target_source->play_uri(uri, pipeline)) {
    ESP_LOGE(TAG, "Pipeline %zu: Failed to play URI: %s", pipeline, uri.c_str());
  }

  // Reset pending frame counter for this pipeline since we're starting a new source
  if (pipeline == MEDIA_PIPELINE) {
    this->media_pending_frames_ = 0;
  } else {
    this->announcement_pending_frames_ = 0;
  }

  return true;  // Remove from queue
}

void SpeakerSourceMediaPlayer::queue_command_(MediaPlayerControlCommand::Type type, size_t pipeline) {
  MediaPlayerControlCommand cmd;
  cmd.type = type;
  cmd.pipeline = pipeline;
  if (xQueueSend(this->media_control_command_queue_, &cmd, 0) != pdTRUE) {
    ESP_LOGE(TAG, "Failed to queue command type %d", static_cast<int>(type));
  }
}

void SpeakerSourceMediaPlayer::queue_play_current_(size_t pipeline, uint32_t delay_ms) {
  if (delay_ms > 0) {
    const std::string timeout_id = (pipeline == MEDIA_PIPELINE) ? "next_media" : "next_ann";
    this->set_timeout(timeout_id, delay_ms,
                      [this, pipeline]() { this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline); });
  } else {
    this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
  }
}

void SpeakerSourceMediaPlayer::process_control_queue_() {
  MediaPlayerControlCommand control_command;

  // Use peek to check command without removing it
  if (xQueuePeek(this->media_control_command_queue_, &control_command, 0) != pdTRUE) {
    return;
  }

  bool command_executed = false;
  size_t pipeline = control_command.pipeline;

  // Get active source and playlist for this pipeline
  media_source::MediaSource *active_source =
      (pipeline == MEDIA_PIPELINE) ? this->media_active_source_ : this->announcement_active_source_;
  std::vector<std::string> *playlist =
      (pipeline == MEDIA_PIPELINE) ? &this->media_playlist_ : &this->announcement_playlist_;
  size_t *playlist_index =
      (pipeline == MEDIA_PIPELINE) ? &this->media_playlist_index_ : &this->announcement_playlist_index_;
  RepeatMode *repeat_mode = (pipeline == MEDIA_PIPELINE) ? &this->media_repeat_mode_ : &this->announcement_repeat_mode_;
  uint32_t playlist_delay_ms =
      (pipeline == MEDIA_PIPELINE) ? this->media_playlist_delay_ms_ : this->announcement_playlist_delay_ms_;

  // Check if active source has internal playlist management
  bool has_internal_playlist = (active_source != nullptr) && active_source->get_capabilities().has_internal_playlist;

  switch (control_command.type) {
    case MediaPlayerControlCommand::PLAY_URI: {
      // Always use our local playlist to start playback
      const std::string timeout_id = (pipeline == MEDIA_PIPELINE) ? "next_media" : "next_ann";
      this->cancel_timeout(timeout_id);
      playlist->clear();
      *playlist_index = 0;  // Reset index
      playlist->push_back(*control_command.data.uri);

      // Queue PLAY_CURRENT to initiate playback
      this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
      command_executed = true;
      break;
    }

    case MediaPlayerControlCommand::ENQUEUE_URI: {
      // Always add to our local playlist
      bool was_empty = playlist->empty();
      playlist->push_back(*control_command.data.uri);

      if (was_empty) {
        *playlist_index = 0;  // Reset index when adding to empty playlist
      }

      // If nothing was playing, queue PLAY_CURRENT to start
      bool nothing_playing =
          (active_source == nullptr) || (active_source->get_state(pipeline) == media_source::MediaSourceState::IDLE);
      if (was_empty && nothing_playing) {
        this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
      }
      command_executed = true;
      break;
    }

    case MediaPlayerControlCommand::PLAYLIST_ADVANCE: {
      // Internal message: a track finished, advance to next
      if (*repeat_mode != REPEAT_ONE) {
        // Advance index (unless repeat_one keeps same track)
        (*playlist_index)++;
      }

      // Check if we should continue playback
      if (*playlist_index < playlist->size()) {
        // More items to play
        this->queue_play_current_(pipeline, playlist_delay_ms);
      } else if (*repeat_mode == REPEAT_ALL && !playlist->empty()) {
        // At end but repeat_all is on - wrap to beginning
        *playlist_index = 0;
        this->queue_play_current_(pipeline, playlist_delay_ms);
      }
      // else: at end with repeat_off - stay idle
      command_executed = true;
      break;
    }

    case MediaPlayerControlCommand::PLAY_CURRENT: {
      // Play the item at current playlist index
      if (*playlist_index < playlist->size()) {
        command_executed = this->try_execute_play_uri_((*playlist)[*playlist_index], pipeline);
      } else {
        command_executed = true;  // Index out of bounds or empty playlist
      }
      break;
    }

    case MediaPlayerControlCommand::SEND_COMMAND: {
      media_source::MediaSourceCommand source_command = control_command.data.source_command;

      // Handle commands that need local playlist management
      if (!has_internal_playlist) {
        switch (source_command) {
          case media_source::MEDIA_SOURCE_COMMAND_STOP: {
            // Clear the playlist to prevent auto-progression
            const std::string timeout_id = (pipeline == MEDIA_PIPELINE) ? "next_media" : "next_ann";
            this->cancel_timeout(timeout_id);
            playlist->clear();
            *playlist_index = 0;  // Reset index
            break;
          }
          case media_source::MEDIA_SOURCE_COMMAND_NEXT: {
            // NEXT from HA: force advance to next track
            if (*playlist_index + 1 < playlist->size()) {
              (*playlist_index)++;
              this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
            } else if (*repeat_mode == REPEAT_ALL && !playlist->empty()) {
              *playlist_index = 0;
              this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
            }
            // else: at end with no repeat_all - stay on current
            command_executed = true;
            break;
          }
          case media_source::MEDIA_SOURCE_COMMAND_PREVIOUS: {
            // PREVIOUS from HA: go back to previous track
            if (*playlist_index > 0) {
              (*playlist_index)--;
              this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
            } else if (*repeat_mode == REPEAT_ALL && !playlist->empty()) {
              // At beginning with repeat_all - wrap to end
              *playlist_index = playlist->size() - 1;
              this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
            }
            // else: at beginning with no repeat_all - stay on current
            command_executed = true;
            break;
          }
          case media_source::MEDIA_SOURCE_COMMAND_REPEAT_ONE:
            *repeat_mode = REPEAT_ONE;
            command_executed = true;
            break;
          case media_source::MEDIA_SOURCE_COMMAND_REPEAT_OFF:
            *repeat_mode = REPEAT_OFF;
            command_executed = true;
            break;
          case media_source::MEDIA_SOURCE_COMMAND_REPEAT_ALL:
            *repeat_mode = REPEAT_ALL;
            command_executed = true;
            break;
          case media_source::MEDIA_SOURCE_COMMAND_CLEAR_PLAYLIST: {
            // Clear playlist but keep current item playing
            const std::string timeout_id = (pipeline == MEDIA_PIPELINE) ? "next_media" : "next_ann";
            this->cancel_timeout(timeout_id);
            if (*playlist_index < playlist->size()) {
              std::string current = (*playlist)[*playlist_index];
              playlist->clear();
              playlist->push_back(current);
              *playlist_index = 0;
            } else {
              playlist->clear();
              *playlist_index = 0;
            }
            command_executed = true;
            break;
          }
          case media_source::MEDIA_SOURCE_COMMAND_SHUFFLE:
          case media_source::MEDIA_SOURCE_COMMAND_UNSHUFFLE:
            // TODO: Handle these for local playlist
            command_executed = true;
            break;
          default:
            break;
        }
      }

      // If not already handled, route command to the appropriate source
      if (!command_executed) {
        // Handle TOGGLE by checking current state
        if (source_command == media_source::MEDIA_SOURCE_COMMAND_TOGGLE) {
          if ((active_source != nullptr) &&
              (active_source->get_state(pipeline) == media_source::MediaSourceState::PLAYING)) {
            source_command = media_source::MEDIA_SOURCE_COMMAND_PAUSE;
          } else {
            source_command = media_source::MEDIA_SOURCE_COMMAND_PLAY;
          }
        }

        // Handle GROUP_JOIN - check if any source supports it
        if (source_command == media_source::MEDIA_SOURCE_COMMAND_GROUP_JOIN) {
          media_source::MediaSource *last_source =
              (pipeline == MEDIA_PIPELINE) ? this->media_last_source_ : this->announcement_last_source_;
          bool active_can_join = (active_source != nullptr) && active_source->get_capabilities().supports_group_join;
          bool last_can_join = (last_source != nullptr) && last_source->get_capabilities().supports_group_join;

          if (!active_can_join && !last_can_join) {
            command_executed = true;  // No source can handle this, just drop it
          }
        }

        if (!command_executed) {
          if (active_source != nullptr) {
            active_source->handle_command(source_command, pipeline);
          } else {
            media_source::MediaSource *last_source =
                (pipeline == MEDIA_PIPELINE) ? this->media_last_source_ : this->announcement_last_source_;
            if (last_source != nullptr) {
              last_source->handle_command(source_command, pipeline);
            }
          }
          command_executed = true;
        }
      }
      break;
    }
  }

  // Only remove from queue if successfully executed
  if (command_executed) {
    xQueueReceive(this->media_control_command_queue_, &control_command, 0);

    // Delete the allocated string for PLAY_URI and ENQUEUE_URI commands
    if (control_command.type == MediaPlayerControlCommand::PLAY_URI ||
        control_command.type == MediaPlayerControlCommand::ENQUEUE_URI) {
      delete control_command.data.uri;
    }
  }
}

void SpeakerSourceMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (!this->is_ready()) {
    // Ignore any commands sent before the media player is setup
    return;
  }

  MediaPlayerControlCommand control_command;

  // Determine which pipeline to use based on announcement flag
  if (call.get_announcement().has_value() && call.get_announcement().value() &&
      this->announcement_speaker_ != nullptr) {
    control_command.pipeline = ANNOUNCEMENT_PIPELINE;
  } else {
    control_command.pipeline = MEDIA_PIPELINE;  // Default
  }

  if (call.get_media_url().has_value()) {
    bool enqueue =
        call.get_command().has_value() && call.get_command().value() == media_player::MEDIA_PLAYER_COMMAND_ENQUEUE;

    control_command.type = enqueue ? MediaPlayerControlCommand::ENQUEUE_URI : MediaPlayerControlCommand::PLAY_URI;
    control_command.data.uri = new std::string(call.get_media_url().value());
    if (xQueueSend(this->media_control_command_queue_, &control_command, 0) != pdTRUE) {
      delete control_command.data.uri;
      ESP_LOGE(TAG, "Failed to queue command, command dropped");
    }
    return;
  }

  if (call.get_volume().has_value()) {
    this->set_volume_(call.get_volume().value());
    this->publish_state();
    return;
  }

  if (call.get_command().has_value()) {
    media_source::MediaSourceCommand media_source_command = media_source::MEDIA_SOURCE_COMMAND_NOP;

    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_PLAY;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_PAUSE;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_TOGGLE;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_STOP;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->set_mute_state_(true);
        this->publish_state();
        return;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->set_mute_state_(false);
        this->publish_state();
        return;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP:
        this->set_volume_(std::min(1.0f, this->volume + this->volume_increment_));
        this->publish_state();
        return;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
        this->set_volume_(std::max(0.0f, this->volume - this->volume_increment_));
        this->publish_state();
        return;
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ALL:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_REPEAT_ALL;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_REPEAT_ONE;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_REPEAT_OFF;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_CLEAR_PLAYLIST;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_NEXT:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_NEXT;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PREVIOUS:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_PREVIOUS;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_SHUFFLE:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_SHUFFLE;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNSHUFFLE:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_UNSHUFFLE;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_GROUP_JOIN:
        media_source_command = media_source::MEDIA_SOURCE_COMMAND_GROUP_JOIN;
        break;
      default:
        return;
    }

    control_command.type = MediaPlayerControlCommand::SEND_COMMAND;
    control_command.data.source_command = media_source_command;
    xQueueSend(this->media_control_command_queue_, &control_command, 0);
  }
}

media_player::MediaPlayerTraits SpeakerSourceMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(true);

  if (this->announcement_format_.has_value()) {
    traits.get_supported_formats().push_back(this->announcement_format_.value());
  }
  if (this->media_format_.has_value()) {
    traits.get_supported_formats().push_back(this->media_format_.value());
  }

  return traits;
};

void SpeakerSourceMediaPlayer::save_volume_restore_state_() {
  VolumeRestoreState volume_restore_state;
  volume_restore_state.volume = this->volume;
  volume_restore_state.is_muted = this->is_muted_;
  this->pref_.save(&volume_restore_state);
}

void SpeakerSourceMediaPlayer::set_mute_state_(bool mute_state) {
  if (this->media_speaker_ != nullptr) {
    this->media_speaker_->set_mute_state(mute_state);
  }
  if (this->announcement_speaker_ != nullptr) {
    this->announcement_speaker_->set_mute_state(mute_state);
  }

  bool old_mute_state = this->is_muted_;
  this->is_muted_ = mute_state;

  this->save_volume_restore_state_();

  // Notify media sources that support volume control about the mute state change
  for (auto *media_source : this->media_sources_) {
    if (media_source->get_capabilities().supports_volume_control) {
      media_source->notify_mute_changed(mute_state);
    }
  }

  if (old_mute_state != mute_state) {
    if (mute_state) {
      this->defer([this]() { this->mute_trigger_->trigger(); });
    } else {
      this->defer([this]() { this->unmute_trigger_->trigger(); });
    }
  }
}

void SpeakerSourceMediaPlayer::set_volume_(float volume, bool publish) {
  // Remap the volume to fit with in the configured limits
  float bounded_volume = remap<float, float>(volume, 0.0f, 1.0f, this->volume_min_, this->volume_max_);

  if (this->media_speaker_ != nullptr) {
    this->media_speaker_->set_volume(bounded_volume);
  }

  if (this->announcement_speaker_ != nullptr) {
    this->announcement_speaker_->set_volume(bounded_volume);
  }

  if (publish) {
    this->volume = volume;
    this->save_volume_restore_state_();
  }

  // Notify media sources that support volume control about the volume change
  for (auto *media_source : this->media_sources_) {
    if (media_source->get_capabilities().supports_volume_control) {
      media_source->notify_volume_changed(volume);
    }
  }

  // Turn on the mute state if the volume is effectively zero, off otherwise
  if (volume < 0.001) {
    this->set_mute_state_(true);
  } else {
    this->set_mute_state_(false);
  }

  this->defer([this, volume]() { this->volume_trigger_->trigger(volume); });
}

}  // namespace speaker_source
}  // namespace esphome

#endif
