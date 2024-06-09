#include "media_player.h"

#include "esphome/core/log.h"

namespace esphome {
namespace media_player {

static const char *const TAG = "media_player";

const char *media_player_state_to_string(MediaPlayerState state) {
  switch (state) {
    case MEDIA_PLAYER_STATE_ON:
      return "ON";
    case MEDIA_PLAYER_STATE_OFF:
      return "OFF";
    case MEDIA_PLAYER_STATE_IDLE:
      return "IDLE";
    case MEDIA_PLAYER_STATE_PLAYING:
      return "PLAYING";
    case MEDIA_PLAYER_STATE_PAUSED:
      return "PAUSED";
    case MEDIA_PLAYER_STATE_ANNOUNCING:
      return "ANNOUNCING";
    case MEDIA_PLAYER_STATE_NONE:
      return "NONE";
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
    case MEDIA_PLAYER_COMMAND_VOLUME_UP:
      return "VOLUME_UP";
    case MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
      return "VOLUME_DOWN";
    case MEDIA_PLAYER_COMMAND_NEXT_TRACK:
      return "NEXT_TRACK";
    case MEDIA_PLAYER_COMMAND_PREVIOUS_TRACK:
      return "PREVIOUS_TRACK";
    case MEDIA_PLAYER_COMMAND_TURN_ON:
      return "TURN_ON";
    case MEDIA_PLAYER_COMMAND_TURN_OFF:
      return "TURN_OFF";
    case MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST:
      return "CLEAR_PLAYLIST";
    case MEDIA_PLAYER_COMMAND_SHUFFLE:
      return "SHUFFLE";
    case MEDIA_PLAYER_COMMAND_UNSHUFFLE:
      return "UNSHUFFLE";
    case MEDIA_PLAYER_COMMAND_REPEAT_OFF:
      return "REPEAT_OFF";
    case MEDIA_PLAYER_COMMAND_REPEAT_ONE:
      return "REPEAT_ONE";
    case MEDIA_PLAYER_COMMAND_REPEAT_ALL:
      return "REPEAT_ALL";
    case MEDIA_PLAYER_COMMAND_JOIN:
      return "JOIN";
    case MEDIA_PLAYER_COMMAND_UNJOIN:
      return "UNJOIN";
    default:
      return "UNKNOWN";
  }
}

const char *media_player_enqueue_to_string(MediaPlayerEnqueue enqueue) {
  switch (enqueue) {
    case MEDIA_PLAYER_ENQUEUE_ADD:
      return "add";
    case MEDIA_PLAYER_ENQUEUE_NEXT:
      return "next";
    case MEDIA_PLAYER_ENQUEUE_PLAY:
      return "play";
    case MEDIA_PLAYER_ENQUEUE_REPLACE:
      return "replace";
    default:
      return "UNKNOWN";
  }
}

const char *media_player_repeat_mode_to_string(MediaPlayerRepeatMode repeat_mode) {
  switch (repeat_mode) {
    case MEDIA_PLAYER_REPEAT_ALL:
      return "all";
    case MEDIA_PLAYER_REPEAT_OFF:
      return "off";
    case MEDIA_PLAYER_REPEAT_ONE:
      return "one";
    default:
      return "UNKNOWN";
  }
}

const char *media_player_mrm_to_string(MediaPlayerMRM mrm) {
  switch (mrm) {
    case MEDIA_PLAYER_MRM_OFF:
      return "off";
    case MEDIA_PLAYER_MRM_FOLLOWER:
      return "follower";
    case MEDIA_PLAYER_MRM_LEADER:
      return "leader";
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
  if (this->enqueue_.has_value()) {
    ESP_LOGD(TAG, "  Enqueue: %s", media_player_enqueue_to_string(this->enqueue_.value()));
  }
  if (this->announcement_.has_value()) {
    ESP_LOGD(TAG, " Announcement: %s", this->announcement_.value() ? "yes" : "no");
  }
  if (this->mrm_.has_value()) {
    ESP_LOGD(TAG, "  MRM: %s", media_player_mrm_to_string(this->mrm_.value()));
  }
  if (this->group_members_.has_value()) {
    ESP_LOGD(TAG, "  group_members: %s", this->group_members_.value().c_str());
  }
  if (this->volume_.has_value()) {
    ESP_LOGD(TAG, "  Volume: %.2f", this->volume_.value());
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
  } else if (str_equals_case_insensitive(command, "NEXT_TRACK")) {
    this->set_command(MEDIA_PLAYER_COMMAND_NEXT_TRACK);
  } else if (str_equals_case_insensitive(command, "PREVIOUS_TRACK")) {
    this->set_command(MEDIA_PLAYER_COMMAND_PREVIOUS_TRACK);
  } else if (str_equals_case_insensitive(command, "TURN_ON")) {
    this->set_command(MEDIA_PLAYER_COMMAND_TURN_ON);
  } else if (str_equals_case_insensitive(command, "TURN_OFF")) {
    this->set_command(MEDIA_PLAYER_COMMAND_TURN_OFF);
  } else if (str_equals_case_insensitive(command, "CLEAR_PLAYLIST")) {
    this->set_command(MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST);
  } else if (str_equals_case_insensitive(command, "SHUFFLE")) {
    this->set_command(MEDIA_PLAYER_COMMAND_SHUFFLE);
  } else if (str_equals_case_insensitive(command, "UNSHUFFLE")) {
    this->set_command(MEDIA_PLAYER_COMMAND_UNSHUFFLE);
  } else if (str_equals_case_insensitive(command, "REPEAT_OFF")) {
    this->set_command(MEDIA_PLAYER_COMMAND_REPEAT_OFF);
  } else if (str_equals_case_insensitive(command, "REPEAT_ONE")) {
    this->set_command(MEDIA_PLAYER_COMMAND_REPEAT_ONE);
  } else if (str_equals_case_insensitive(command, "REPEAT_ALL")) {
    this->set_command(MEDIA_PLAYER_COMMAND_REPEAT_ALL);
  } else if (str_equals_case_insensitive(command, "JOIN")) {
    this->set_command(MEDIA_PLAYER_COMMAND_JOIN);
  } else if (str_equals_case_insensitive(command, "UNJOIN")) {
    this->set_command(MEDIA_PLAYER_COMMAND_UNJOIN);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized command %s", this->parent_->get_name().c_str(), command.c_str());
  }
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_media_url(const std::string &media_url) {
  this->media_url_ = media_url;
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_enqueue(MediaPlayerEnqueue enqueue) {
  this->enqueue_ = enqueue;
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_enqueue(const std::string &enqueue) {
  if (str_equals_case_insensitive(enqueue, "ADD")) {
    this->set_enqueue(MEDIA_PLAYER_ENQUEUE_ADD);
  } else if (str_equals_case_insensitive(enqueue, "NEXT")) {
    this->set_enqueue(MEDIA_PLAYER_ENQUEUE_NEXT);
  } else if (str_equals_case_insensitive(enqueue, "PLAY")) {
    this->set_enqueue(MEDIA_PLAYER_ENQUEUE_PLAY);
  } else if (str_equals_case_insensitive(enqueue, "REPLACE")) {
    this->set_enqueue(MEDIA_PLAYER_ENQUEUE_REPLACE);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized enqueue %s", this->parent_->get_name().c_str(), enqueue.c_str());
  }
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

MediaPlayerCall &MediaPlayerCall::set_mrm(MediaPlayerMRM mrm) {
  this->mrm_ = mrm;
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_mrm(const std::string &mrm) {
  if (str_equals_case_insensitive(mrm, "ADD")) {
    this->set_mrm(MEDIA_PLAYER_MRM_OFF);
  } else if (str_equals_case_insensitive(mrm, "FOLLOWER")) {
    this->set_mrm(MEDIA_PLAYER_MRM_FOLLOWER);
  } else if (str_equals_case_insensitive(mrm, "LEADER")) {
    this->set_mrm(MEDIA_PLAYER_MRM_LEADER);
  } else {
    ESP_LOGW(TAG, "'%s' - Unrecognized mrm %s", this->parent_->get_name().c_str(), mrm.c_str());
  }
  return *this;
}

MediaPlayerCall &MediaPlayerCall::set_group_members(const std::string &group_members) {
  this->group_members_ = group_members;
  return *this;
}

void MediaPlayer::add_on_state_callback(std::function<void()> &&callback) {
  this->state_callback_.add(std::move(callback));
}

void MediaPlayer::publish_state() { this->state_callback_.call(); }

}  // namespace media_player
}  // namespace esphome
