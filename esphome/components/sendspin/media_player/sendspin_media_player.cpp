#include "sendspin_media_player.h"

#if defined(USE_ESP32) && defined(USE_MEDIA_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <sendspin/types.h>
#ifdef USE_SENDSPIN_METADATA
#include <sendspin/metadata_role.h>
#endif

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include <esp_timer.h>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.media_player";

// THREAD CONTEXT: Main loop. The callbacks registered here also fire on the main loop,
// since SendspinHub dispatches group updates, controller state, and metadata from client_->loop().
void SendspinMediaPlayer::setup() {
  // Register for group updates to sync playback state
  this->parent_->add_group_update_callback([this](const sendspin::GroupUpdateObject &group_obj) {
    if (group_obj.playback_state.has_value()) {
      auto new_group_state = group_obj.playback_state.value();
#ifdef USE_SENDSPIN_METADATA
      // A fresh metadata update confirming the new track's speed can lag several seconds behind
      // this group transition. Starting/resuming playback makes any cached pause signal stale -
      // assume "playing" until fresh metadata says otherwise, rather than showing a stale PAUSED.
      if (new_group_state == sendspin::SendspinPlaybackState::PLAYING &&
          this->group_playback_state_ != sendspin::SendspinPlaybackState::PLAYING) {
        this->playback_speed_ = 1;
      }
#endif
      this->group_playback_state_ = new_group_state;
      this->update_state_();
    }
  });

  this->parent_->add_controller_state_callback([this](const sendspin::ServerStateControllerObject &state) {
    float new_volume = static_cast<float>(state.volume) / 100.0f;
    bool new_muted = state.muted;
    if ((new_volume != this->volume) || (new_muted != this->muted_)) {
      this->volume = new_volume;
      this->muted_ = new_muted;
      this->publish_state();
    }
  });

#ifdef USE_SENDSPIN_METADATA
  // Only the metadata role's playback progress carries playback_speed and duration, which is how
  // a paused track becomes observable; the group update alone reports STOPPED for both a paused
  // and a genuinely empty/idle group, so it can't tell the two apart on its own.
  this->parent_->add_metadata_update_callback([this](const sendspin::ServerMetadataStateObject &metadata) {
    if (!metadata.progress.has_value())
      return;
    uint32_t new_speed = metadata.progress.value().playback_speed;
    uint32_t new_duration = metadata.progress.value().track_duration;
    if (new_speed != this->playback_speed_ || new_duration != this->track_duration_ms_) {
      this->playback_speed_ = new_speed;
      this->track_duration_ms_ = new_duration;
      this->update_state_();
    }
  });
#endif

  // Publish an initial state
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->publish_state();
}

void SendspinMediaPlayer::update_state_() {
  media_player::MediaPlayerState new_state = media_player::MEDIA_PLAYER_STATE_IDLE;
  if (this->group_playback_state_ == sendspin::SendspinPlaybackState::PLAYING) {
    new_state = media_player::MEDIA_PLAYER_STATE_PLAYING;
  }
#ifdef USE_SENDSPIN_METADATA
  // A paused track keeps a known nonzero duration while its progress is frozen (speed 0); this
  // overrides the base state above regardless of what the group itself reported, since the group
  // reports STOPPED both when paused and when genuinely idle/empty. Known limitation: some servers
  // (e.g. Music Assistant, at least for group/queue playback - see music-assistant/support#5813)
  // report this same signal for an explicit stop, not just pause, since they don't reset or clear
  // metadata on stop; such a stop is indistinguishable from pause here and is reported as PAUSED.
  if (this->playback_speed_ == 0 && this->track_duration_ms_ > 0) {
    new_state = media_player::MEDIA_PLAYER_STATE_PAUSED;
  }
#endif
  if (this->state != new_state) {
    this->state = new_state;
    this->publish_state();
    ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
  }
}

// THREAD CONTEXT: Main loop (invoked by the media_player framework)
media_player::MediaPlayerTraits SendspinMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();

  // By default, the base media player always enables these traits, but they are not actually supported by this media
  // player
  traits.clear_feature_flags(media_player::MediaPlayerEntityFeature::PLAY_MEDIA |
                             media_player::MediaPlayerEntityFeature::BROWSE_MEDIA |
                             media_player::MediaPlayerEntityFeature::MEDIA_ANNOUNCE);

  traits.add_feature_flags(
      media_player::MediaPlayerEntityFeature::PLAY | media_player::MediaPlayerEntityFeature::PAUSE |
      media_player::MediaPlayerEntityFeature::STOP | media_player::MediaPlayerEntityFeature::VOLUME_STEP |
      media_player::MediaPlayerEntityFeature::VOLUME_SET | media_player::MediaPlayerEntityFeature::VOLUME_MUTE);

  // NEXT_TRACK, PREVIOUS_TRACK, SHUFFLE_SET, and REPEAT_SET are intentionally not advertised: the ESPHome native API
  // does not implement the corresponding media player commands, so Home Assistant cannot actually send them even if
  // we expose the capability. They remain accessible via ESPHome YAML automations.

  return traits;
}

// THREAD CONTEXT: Main loop (invoked by the media_player framework)
void SendspinMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (!this->is_ready()) {
    // Ignore any commands sent before the media player is setup
    return;
  }

  auto volume = call.get_volume();
  if (volume.has_value()) {
    uint8_t new_volume = static_cast<uint8_t>(std::roundf(volume.value() * 100.0f));
    this->parent_->send_client_command(sendspin::SendspinControllerCommand::VOLUME, new_volume, std::nullopt);
  }

  auto command = call.get_command();
  if (!command.has_value()) {
    return;
  }
  switch (command.value()) {
    case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
      if (this->state == media_player::MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING) {
        this->parent_->send_client_command(sendspin::SendspinControllerCommand::PAUSE);
      } else {
        this->parent_->send_client_command(sendspin::SendspinControllerCommand::PLAY);
      }
      break;
    case media_player::MEDIA_PLAYER_COMMAND_PLAY:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::PLAY);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::PAUSE);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_STOP:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::STOP);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::REPEAT_OFF);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::REPEAT_ONE);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_REPEAT_ALL:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::REPEAT_ALL);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_SHUFFLE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::SHUFFLE);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_UNSHUFFLE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::UNSHUFFLE);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_NEXT:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::NEXT);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_PREVIOUS:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::PREVIOUS);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP:
      this->parent_->send_client_command(
          sendspin::SendspinControllerCommand::VOLUME,
          static_cast<uint8_t>(std::roundf(std::min(1.0f, this->volume + this->volume_increment_) * 100.0f)),
          std::nullopt);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
      this->parent_->send_client_command(
          sendspin::SendspinControllerCommand::VOLUME,
          static_cast<uint8_t>(std::roundf(std::max(0.0f, this->volume - this->volume_increment_) * 100.0f)),
          std::nullopt);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_MUTE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::MUTE, std::nullopt, true);
      break;
    case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
      this->parent_->send_client_command(sendspin::SendspinControllerCommand::MUTE, std::nullopt, false);
      break;
    default:
      break;
  }
}

void SendspinMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "Sendspin Media Player: volume_increment=%.2f", this->volume_increment_);
}

}  // namespace esphome::sendspin_
#endif
