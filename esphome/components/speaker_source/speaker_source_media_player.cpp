#include "speaker_source_media_player.h"

#ifdef USE_ESP32

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::speaker_source {

static constexpr uint32_t MEDIA_CONTROLS_QUEUE_LENGTH = 20;

static const char *const TAG = "speaker_source_media_player";

// SourceBinding method implementations (defined here because SpeakerSourceMediaPlayer is forward-declared in the
// header)

// THREAD CONTEXT: Called from media source decode task thread
size_t SourceBinding::write_audio(const uint8_t *data, size_t length, uint32_t timeout_ms,
                                  const audio::AudioStreamInfo &stream_info) {
  return this->player->handle_media_output_(this->pipeline, this->source, data, length, timeout_ms, stream_info);
}

// THREAD CONTEXT: Called from main loop (media source's loop() calls set_state_ which calls report_state)
void SourceBinding::report_state(media_source::MediaSourceState state) {
  this->player->handle_media_state_changed_(this->pipeline, this->source, state);
}

// THREAD CONTEXT: Called from media source task thread; uses defer() to marshal to main loop
void SourceBinding::request_volume(float volume) {
  this->player->defer([this, volume]() { this->player->handle_volume_request_(volume); });
}

// THREAD CONTEXT: Called from media source task thread; uses defer() to marshal to main loop
void SourceBinding::request_mute(bool is_muted) {
  this->player->defer([this, is_muted]() { this->player->handle_mute_request_(is_muted); });
}

// THREAD CONTEXT: Called from media source task thread; uses defer() to marshal to main loop
void SourceBinding::request_play_uri(const std::string &uri) {
  this->player->defer([this, uri]() { this->player->handle_play_uri_request_(this->pipeline, uri); });
}

// THREAD CONTEXT: Called during code generation setup (main loop)
void SpeakerSourceMediaPlayer::add_media_source(uint8_t pipeline, media_source::MediaSource *media_source) {
  auto &binding =
      this->pipelines_[pipeline].sources.emplace_back(std::make_unique<SourceBinding>(this, media_source, pipeline));
  media_source->set_listener(binding.get());
}

void SpeakerSourceMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Speaker Source Media Player:\n"
                "  Volume Increment: %.2f\n"
                "  Volume Min: %.2f\n"
                "  Volume Max: %.2f",
                this->volume_increment_, this->volume_min_, this->volume_max_);
}

void SpeakerSourceMediaPlayer::setup() {
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;

  this->media_control_command_queue_ = xQueueCreate(MEDIA_CONTROLS_QUEUE_LENGTH, sizeof(MediaPlayerControlCommand));

  this->pref_ = this->make_entity_preference<VolumeRestoreState>();

  VolumeRestoreState volume_restore_state;
  if (this->pref_.load(&volume_restore_state)) {
    this->set_volume_(volume_restore_state.volume);
    this->set_mute_state_(volume_restore_state.is_muted);
  } else {
    this->set_volume_(this->volume_initial_);
    this->set_mute_state_(false);
  }

  // Register callbacks to receive playback notifications from speakers
  for (size_t i = 0; i < this->pipelines_.size(); i++) {
    if (this->pipelines_[i].is_configured()) {
      this->pipelines_[i].speaker->add_audio_output_callback([this, i](uint32_t frames, int64_t timestamp) {
        this->handle_speaker_playback_callback_(frames, timestamp, i);
      });
    }
  }
}

// THREAD CONTEXT: Called from the speaker's playback callback task (not main loop)
void SpeakerSourceMediaPlayer::handle_speaker_playback_callback_(uint32_t frames, int64_t timestamp, uint8_t pipeline) {
  PipelineContext &ps = this->pipelines_[pipeline];

  // Load once so the null check and use below are consistent
  media_source::MediaSource *active_source = ps.active_source.load(std::memory_order_relaxed);
  if (active_source == nullptr) {
    return;
  }

  // CAS loop to safely subtract frames without underflow. If pending_frames is reset to 0 (new source
  // starting) between the load and the subtract, compare_exchange_weak will fail and reload the current value.
  uint32_t current = ps.pending_frames.load(std::memory_order_relaxed);
  uint32_t source_frames;
  do {
    source_frames = std::min(frames, current);
  } while (source_frames > 0 &&
           !ps.pending_frames.compare_exchange_weak(current, current - source_frames, std::memory_order_relaxed));

  if (source_frames > 0) {
    // Notify the source about the played audio
    active_source->notify_audio_played(source_frames, timestamp);
  }
}

