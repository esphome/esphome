#include "speaker_media_player.h"

#ifdef USE_ESP_IDF

#include "esphome/core/log.h"

#include "esphome/components/audio/audio.h"
#ifdef USE_OTA
#include "esphome/components/ota/ota_backend.h"
#endif

namespace esphome {
namespace speaker {

// Framework:
//  - Media player that can handle two streams: one for media and one for announcements
//    - Each stream has an individual speaker component for output
//  - Each stream is handled by an ``AudioPipeline`` object with two parts/tasks
//    - ``AudioReader`` handles reading from an HTTP source or from a PROGMEM flash set at compile time
//    - ``AudioDecoder`` handles decoding the audio file. All formats are limited to two channels and 16 bits per sample
//      - FLAC
//      - MP3 (based on the libhelix decoder)
//      - WAV
//    - Each task runs until it is done processing the file or it receives a stop command
//    - Inter-task communication uses a FreeRTOS Event Group
//    - The ``AudioPipeline`` sets up a ring buffer between the reader and decoder tasks. The decoder task outputs audio
//      directly to a speaker component.
//    - The pipelines internal state needs to be processed by regularly calling ``process_state``.
//  - Generic media player commands are received by the ``control`` function. The commands are added to the
//    ``media_control_command_queue_`` to be processed in the component's loop
//    - Local file play back is initiatied with ``play_file`` and adds it to the ``media_control_command_queue_``
//    - Starting a stream intializes the appropriate pipeline or stops it if it is already running
//    - Volume and mute commands are achieved by the ``mute``, ``unmute``, ``set_volume`` functions.
//      - Volume commands are ignored if the media control queue is full to avoid crashing with rapid volume
//        increases/decreases.
//      - These functions all send the appropriate information to the speakers to implement.
//    - Pausing is implemented in the decoder task and is also sent directly to the media speaker component to decrease
//      latency.
//  - The components main loop performs housekeeping:
//    - It reads the media control queue and processes it directly
//    - It determines the overall state of the media player by considering the state of each pipeline
//      - announcement playback takes highest priority
//    - Handles playlists and repeating by starting the appropriate file when a previous file is finished
//  - Logging only happens in the main loop task to reduce task stack memory usage.

static const uint32_t MEDIA_CONTROLS_QUEUE_LENGTH = 20;

static const UBaseType_t MEDIA_PIPELINE_TASK_PRIORITY = 1;
static const UBaseType_t ANNOUNCEMENT_PIPELINE_TASK_PRIORITY = 1;

static const float FIRST_BOOT_DEFAULT_VOLUME = 0.5f;

static const char *const TAG = "speaker_media_player";

void SpeakerMediaPlayer::setup() {
  state = media_player::MEDIA_PLAYER_STATE_IDLE;

  this->media_control_command_queue_ = xQueueCreate(MEDIA_CONTROLS_QUEUE_LENGTH, sizeof(MediaCallCommand));

  this->pref_ = global_preferences->make_preference<VolumeRestoreState>(this->get_object_id_hash());

  VolumeRestoreState volume_restore_state;
  if (this->pref_.load(&volume_restore_state)) {
    this->set_volume_(volume_restore_state.volume);
    this->set_mute_state_(volume_restore_state.is_muted);
  } else {
    this->set_volume_(FIRST_BOOT_DEFAULT_VOLUME);
    this->set_mute_state_(false);
  }

#ifdef USE_OTA
  ota::get_global_ota_callback()->add_on_state_callback(
      [this](ota::OTAState state, float progress, uint8_t error, ota::OTAComponent *comp) {
        if (state == ota::OTA_STARTED) {
          if (this->media_pipeline_ != nullptr) {
            this->media_pipeline_->suspend_tasks();
          }
          if (this->announcement_pipeline_ != nullptr) {
            this->announcement_pipeline_->suspend_tasks();
          }
        } else if (state == ota::OTA_ERROR) {
          if (this->media_pipeline_ != nullptr) {
            this->media_pipeline_->resume_tasks();
          }
          if (this->announcement_pipeline_ != nullptr) {
            this->announcement_pipeline_->resume_tasks();
          }
        }
      });
#endif

  this->announcement_pipeline_ =
      make_unique<AudioPipeline>(this->announcement_speaker_, this->buffer_size_, this->task_stack_in_psram_, "ann",
                                 ANNOUNCEMENT_PIPELINE_TASK_PRIORITY);

  if (this->announcement_pipeline_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create announcement pipeline");
    this->mark_failed();
  }

  if (!this->single_pipeline_()) {
    this->media_pipeline_ = make_unique<AudioPipeline>(this->media_speaker_, this->buffer_size_,
                                                       this->task_stack_in_psram_, "ann", MEDIA_PIPELINE_TASK_PRIORITY);

    if (this->media_pipeline_ == nullptr) {
      ESP_LOGE(TAG, "Failed to create media pipeline");
      this->mark_failed();
    }

    // Setup callback to track the duration of audio played by the media pipeline
    this->media_speaker_->add_audio_output_callback(
        [this](uint32_t new_playback_ms, uint32_t remainder_us, uint32_t pending_ms, uint32_t write_timestamp) {
          this->playback_ms_ += new_playback_ms;
          this->remainder_us_ = remainder_us;
          this->pending_ms_ = pending_ms;
          this->last_audio_write_timestamp_ = write_timestamp;
          this->playback_us_ = this->playback_ms_ * 1000 + this->remainder_us_;
        });
  }

  ESP_LOGI(TAG, "Set up speaker media player");
}

void SpeakerMediaPlayer::set_playlist_delay_ms(AudioPipelineType pipeline_type, uint32_t delay_ms) {
  switch (pipeline_type) {
    case AudioPipelineType::ANNOUNCEMENT:
      this->announcement_playlist_delay_ms_ = delay_ms;
      break;
    case AudioPipelineType::MEDIA:
      this->media_playlist_delay_ms_ = delay_ms;
      break;
  }
}

void SpeakerMediaPlayer::watch_media_commands_() {
  if (!this->is_ready()) {
    return;
  }

  MediaCallCommand media_command;
  esp_err_t err = ESP_OK;

  if (xQueueReceive(this->media_control_command_queue_, &media_command, 0) == pdTRUE) {
    bool new_url = media_command.new_url.has_value() && media_command.new_url.value();
    bool new_file = media_command.new_file.has_value() && media_command.new_file.value();

    if (new_url || new_file) {
      bool enqueue = media_command.enqueue.has_value() && media_command.enqueue.value();

      if (this->single_pipeline_() || (media_command.announce.has_value() && media_command.announce.value())) {
        // Announcement playlist/pipeline

        if (!enqueue) {
          // Clear the queue and ensure the loaded next item doesn't start playing
          this->cancel_timeout("next_ann");
          this->announcement_playlist_.clear();
        }

        PlaylistItem playlist_item;
        if (new_url) {
          playlist_item.url = this->announcement_url_;
          if (!enqueue) {
            // Not adding to the queue, so directly start playback and internally unpause the pipeline
            this->announcement_pipeline_->start_url(playlist_item.url.value());
            this->announcement_pipeline_->set_pause_state(false);
          }
        } else {
          playlist_item.file = this->announcement_file_;
          if (!enqueue) {
            // Not adding to the queue, so directly start playback and internally unpause the pipeline
            this->announcement_pipeline_->start_file(playlist_item.file.value());
            this->announcement_pipeline_->set_pause_state(false);
          }
        }
        this->announcement_playlist_.push_back(playlist_item);
      } else {
        // Media playlist/pipeline

        if (!enqueue) {
          // Clear the queue and ensure the loaded next item doesn't start playing
          this->cancel_timeout("next_media");
          this->media_playlist_.clear();
        }

        this->is_paused_ = false;
        PlaylistItem playlist_item;
        if (new_url) {
          playlist_item.url = this->media_url_;
          if (!enqueue) {
            // Not adding to the queue, so directly start playback and internally unpause the pipeline
            this->media_pipeline_->start_url(playlist_item.url.value());
            this->media_pipeline_->set_pause_state(false);
          }
        } else {
          playlist_item.file = this->media_file_;
          if (!enqueue) {
            // Not adding to the queue, so directly start playback and internally unpause the pipeline
            this->media_pipeline_->start_file(playlist_item.file.value());
            this->media_pipeline_->set_pause_state(false);
          }
        }
        this->media_playlist_.push_back(playlist_item);
      }

      if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error starting the audio pipeline: %s", esp_err_to_name(err));
        this->status_set_error();
      } else {
        this->status_clear_error();
      }

      return;  // Don't process the new file play command further
    }

    if (media_command.volume.has_value()) {
      this->set_volume_(media_command.volume.value());
      this->publish_state();
    }

    if (media_command.command.has_value()) {
      switch (media_command.command.value()) {
        case media_player::MEDIA_PLAYER_COMMAND_PLAY:
          if ((this->media_pipeline_ != nullptr) && (this->is_paused_)) {
            this->media_pipeline_->set_pause_state(false);
          }
          this->is_paused_ = false;
          break;
        case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
          if ((this->media_pipeline_ != nullptr) && (!this->is_paused_)) {
            this->media_pipeline_->set_pause_state(true);
          }
          this->is_paused_ = true;
          break;
        case media_player::MEDIA_PLAYER_COMMAND_STOP:
          if (this->single_pipeline_() || (media_command.announce.has_value() && media_command.announce.value())) {
            if (this->announcement_pipeline_ != nullptr) {
              this->cancel_timeout("next_ann");
              this->announcement_playlist_.clear();
              this->announcement_pipeline_->stop();
            }
          } else {
            if (this->media_pipeline_ != nullptr) {
              this->cancel_timeout("next_media");
              this->media_playlist_.clear();
              this->media_pipeline_->stop();
            }
          }
          break;
        case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
          if (this->media_pipeline_ != nullptr) {
            if (this->is_paused_) {
              this->media_pipeline_->set_pause_state(false);
              this->is_paused_ = false;
            } else {
              this->media_pipeline_->set_pause_state(true);
              this->is_paused_ = true;
            }
          }
          break;
        case media_player::MEDIA_PLAYER_COMMAND_MUTE: {
          this->set_mute_state_(true);

          this->publish_state();
          break;
        }
        case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
          this->set_mute_state_(false);
          this->publish_state();
          break;
        case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP:
          this->set_volume_(std::min(1.0f, this->volume + this->volume_increment_));
          this->publish_state();
          break;
        case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
          this->set_volume_(std::max(0.0f, this->volume - this->volume_increment_));
          this->publish_state();
          break;
        case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
          if (this->single_pipeline_() || (media_command.announce.has_value() && media_command.announce.value())) {
            this->announcement_repeat_one_ = true;
          } else {
            this->media_repeat_one_ = true;
          }
          break;
        case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
          if (this->single_pipeline_() || (media_command.announce.has_value() && media_command.announce.value())) {
            this->announcement_repeat_one_ = false;
          } else {
            this->media_repeat_one_ = false;
          }
          break;
        case media_player::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST:
          if (this->single_pipeline_() || (media_command.announce.has_value() && media_command.announce.value())) {
            if (this->announcement_playlist_.empty()) {
              this->announcement_playlist_.resize(1);
            }
          } else {
            if (this->media_playlist_.empty()) {
              this->media_playlist_.resize(1);
            }
          }
          break;
        default:
          break;
      }
    }
  }
}

void SpeakerMediaPlayer::loop() {
  this->watch_media_commands_();

  // Determine state of the media player
  media_player::MediaPlayerState old_state = this->state;

  AudioPipelineState old_media_pipeline_state = this->media_pipeline_state_;
  if (this->media_pipeline_ != nullptr) {
    this->media_pipeline_state_ = this->media_pipeline_->process_state();
    this->decoded_playback_ms_ = this->media_pipeline_->get_playback_ms();
  }

  if (this->media_pipeline_state_ == AudioPipelineState::ERROR_READING) {
    ESP_LOGE(TAG, "The media pipeline's file reader encountered an error.");
  } else if (this->media_pipeline_state_ == AudioPipelineState::ERROR_DECODING) {
    ESP_LOGE(TAG, "The media pipeline's audio decoder encountered an error.");
  }

  AudioPipelineState old_announcement_pipeline_state = this->announcement_pipeline_state_;
  if (this->announcement_pipeline_ != nullptr) {
    this->announcement_pipeline_state_ = this->announcement_pipeline_->process_state();
  }

  if (this->announcement_pipeline_state_ == AudioPipelineState::ERROR_READING) {
    ESP_LOGE(TAG, "The announcement pipeline's file reader encountered an error.");
  } else if (this->announcement_pipeline_state_ == AudioPipelineState::ERROR_DECODING) {
    ESP_LOGE(TAG, "The announcement pipeline's audio decoder encountered an error.");
  }

  if (this->announcement_pipeline_state_ != AudioPipelineState::STOPPED) {
    this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
  } else {
    if (!this->announcement_playlist_.empty()) {
      uint32_t timeout_ms = 0;
      if (old_announcement_pipeline_state == AudioPipelineState::PLAYING) {
        // Finished the current announcement file
        if (!this->announcement_repeat_one_) {
          //  Pop item off the playlist if repeat is disabled
          this->announcement_playlist_.pop_front();
        }
        // Only delay starting playback if moving on the next playlist item or repeating the current item
        timeout_ms = this->announcement_playlist_delay_ms_;
      }

      if (!this->announcement_playlist_.empty()) {
        // Start the next announcement file
        PlaylistItem playlist_item = this->announcement_playlist_.front();
        if (playlist_item.url.has_value()) {
          this->announcement_pipeline_->start_url(playlist_item.url.value());
        } else if (playlist_item.file.has_value()) {
          this->announcement_pipeline_->start_file(playlist_item.file.value());
        }

        if (timeout_ms > 0) {
          // Pause pipeline internally to facilitiate delay between items
          this->announcement_pipeline_->set_pause_state(true);
          // Internally unpause the pipeline after the delay between playlist items
          this->set_timeout("next_ann", timeout_ms,
                            [this]() { this->announcement_pipeline_->set_pause_state(this->is_paused_); });
        }
      }
    } else {
      if (this->is_paused_) {
        this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
      } else if (this->media_pipeline_state_ == AudioPipelineState::PLAYING) {
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
      } else if (this->media_pipeline_state_ == AudioPipelineState::STOPPED) {
        // Reset playback durations
        this->decoded_playback_ms_ = 0;
        this->playback_us_ = 0;
        this->playback_ms_ = 0;
        this->remainder_us_ = 0;
        this->pending_ms_ = 0;

        if (!media_playlist_.empty()) {
          uint32_t timeout_ms = 0;
          if (old_media_pipeline_state == AudioPipelineState::PLAYING) {
            // Finished the current media file
            if (!this->media_repeat_one_) {
              // Pop item off the playlist if repeat is disabled
              this->media_playlist_.pop_front();
            }
            // Only delay starting playback if moving on the next playlist item or repeating the current item
            timeout_ms = this->announcement_playlist_delay_ms_;
          }
          if (!this->media_playlist_.empty()) {
            PlaylistItem playlist_item = this->media_playlist_.front();
            if (playlist_item.url.has_value()) {
              this->media_pipeline_->start_url(playlist_item.url.value());
            } else if (playlist_item.file.has_value()) {
              this->media_pipeline_->start_file(playlist_item.file.value());
            }

            if (timeout_ms > 0) {
              // Pause pipeline internally to facilitiate delay between items
              this->media_pipeline_->set_pause_state(true);
              // Internally unpause the pipeline after the delay between playlist items
              this->set_timeout("next_media", timeout_ms,
                                [this]() { this->media_pipeline_->set_pause_state(this->is_paused_); });
            }
          }
        } else {
          this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
        }
      }
    }
  }

  if (this->state != old_state) {
    this->publish_state();
    ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
  }
}

void SpeakerMediaPlayer::play_file(audio::AudioFile *media_file, bool announcement, bool enqueue) {
  if (!this->is_ready()) {
    // Ignore any commands sent before the media player is setup
    return;
  }

  MediaCallCommand media_command;

  media_command.new_file = true;
  if (this->single_pipeline_() || announcement) {
    this->announcement_file_ = media_file;
    media_command.announce = true;
  } else {
    this->media_file_ = media_file;
    media_command.announce = false;
  }
  media_command.enqueue = enqueue;
  xQueueSend(this->media_control_command_queue_, &media_command, portMAX_DELAY);
}

void SpeakerMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (!this->is_ready()) {
    // Ignore any commands sent before the media player is setup
    return;
  }

