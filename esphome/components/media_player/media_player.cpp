#include "media_player.h"

#include "esphome/core/log.h"

namespace esphome {
namespace media_player {

static const char *const TAG = "media_player";

const char *media_player_state_to_string(MediaPlayerState state) {
  switch (state) {
    case MEDIA_PLAYER_STATE_IDLE:
      return "IDLE";
    case MEDIA_PLAYER_STATE_PLAYING:
      return "PLAYING";
    case MEDIA_PLAYER_STATE_PAUSED:
      return "PAUSED";
    case MEDIA_PLAYER_STATE_ANNOUNCING:
      return "ANNOUNCING";
    case MEDIA_PLAYER_STATE_NONE:
    default:
      return "UNKNOWN";
  }
}

const char *media_player_command_to_string(MediaPlayerCommand command) {
  switch (command) {
    case MEDIA_PLAYER_COMMAND_PLAY:
      return "PLAY";
    case MEDIA_PLAYER_COMMAND_PAUSE:
      return "PAUSE";
    case MEDIA_PLAYER_COMMAND_STOP:
      return "STOP";
    case MEDIA_PLAYER_COMMAND_MUTE:
      return "MUTE";
    case MEDIA_PLAYER_COMMAND_UNMUTE:
      return "UNMUTE";
    case MEDIA_PLAYER_COMMAND_TOGGLE:
      return "TOGGLE";
    default:
      return "UNKNOWN";
  }
}

void MediaPlayerCall::validate_() {
  if (this->media_url_.has_value()) {
    if (this->command_.has_value()) {
      ESP_LOGW(TAG, "MediaPlayerCall: Setting both command and media_url is not needed.");
      this->command_.reset();
    }
  }
  if (this->volume_.has_value()) {
    if (this->volume_.value() < 0.0f || this->volume_.value() > 1.0f) {
      ESP_LOGW(TAG, "MediaPlayerCall: Volume must be between 0.0 and 1.0.");
      this->volume_.reset();
    }
  }
}

void MediaPlayerCall::perform() {
  ESP_LOGD(TAG, "'%s' - Setting", this->parent_->get_name().c_str());
  this->validate_();
  if (this->command_.has_value()) {
    const char *command_s = media_player_command_to_string(this->command_.value());
    ESP_LOGD(TAG, "  Command: %s", command_s);
  }
  if (this->media_url_.has_value()) {
    ESP_LOGD(TAG, "  Media URL: %s", this->media_url_.value().c_str());
  }
  if (this->volume_.has_value()) {
    ESP_LOGD(TAG, "  Volume: %.2f", this->volume_.value());
  }
  if (this->announcement_.has_value()) {
    ESP_LOGD(TAG, " Announcement: %s", this->announcement_.value() ? "yes" : "no");
  }
  this->parent_->control(*this);
}

MediaPlayerCall &MediaPlayerCall::set_command(MediaPlayerCommand command) {
  this->command_ = command;
  return *this;
}
MediaPlayerCall &MediaPlayerCall::set_command(optional<MediaPlayerCommand> command) {
  this->command_ = command;
  return *this;
}
MediaPlayerCall &MediaPlayerCall::set_command(const std::string &command) {
  if (str_equals_case_insensitive(command, "PLAY")) {
    this->set_command(MEDIA_PLAYER_COMMAND_PLAY);
  } else if (str_equals_case_insensitive(command, "PAUSE")) {
    this->set_command(MEDIA_PLAYER_COMMAND_PAUSE);
  } else if (str_equals_case_insensitive(command, "STOP")) {
    this->set_command(MEDIA_PLAYER_COMMAND_STOP);
  } else if (str_equals_case_insensitive(command, "MUTE")) {
    this->set_command(MEDIA_PLAYER_COMMAND_MUTE);
  } else if (str_equals_case_insensitive(command, "UNMUTE")) {
    this->set_command(MEDIA_PLAYER_COMMAND_UNMUTE);
  } else if (str_equals_case_insensitive(command, "TOGGLE")) {
    this->set_command(MEDIA_PLAYER_COMMAND_TOGGLE);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized command %s", this->parent_->get_name().c_str(), command.c_str());
  }
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_media_url(const std::string &media_url) {
  this->media_url_ = media_url;
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_volume(float volume) {
  this->volume_ = volume;
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_announcement(bool announce) {
  this->announcement_ = announce;
  return *this;
}

void MediaPlayer::add_on_state_callback(std::function<void()> &&callback) {
  this->state_callback_.add(std::move(callback));
}

void MediaPlayer::publish_state() { this->state_callback_.call(); }

}  // namespace media_player
}  // namespace esphome