// THREAD CONTEXT: Called from main loop via defer()
void SpeakerSourceMediaPlayer::handle_volume_request_(float volume) {
  // Update the media player's volume
  this->set_volume_(volume);
  this->publish_state();
}

// THREAD CONTEXT: Called from main loop via defer()
void SpeakerSourceMediaPlayer::handle_mute_request_(bool is_muted) {
  // Update the media player's mute state
  this->set_mute_state_(is_muted);
  this->publish_state();
}

// THREAD CONTEXT: Called from main loop via defer()
void SpeakerSourceMediaPlayer::handle_play_uri_request_(uint8_t pipeline, const std::string &uri) {
  // Smart source is requesting the player to play a different URI
  auto call = this->make_call();
  call.set_media_url(uri);
  call.set_announcement(pipeline == ANNOUNCEMENT_PIPELINE);
  call.perform();
}

// THREAD CONTEXT: Called from main loop (media source's loop() calls set_state_ which calls report_state)
void SpeakerSourceMediaPlayer::handle_media_state_changed_(uint8_t pipeline, media_source::MediaSource *source,
                                                           media_source::MediaSourceState state) {
  PipelineContext &ps = this->pipelines_[pipeline];

  if (state == media_source::MediaSourceState::IDLE) {
    // Track whether this IDLE was from an orchestrator-initiated stop (e.g., NEXT/PREV/PLAY_URI)
    // so we can suppress spurious PLAYLIST_ADVANCE below
    bool was_stopping = (ps.stopping_source == source);

    // Source went idle - clear stopping flag if this was the source we asked to stop
    if (was_stopping) {
      ps.stopping_source = nullptr;
    }

    // Clear pending flag if this was the source we asked to play
    if (ps.pending_source == source) {
      ps.pending_source = nullptr;
    }

    // Source went idle - clear it if it's the active source
    if (ps.active_source == source) {
      ps.last_source = ps.active_source;
      ps.active_source = nullptr;

      // Finish the speaker to ensure it's ready for the next playback
      ps.speaker->finish();

      // Only advance the playlist if the track finished naturally (not stopped by the orchestrator)
      if (!was_stopping) {
        this->queue_command_(MediaPlayerControlCommand::PLAYLIST_ADVANCE, pipeline);
      }
    }
  } else if (state == media_source::MediaSourceState::PLAYING) {
    // Source started playing - make it the active source if no one else is active
    if (ps.active_source == nullptr) {
      ps.active_source = source;
      ps.last_source = nullptr;

      // Clear pending flag now that the source is active
      if (ps.pending_source == source) {
        ps.pending_source = nullptr;
      }
    }
  }
}

// THREAD CONTEXT: Called from media source decode task thread (not main loop).
// Reads ps.active_source (atomic), writes ps.pending_frames (atomic), and calls
// ps.speaker methods (speaker pointer is immutable after setup).
size_t SpeakerSourceMediaPlayer::handle_media_output_(uint8_t pipeline, media_source::MediaSource *source,
                                                      const uint8_t *data, size_t length, uint32_t timeout_ms,
                                                      const audio::AudioStreamInfo &stream_info) {
  PipelineContext &ps = this->pipelines_[pipeline];

  // Single read; the if-body only uses ps.speaker (immutable after setup) and the source parameter.
  if (ps.active_source == source) {
    // This source is active - play the audio
    if (ps.speaker->get_audio_stream_info() != stream_info) {
      // Setup the speaker to play this stream
      ps.speaker->set_audio_stream_info(stream_info);
      vTaskDelay(pdMS_TO_TICKS(timeout_ms));
      return 0;
    }
    size_t bytes_written = ps.speaker->play(data, length, pdMS_TO_TICKS(timeout_ms));
    if (bytes_written > 0) {
      // Track frames sent to speaker for this source
      ps.pending_frames.fetch_add(stream_info.bytes_to_frames(bytes_written), std::memory_order_relaxed);
    }
    return bytes_written;
  }

  // Not the active source - wait for state callback to set us as active when we transition to PLAYING
  vTaskDelay(pdMS_TO_TICKS(timeout_ms));
  return 0;
}