  MediaCallCommand media_command;

  if (this->single_pipeline_() || (call.get_announcement().has_value() && call.get_announcement().value())) {
    media_command.announce = true;
  } else {
    media_command.announce = false;
  }

  if (call.get_media_url().has_value()) {
    std::string new_uri = call.get_media_url().value();

    media_command.new_url = true;
    if (this->single_pipeline_() || (call.get_announcement().has_value() && call.get_announcement().value())) {
      this->announcement_url_ = new_uri;
    } else {
      this->media_url_ = new_uri;
    }

    if (call.get_command().has_value()) {
      if (call.get_command().value() == media_player::MEDIA_PLAYER_COMMAND_ENQUEUE) {
        media_command.enqueue = true;
      }
    }

    xQueueSend(this->media_control_command_queue_, &media_command, portMAX_DELAY);
    return;
  }

  if (call.get_volume().has_value()) {
    media_command.volume = call.get_volume().value();
    // Wait 0 ticks for queue to be free, volume sets aren't that important!
    xQueueSend(this->media_control_command_queue_, &media_command, 0);
    return;
  }

  if (call.get_command().has_value()) {
    media_command.command = call.get_command().value();
    TickType_t ticks_to_wait = portMAX_DELAY;
    if ((call.get_command().value() == media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP) ||
        (call.get_command().value() == media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN)) {
      ticks_to_wait = 0;  // Wait 0 ticks for queue to be free, volume sets aren't that important!
    }
    xQueueSend(this->media_control_command_queue_, &media_command, ticks_to_wait);
    return;
  }
}

