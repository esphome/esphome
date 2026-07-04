#include "sendspin_media_player.h"

#if defined(USE_ESP32) && defined(USE_MEDIA_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <sendspin/types.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

#include <esp_timer.h>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.media_player";

// THREAD CONTEXT: Main loop. The callbacks registered here also fire on the main loop,
// since SendspinHub dispatches group updates and controller state from client_->loop().
void SendspinMediaPlayer::setup() {
  // Register for group updates to sync playback state
  this->parent_->add_group_update_callback([this](const sendspin::GroupUpdateObject &group_obj) {
    if (group_obj.playback_state.has_value()) {
      media_player::MediaPlayerState new_state;
      switch (group_obj.playback_state.value()) {
        case sendspin::SendspinPlaybackState::PLAYING:
          new_state = media_player::MEDIA_PLAYER_STATE_PLAYING;
          break;
        case sendspin::SendspinPlaybackState::STOPPED:
        default:
          new_state = media_player::MEDIA_PLAYER_STATE_IDLE;
          break;
      }
      if (this->state != new_state) {
        this->state = new_state;
        this->publish_state();
        ESP_LOGD(TAG, "State changed to %s", media_player::media_player_state_to_string(this->state));
      }
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

  // Publish an initial state
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->publish_state();
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