// THREAD CONTEXT: Called from main loop (loop)
media_player::MediaPlayerState SpeakerSourceMediaPlayer::get_source_state_(
    media_source::MediaSource *source, bool playlist_active, media_player::MediaPlayerState old_state) const {
  if (source != nullptr) {
    switch (source->get_state()) {
      case media_source::MediaSourceState::PLAYING:
        return media_player::MEDIA_PLAYER_STATE_PLAYING;
      case media_source::MediaSourceState::PAUSED:
        return media_player::MEDIA_PLAYER_STATE_PAUSED;
      case media_source::MediaSourceState::ERROR:
        ESP_LOGE(TAG, "Media source error");
        return media_player::MEDIA_PLAYER_STATE_IDLE;
      case media_source::MediaSourceState::IDLE:
      default:
        return media_player::MEDIA_PLAYER_STATE_IDLE;
    }
  }

  // No active source. Stay PLAYING during playlist transitions
  if (playlist_active && old_state == media_player::MEDIA_PLAYER_STATE_PLAYING) {
    return media_player::MEDIA_PLAYER_STATE_PLAYING;
  }
  return media_player::MEDIA_PLAYER_STATE_IDLE;
}

void SpeakerSourceMediaPlayer::loop() {
  // Process queued control commands
  this->process_control_queue_();

  // Update state based on active sources - announcement pipeline takes priority
  media_player::MediaPlayerState old_state = this->state;

  PipelineContext &ann_ps = this->pipelines_[ANNOUNCEMENT_PIPELINE];
  PipelineContext &media_ps = this->pipelines_[MEDIA_PIPELINE];

  // Check playlist state to detect transitions between items
  bool announcement_playlist_active = (ann_ps.playlist_index < ann_ps.playlist.size()) ||
                                      (ann_ps.repeat_mode != REPEAT_OFF && !ann_ps.playlist.empty());
  bool media_playlist_active = (media_ps.playlist_index < media_ps.playlist.size()) ||
                               (media_ps.repeat_mode != REPEAT_OFF && !media_ps.playlist.empty());

  // Check announcement pipeline first
  media_source::MediaSource *announcement_source = ann_ps.active_source;
  if (announcement_source != nullptr) {
    media_source::MediaSourceState announcement_state = announcement_source->get_state();
    if (announcement_state != media_source::MediaSourceState::IDLE) {
      // Announcement is active - announcements take priority and never report PAUSED
      switch (announcement_state) {
        case media_source::MediaSourceState::PLAYING:
        case media_source::MediaSourceState::PAUSED:  // Treat paused announcements as announcing
          this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
          break;
        case media_source::MediaSourceState::ERROR:
          ESP_LOGE(TAG, "Announcement source error");
          // Fall through to media pipeline state
          this->state = this->get_source_state_(media_ps.active_source, media_playlist_active, old_state);
          break;
        default:
          break;
      }
    } else {
      // Announcement source is idle, fall through to media pipeline
      this->state = this->get_source_state_(media_ps.active_source, media_playlist_active, old_state);
    }
  } else if (announcement_playlist_active && old_state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING) {
    this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
  } else {
    // No active announcement, check media pipeline
    this->state = this->get_source_state_(media_ps.active_source, media_playlist_active, old_state);
  }

  if (this->state != old_state) {
    this->publish_state();
    ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
  }
}

media_source::MediaSource *SpeakerSourceMediaPlayer::find_source_for_uri_(const std::string &uri, uint8_t pipeline) {
  PipelineContext &ps = this->pipelines_[pipeline];
  media_source::MediaSource *first_match = nullptr;
  for (auto &binding : ps.sources) {
    if (binding->source->can_handle(uri)) {
      // Prefer an idle source; otherwise remember the first match (will be stopped by try_execute_play_uri_)
      if (binding->source->get_state() == media_source::MediaSourceState::IDLE) {
        return binding->source;
      }
      if (first_match == nullptr) {
        first_match = binding->source;
      }
    }
  }
  return first_match;
}

