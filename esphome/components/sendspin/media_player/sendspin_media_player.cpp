#include "sendspin_media_player.h"

#if defined(USE_ESP_IDF) && defined(USE_MEDIA_PLAYER)

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <memory>
#include <optional>

#include <esp_timer.h>

namespace esphome {
namespace sendspin {

static const char *const TAG = "sendspin.media_player";

void SendspinMediaPlayer::setup() {
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
      // TODO: Group volume/mute commands need to be sent
      default:
        break;
    }
  }
}

}  // namespace sendspin
}  // namespace esphome
#endif
