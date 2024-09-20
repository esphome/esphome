#pragma once

#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace media_player {

enum MediaPlayerState : uint8_t {
  MEDIA_PLAYER_STATE_NONE = 0,
  MEDIA_PLAYER_STATE_IDLE = 1,
  MEDIA_PLAYER_STATE_PLAYING = 2,
  MEDIA_PLAYER_STATE_PAUSED = 3,
  MEDIA_PLAYER_STATE_ANNOUNCING = 4,
  MEDIA_PLAYER_STATE_OFF = 5,
  MEDIA_PLAYER_STATE_ON = 6,
};
const char *media_player_state_to_string(MediaPlayerState state);

enum MediaPlayerCommand : uint8_t {
  MEDIA_PLAYER_COMMAND_PLAY = 0,
  MEDIA_PLAYER_COMMAND_PAUSE = 1,
  MEDIA_PLAYER_COMMAND_STOP = 2,
  MEDIA_PLAYER_COMMAND_MUTE = 3,
  MEDIA_PLAYER_COMMAND_UNMUTE = 4,
  MEDIA_PLAYER_COMMAND_TOGGLE = 5,
  MEDIA_PLAYER_COMMAND_VOLUME_UP = 6,
  MEDIA_PLAYER_COMMAND_VOLUME_DOWN = 7,
  MEDIA_PLAYER_COMMAND_NEXT_TRACK = 8,
  MEDIA_PLAYER_COMMAND_PREVIOUS_TRACK = 9,
  MEDIA_PLAYER_COMMAND_TURN_ON = 10,
  MEDIA_PLAYER_COMMAND_TURN_OFF = 11,
  MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST = 12,
  MEDIA_PLAYER_COMMAND_SHUFFLE = 13,
  MEDIA_PLAYER_COMMAND_UNSHUFFLE = 14,
  MEDIA_PLAYER_COMMAND_REPEAT_OFF = 15,
  MEDIA_PLAYER_COMMAND_REPEAT_ONE = 16,
  MEDIA_PLAYER_COMMAND_REPEAT_ALL = 17,
  MEDIA_PLAYER_COMMAND_JOIN = 18,
  MEDIA_PLAYER_COMMAND_UNJOIN = 19,
};
const char *media_player_command_to_string(MediaPlayerCommand command);

enum MediaPlayerEnqueue : uint8_t {
  MEDIA_PLAYER_ENQUEUE_ADD = 0,
  MEDIA_PLAYER_ENQUEUE_NEXT = 1,
  MEDIA_PLAYER_ENQUEUE_PLAY = 2,
  MEDIA_PLAYER_ENQUEUE_REPLACE = 3,
};
const char *media_player_enqueue_to_string(MediaPlayerEnqueue enqueue);

enum MediaPlayerRepeatMode : uint8_t {
  MEDIA_PLAYER_REPEAT_ALL = 0,
  MEDIA_PLAYER_REPEAT_OFF = 1,
  MEDIA_PLAYER_REPEAT_ONE = 2,
};
const char *media_player_repeat_mode_to_string(MediaPlayerRepeatMode repeat_mode);

enum MediaPlayerMRM : uint8_t {
  MEDIA_PLAYER_MRM_OFF = 0,
  MEDIA_PLAYER_MRM_FOLLOWER = 1,
  MEDIA_PLAYER_MRM_LEADER = 2,
};
const char *media_player_mrm_to_string(MediaPlayerMRM mrm);

enum class MediaPlayerFormatPurpose : uint8_t {
  PURPOSE_DEFAULT = 0,
  PURPOSE_ANNOUNCEMENT = 1,
};

struct MediaPlayerSupportedFormat {
  std::string format;
  uint32_t sample_rate;
  uint32_t num_channels;
  MediaPlayerFormatPurpose purpose;
  uint32_t sample_bytes;
};

class MediaPlayer;

class MediaPlayerTraits {
 public:
  MediaPlayerTraits() = default;

  void set_supports_pause(bool supports_pause) { this->supports_pause_ = supports_pause; }

  bool get_supports_pause() const { return this->supports_pause_; }

  std::vector<MediaPlayerSupportedFormat> &get_supported_formats() { return this->supported_formats_; }

  void set_supports_next_previous_track(bool supports_next_previous_track) {
    this->supports_next_previous_track_ = supports_next_previous_track;
  }

  bool get_supports_next_previous_track() const { return this->supports_next_previous_track_; }

  void set_supports_turn_off_on(bool supports_turn_off_on) { this->supports_turn_off_on_ = supports_turn_off_on; }

  bool get_supports_turn_off_on() const { return this->supports_turn_off_on_; }

  void set_supports_grouping(bool supports_grouping) { this->supports_grouping_ = supports_grouping; }

  bool get_supports_grouping() const { return this->supports_grouping_; }

 protected:
  bool supports_pause_{false};
  std::vector<MediaPlayerSupportedFormat> supported_formats_{};
  bool supports_next_previous_track_{false};
  bool supports_turn_off_on_{false};
  bool supports_grouping_{false};
};

class MediaPlayerCall {
 public:
  MediaPlayerCall(MediaPlayer *parent) : parent_(parent) {}

  MediaPlayerCall &set_command(MediaPlayerCommand command);
  MediaPlayerCall &set_command(optional<MediaPlayerCommand> command);
  MediaPlayerCall &set_command(const std::string &command);

  MediaPlayerCall &set_media_url(const std::string &url);
  MediaPlayerCall &set_enqueue(MediaPlayerEnqueue enqueue);
  MediaPlayerCall &set_enqueue(const std::string &enqueue);

  MediaPlayerCall &set_volume(float volume);
  MediaPlayerCall &set_announcement(bool announce);
  MediaPlayerCall &set_group_members(const std::string &group_members);

  void perform();

  const optional<MediaPlayerCommand> &get_command() const { return command_; }
  const optional<std::string> &get_media_url() const { return media_url_; }
  const optional<MediaPlayerEnqueue> &get_enqueue() const { return enqueue_; }
  const optional<float> &get_volume() const { return volume_; }
  const optional<bool> &get_announcement() const { return announcement_; }
  const optional<std::string> &get_group_members() const { return group_members_; }

 protected:
  void validate_();
  MediaPlayer *const parent_;
  optional<MediaPlayerCommand> command_;
  optional<std::string> media_url_;
  optional<MediaPlayerEnqueue> enqueue_;
  optional<float> volume_;
  optional<bool> announcement_;
  optional<std::string> group_members_{};
};

class MediaPlayer : public EntityBase {
 public:
  MediaPlayerState state{MEDIA_PLAYER_STATE_NONE};
  float volume{1.0f};

  MediaPlayerCall make_call() { return MediaPlayerCall(this); }

  void publish_state();

  void add_on_state_callback(std::function<void()> &&callback);

  virtual bool is_muted() const { return false; }

  virtual bool is_shuffle() const { return false; }

  virtual std::string repeat() const { return ""; }

  virtual std::string artist() const { return ""; }

  virtual std::string album() const { return ""; }

  virtual std::string title() const { return ""; }

  virtual int duration() const { return 0; }

  virtual int position() const { return 0; }

  virtual MediaPlayerTraits get_traits() = 0;

 protected:
  friend MediaPlayerCall;

  virtual void control(const MediaPlayerCall &call) = 0;

  CallbackManager<void()> state_callback_{};
};

}  // namespace media_player
}  // namespace esphome