bool SpeakerSourceMediaPlayer::try_execute_play_uri_(const std::string &uri, uint8_t pipeline) {
  // Find target source
  media_source::MediaSource *target_source = this->find_source_for_uri_(uri, pipeline);
  if (target_source == nullptr) {
    ESP_LOGW(TAG, "No source for URI");
    ESP_LOGV(TAG, "URI: %s", uri.c_str());
    return true;  // Remove from queue (unrecoverable)
  }

  PipelineContext &ps = this->pipelines_[pipeline];

  media_source::MediaSource *active_source = ps.active_source;

  // If active source exists and is not IDLE, stop it and wait
  if (active_source != nullptr) {
    media_source::MediaSourceState active_state = active_source->get_state();
    if (active_state != media_source::MediaSourceState::IDLE) {
      // Only send END command once per source - check if we've already asked this source to stop
      if (ps.stopping_source != active_source) {
        ESP_LOGV(TAG, "Pipeline %u: stopping active source", pipeline);
        ps.stopping_source = active_source;
        active_source->handle_command(media_source::MediaSourceCommand::STOP);
        ps.speaker->stop();
      }
      return false;  // Leave in queue, retry next loop
    }
  }

  // Also check target source directly - handles case where source errored before PLAYING state
  media_source::MediaSourceState target_state = target_source->get_state();
  if (target_state != media_source::MediaSourceState::IDLE) {
    // Only send STOP command once per source
    if (ps.stopping_source != target_source) {
      ESP_LOGV(TAG, "Pipeline %u: target source busy, stopping", pipeline);
      ps.stopping_source = target_source;
      target_source->handle_command(media_source::MediaSourceCommand::STOP);
      ps.speaker->stop();
    }
    return false;  // Leave in queue, retry next loop
  }

  // Clear stopping flag since we're past the stopping phase
  ps.stopping_source = nullptr;

  // Check if speaker is ready
  if (!ps.speaker->is_stopped()) {
    return false;  // Speaker not ready yet, retry later
  }

  // Set pending source so handle_media_state_changed_ can recognize it when the source transitions to PLAYING
  ps.pending_source = target_source;

  // Speaker is ready, try to play
  if (!target_source->play_uri(uri)) {
    ESP_LOGE(TAG, "Pipeline %u: Failed to play URI: %s", pipeline, uri.c_str());
    ps.pending_source = nullptr;
    this->queue_command_(MediaPlayerControlCommand::PLAYLIST_ADVANCE, pipeline);
  }

  // Reset pending frame counter for this pipeline since we're starting a new source
  ps.pending_frames.store(0, std::memory_order_relaxed);

  return true;  // Remove from queue
}

// THREAD CONTEXT: Called from main loop (process_control_queue_, queue_play_current_, handle_media_state_changed_)
void SpeakerSourceMediaPlayer::queue_command_(MediaPlayerControlCommand::Type type, uint8_t pipeline) {
  MediaPlayerControlCommand cmd{};
  cmd.type = type;
  cmd.pipeline = pipeline;
  if (xQueueSend(this->media_control_command_queue_, &cmd, 0) != pdTRUE) {
    ESP_LOGE(TAG, "Queue full, command dropped");
  }
}

// THREAD CONTEXT: Called from main loop via automation commands (direct)
void SpeakerSourceMediaPlayer::set_playlist_delay_ms(uint8_t pipeline, uint32_t delay_ms) {
  if (pipeline < this->pipelines_.size()) {
    this->pipelines_[pipeline].playlist_delay_ms = delay_ms;
  }
}

// THREAD CONTEXT: Called from main loop (process_control_queue_).
// The timeout callback also runs on the main loop.
void SpeakerSourceMediaPlayer::queue_play_current_(uint8_t pipeline, uint32_t delay_ms) {
  if (delay_ms > 0) {
    this->set_timeout(PIPELINE_TIMEOUT_IDS[pipeline], delay_ms,
                      [this, pipeline]() { this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline); });
  } else {
    this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
  }
}