media_player::MediaPlayerTraits SpeakerMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  if (!this->single_pipeline_()) {
    traits.set_supports_pause(true);
  }

  if (this->announcement_format_.has_value()) {
    traits.get_supported_formats().push_back(this->announcement_format_.value());
  }
  if (this->media_format_.has_value()) {
    traits.get_supported_formats().push_back(this->media_format_.value());
  } else if (this->single_pipeline_() && this->announcement_format_.has_value()) {
    // Only one pipeline is defined, so use the announcement format (if configured) for the default purpose
    media_player::MediaPlayerSupportedFormat media_format = this->announcement_format_.value();
    media_format.purpose = media_player::MediaPlayerFormatPurpose::PURPOSE_DEFAULT;
    traits.get_supported_formats().push_back(media_format);
  }

  return traits;
};

void SpeakerMediaPlayer::save_volume_restore_state_() {
  VolumeRestoreState volume_restore_state;
  volume_restore_state.volume = this->volume;
  volume_restore_state.is_muted = this->is_muted_;
  this->pref_.save(&volume_restore_state);
}

void SpeakerMediaPlayer::set_mute_state_(bool mute_state) {
  if (this->media_speaker_ != nullptr) {
    this->media_speaker_->set_mute_state(mute_state);
  }
  if (this->announcement_speaker_ != nullptr) {
    this->announcement_speaker_->set_mute_state(mute_state);
  }

  bool old_mute_state = this->is_muted_;
  this->is_muted_ = mute_state;

  this->save_volume_restore_state_();

  if (old_mute_state != mute_state) {
    if (mute_state) {
      this->defer([this]() { this->mute_trigger_->trigger(); });
    } else {
      this->defer([this]() { this->unmute_trigger_->trigger(); });
    }
  }
}

void SpeakerMediaPlayer::set_volume_(float volume, bool publish) {
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

  // Turn on the mute state if the volume is effectively zero, off otherwise
  if (volume < 0.001) {
    this->set_mute_state_(true);
  } else {
    this->set_mute_state_(false);
  }

  this->defer([this, volume]() { this->volume_trigger_->trigger(volume); });
}

}  // namespace speaker
}  // namespace esphome

#endif