// THREAD CONTEXT: Called from main loop (loop)
void SpeakerSourceMediaPlayer::process_control_queue_() {
  MediaPlayerControlCommand control_command{};

  // Use peek to check command without removing it
  if (xQueuePeek(this->media_control_command_queue_, &control_command, 0) != pdTRUE) {
    return;
  }

  bool command_executed = false;
  uint8_t pipeline = control_command.pipeline;

  // Get pipeline state
  PipelineContext &ps = this->pipelines_[pipeline];
  media_source::MediaSource *active_source = ps.active_source;

  switch (control_command.type) {
    case MediaPlayerControlCommand::PLAY_URI: {
      // Always use our local playlist to start playback
      this->cancel_timeout(PIPELINE_TIMEOUT_IDS[pipeline]);
      ps.playlist.clear();
      ps.shuffle_indices.clear();  // Clear shuffle when starting fresh playlist
      ps.playlist_index = 0;       // Reset index
      ps.playlist.push_back(*control_command.data.uri);

      // Queue PLAY_CURRENT to initiate playback
      this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
      command_executed = true;
      break;
    }

    case MediaPlayerControlCommand::ENQUEUE_URI: {
      // Always add to our local playlist
      ps.playlist.push_back(*control_command.data.uri);

      // If shuffle is active, add the new item to the end of the shuffle order
      if (!ps.shuffle_indices.empty()) {
        ps.shuffle_indices.push_back(ps.playlist.size() - 1);
      }

      // If nothing is playing and no upcoming items are queued, start the new item.
      bool nothing_playing =
          (active_source == nullptr) || (active_source->get_state() == media_source::MediaSourceState::IDLE);
      if (nothing_playing && ps.playlist_index >= ps.playlist.size() - 1) {
        ps.playlist_index = ps.playlist.size() - 1;  // Point to newly added item
        this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
      }
      command_executed = true;
      break;
    }

    case MediaPlayerControlCommand::PLAYLIST_ADVANCE: {
      // Internal message: a track finished, advance to next
      if (ps.repeat_mode != REPEAT_ONE) {
        ps.playlist_index++;
      }

      // Check if we should continue playback
      if (ps.playlist_index < ps.playlist.size()) {
        this->queue_play_current_(pipeline, ps.playlist_delay_ms);
      } else if (ps.repeat_mode == REPEAT_ALL && !ps.playlist.empty()) {
        if (!ps.shuffle_indices.empty()) {
          this->shuffle_playlist_(pipeline);
        }
        ps.playlist_index = 0;
        this->queue_play_current_(pipeline, ps.playlist_delay_ms);
      }
      command_executed = true;
      break;
    }

    case MediaPlayerControlCommand::PLAY_CURRENT: {
      // Play the item at current playlist index (mapped through shuffle if active)
      if (ps.playlist_index < ps.playlist.size()) {
        size_t actual_position = this->get_playlist_position_(pipeline);
        command_executed = this->try_execute_play_uri_(ps.playlist[actual_position], pipeline);
      } else {
        command_executed = true;  // Index out of bounds or empty playlist
      }
      break;
    }

    case MediaPlayerControlCommand::SEND_COMMAND: {
      this->handle_player_command_(control_command.data.command, pipeline);
      command_executed = true;
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

// THREAD CONTEXT: Called from main loop only (via process_control_queue_)
void SpeakerSourceMediaPlayer::handle_player_command_(media_player::MediaPlayerCommand player_command,
                                                      uint8_t pipeline) {
  PipelineContext &ps = this->pipelines_[pipeline];
  media_source::MediaSource *active_source = ps.active_source;
  bool has_internal_playlist = (active_source != nullptr) && active_source->has_internal_playlist();

  // Determine target source: prefer active, fall back to last
  media_source::MediaSource *target_source = nullptr;
  if (active_source != nullptr) {
    target_source = active_source;
  } else if (ps.last_source != nullptr) {
    target_source = ps.last_source;
  }

  switch (player_command) {
    case media_player::MEDIA_PLAYER_COMMAND_TOGGLE: {
      // Convert TOGGLE to PLAY or PAUSE based on current state
      if ((active_source != nullptr) && (active_source->get_state() == media_source::MediaSourceState::PLAYING)) {
        if (target_source != nullptr) {
          target_source->handle_command(media_source::MediaSourceCommand::PAUSE);
        }
      } else if (!has_internal_playlist && active_source == nullptr && !ps.playlist.empty()) {
        bool last_has_internal_playlist = (ps.last_source != nullptr) && ps.last_source->has_internal_playlist();
        if (last_has_internal_playlist) {
          ps.last_source->handle_command(media_source::MediaSourceCommand::PLAY);
        } else {
          if (ps.playlist_index >= ps.playlist.size()) {
            ps.playlist_index = 0;
          }
          this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
        }
      } else {
        if (target_source != nullptr) {
          target_source->handle_command(media_source::MediaSourceCommand::PLAY);
        }
      }
      break;
    }

    case media_player::MEDIA_PLAYER_COMMAND_PLAY: {
      if (!has_internal_playlist && active_source == nullptr && !ps.playlist.empty()) {
        bool last_has_internal_playlist = (ps.last_source != nullptr) && ps.last_source->has_internal_playlist();
        if (last_has_internal_playlist) {
          ps.last_source->handle_command(media_source::MediaSourceCommand::PLAY);
        } else {
          if (ps.playlist_index >= ps.playlist.size()) {
            ps.playlist_index = 0;
          }
          this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
        }
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::PLAY);
      }
      break;
    }

    case media_player::MEDIA_PLAYER_COMMAND_PAUSE: {
      if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::PAUSE);
      }
      break;
    }

    case media_player::MEDIA_PLAYER_COMMAND_STOP: {
      if (!has_internal_playlist) {
        this->cancel_timeout(PIPELINE_TIMEOUT_IDS[pipeline]);
        ps.playlist.clear();
        ps.shuffle_indices.clear();
        ps.playlist_index = 0;
      }
      if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::STOP);
      }
      break;
    }

    case media_player::MEDIA_PLAYER_COMMAND_NEXT: {
      if (!has_internal_playlist) {
        this->cancel_timeout(PIPELINE_TIMEOUT_IDS[pipeline]);
        if (ps.playlist_index + 1 < ps.playlist.size()) {
          ps.playlist_index++;
          this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
        } else if (ps.repeat_mode == REPEAT_ALL && !ps.playlist.empty()) {
          ps.playlist_index = 0;
          this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
        }
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::NEXT);
      }
      break;
    }

    case media_player::MEDIA_PLAYER_COMMAND_PREVIOUS: {
      if (!has_internal_playlist) {
        this->cancel_timeout(PIPELINE_TIMEOUT_IDS[pipeline]);
        if (ps.playlist_index > 0) {
          ps.playlist_index--;
          this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
        } else if (ps.repeat_mode == REPEAT_ALL && !ps.playlist.empty()) {
          ps.playlist_index = ps.playlist.size() - 1;
          this->queue_command_(MediaPlayerControlCommand::PLAY_CURRENT, pipeline);
        }
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::PREVIOUS);
      }
      break;
    }

    case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
      if (!has_internal_playlist) {
        ps.repeat_mode = REPEAT_ONE;
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::REPEAT_ONE);
      }
      break;

    case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
      if (!has_internal_playlist) {
        ps.repeat_mode = REPEAT_OFF;
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::REPEAT_OFF);
      }
      break;

    case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ALL:
      if (!has_internal_playlist) {
        ps.repeat_mode = REPEAT_ALL;
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::REPEAT_ALL);
      }
      break;

    case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST: {
      if (!has_internal_playlist) {
        this->cancel_timeout(PIPELINE_TIMEOUT_IDS[pipeline]);
        if (ps.playlist_index < ps.playlist.size()) {
          size_t actual_position = this->get_playlist_position_(pipeline);
          ps.playlist[0] = std::move(ps.playlist[actual_position]);
          ps.playlist.resize(1);
          ps.playlist_index = 0;
        } else {
          ps.playlist.clear();
          ps.playlist_index = 0;
        }
        ps.shuffle_indices.clear();
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::CLEAR_PLAYLIST);
      }
      break;
    }

    case media_player::MEDIA_PLAYER_COMMAND_SHUFFLE:
      if (!has_internal_playlist) {
        this->shuffle_playlist_(pipeline);
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::SHUFFLE);
      }
      break;

    case media_player::MEDIA_PLAYER_COMMAND_UNSHUFFLE:
      if (!has_internal_playlist) {
        this->unshuffle_playlist_(pipeline);
      } else if (target_source != nullptr) {
        target_source->handle_command(media_source::MediaSourceCommand::UNSHUFFLE);
      }
      break;

    default:
      // TURN_ON, TURN_OFF, ENQUEUE (handled separately with URL) are no-ops
      break;
  }
}

// THREAD CONTEXT: Called from main loop only. Entry points:
// - HA/automation commands (direct)
// - handle_play_uri_request_() via make_call().perform() (deferred from source tasks)
void SpeakerSourceMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (!this->is_ready()) {
    return;
  }

  MediaPlayerControlCommand control_command{};

  // Determine which pipeline to use based on announcement flag, falling back if the preferred pipeline
  // is not configured
  auto announcement = call.get_announcement();
  if (announcement.has_value() && announcement.value()) {
    if (this->pipelines_[ANNOUNCEMENT_PIPELINE].is_configured()) {
      control_command.pipeline = ANNOUNCEMENT_PIPELINE;
    } else {
      control_command.pipeline = MEDIA_PIPELINE;
    }
  } else {
    if (this->pipelines_[MEDIA_PIPELINE].is_configured()) {
      control_command.pipeline = MEDIA_PIPELINE;
    } else {
      control_command.pipeline = ANNOUNCEMENT_PIPELINE;
    }
  }

  const auto &media_url = call.get_media_url();
  if (media_url.has_value()) {
    auto command = call.get_command();
    bool enqueue = command.has_value() && command.value() == media_player::MEDIA_PLAYER_COMMAND_ENQUEUE;

    if (enqueue) {
      control_command.type = MediaPlayerControlCommand::ENQUEUE_URI;
    } else {
      control_command.type = MediaPlayerControlCommand::PLAY_URI;
    }
    // Heap allocation is unavoidable: URIs from Home Assistant are arbitrary-length (media URLs with tokens
    // can easily exceed 500 bytes). Deleted in process_control_queue_() after the command is consumed. FreeRTOS queues
    // require items to be copyable, so we store a pointer to the string in the queue rather than the string itself.
    control_command.data.uri = new std::string(media_url.value());
    if (xQueueSend(this->media_control_command_queue_, &control_command, 0) != pdTRUE) {
      delete control_command.data.uri;
      ESP_LOGE(TAG, "Queue full, URI dropped");
    }
    return;
  }

  auto volume = call.get_volume();
  if (volume.has_value()) {
    this->set_volume_(volume.value());
    this->publish_state();
    return;
  }

  auto cmd = call.get_command();
  if (cmd.has_value()) {
    switch (cmd.value()) {
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->set_mute_state_(true);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->set_mute_state_(false);
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP:
        this->set_volume_(std::min(1.0f, this->volume + this->volume_increment_));
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
        this->set_volume_(std::max(0.0f, this->volume - this->volume_increment_));
        break;
      default:
        // Queue command for processing in loop()
        control_command.type = MediaPlayerControlCommand::SEND_COMMAND;
        control_command.data.command = cmd.value();
        if (xQueueSend(this->media_control_command_queue_, &control_command, 0) != pdTRUE) {
          ESP_LOGE(TAG, "Queue full, command dropped");
        }
        return;
    }
    this->publish_state();
  }
}

media_player::MediaPlayerTraits SpeakerSourceMediaPlayer::get_traits() {
  // This media player supports more traits like playlists, repeat, and shuffle, but the ESPHome API currently (March
  // 2026) doesn't support those commands, so we only report pause support for now since that's used by the frontend and
  // supported by our player.
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(true);

  for (const auto &ps : this->pipelines_) {
    if (ps.format.has_value()) {
      traits.get_supported_formats().push_back(ps.format.value());
    }
  }

  return traits;
}

void SpeakerSourceMediaPlayer::save_volume_restore_state_() {
  VolumeRestoreState volume_restore_state;
  volume_restore_state.volume = this->volume;
  volume_restore_state.is_muted = this->is_muted_;
  this->pref_.save(&volume_restore_state);
}

void SpeakerSourceMediaPlayer::set_mute_state_(bool mute_state, bool publish) {
  if (this->is_muted_ == mute_state) {
    return;
  }

  for (auto &ps : this->pipelines_) {
    if (ps.is_configured()) {
      ps.speaker->set_mute_state(mute_state);
    }
  }

  this->is_muted_ = mute_state;

  if (publish) {
    this->save_volume_restore_state_();
  }

  // Notify all media sources about the mute state change
  for (auto &ps : this->pipelines_) {
    for (auto &binding : ps.sources) {
      binding->source->notify_mute_changed(mute_state);
    }
  }

  if (mute_state) {
    this->defer([this]() { this->mute_trigger_.trigger(); });
  } else {
    this->defer([this]() { this->unmute_trigger_.trigger(); });
  }
}

void SpeakerSourceMediaPlayer::set_volume_(float volume, bool publish) {
  // Remap the volume to fit within the configured limits
  float bounded_volume = remap<float, float>(volume, 0.0f, 1.0f, this->volume_min_, this->volume_max_);

  for (auto &ps : this->pipelines_) {
    if (ps.is_configured()) {
      ps.speaker->set_volume(bounded_volume);
    }
  }

  if (publish) {
    this->volume = volume;
  }

  // Notify all media sources about the volume change
  for (auto &ps : this->pipelines_) {
    for (auto &binding : ps.sources) {
      binding->source->notify_volume_changed(volume);
    }
  }

  // Turn on the mute state if the volume is effectively zero, off otherwise.
  // Pass publish=false to avoid saving twice.
  if (volume < 0.001) {
    this->set_mute_state_(true, false);
  } else {
    this->set_mute_state_(false, false);
  }

  // Save after mute mutation so the restored state has the correct is_muted_ value
  if (publish) {
    this->save_volume_restore_state_();
  }

  this->defer([this, volume]() { this->volume_trigger_.trigger(volume); });
}

size_t SpeakerSourceMediaPlayer::get_playlist_position_(uint8_t pipeline) const {
  const PipelineContext &ps = this->pipelines_[pipeline];

  if (ps.shuffle_indices.empty() || ps.playlist_index >= ps.shuffle_indices.size()) {
    return ps.playlist_index;
  }
  return ps.shuffle_indices[ps.playlist_index];
}

void SpeakerSourceMediaPlayer::shuffle_playlist_(uint8_t pipeline) {
  PipelineContext &ps = this->pipelines_[pipeline];

  if (ps.playlist.size() <= 1) {
    ps.shuffle_indices.clear();
    return;
  }

  // Capture current actual position BEFORE modifying shuffle_indices
  size_t current_actual = this->get_playlist_position_(pipeline);

  // Build indices vector
  ps.shuffle_indices.resize(ps.playlist.size());
  for (size_t i = 0; i < ps.playlist.size(); i++) {
    ps.shuffle_indices[i] = i;
  }

  // Fisher-Yates shuffle using ESPHome's random helper
  for (size_t i = ps.shuffle_indices.size() - 1; i > 0; i--) {
    size_t j = random_uint32() % (i + 1);
    std::swap(ps.shuffle_indices[i], ps.shuffle_indices[j]);
  }

  // Move current track to current position (so playback continues seamlessly)
  if (ps.playlist_index < ps.shuffle_indices.size()) {
    for (size_t i = 0; i < ps.shuffle_indices.size(); i++) {
      if (ps.shuffle_indices[i] == current_actual) {
        std::swap(ps.shuffle_indices[i], ps.shuffle_indices[ps.playlist_index]);
        break;
      }
    }
  }
}

void SpeakerSourceMediaPlayer::unshuffle_playlist_(uint8_t pipeline) {
  PipelineContext &ps = this->pipelines_[pipeline];

  if (!ps.shuffle_indices.empty() && ps.playlist_index < ps.shuffle_indices.size()) {
    ps.playlist_index = ps.shuffle_indices[ps.playlist_index];
  }
  ps.shuffle_indices.clear();
}

}  // namespace esphome::speaker_source

#endif  // USE_ESP32
