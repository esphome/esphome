#pragma once

#include "esphome/core/automation.h"
#include "media_player.h"

namespace esphome {

namespace media_player {

#define MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(ACTION_CLASS, ACTION_COMMAND) \
  template<typename... Ts> class ACTION_CLASS : public Action<Ts...>, public Parented<MediaPlayer> { \
    void play(Ts... x) override { \
      this->parent_->make_call().set_command(MediaPlayerCommand::MEDIA_PLAYER_COMMAND_##ACTION_COMMAND).perform(); \
    } \
  };

#define MEDIA_PLAYER_SIMPLE_STATE_TRIGGER(TRIGGER_CLASS, TRIGGER_STATE) \
  class TRIGGER_CLASS : public Trigger<> { \
   public: \
    explicit TRIGGER_CLASS(MediaPlayer *player) { \
      player->add_on_state_callback([this, player]() { \
        if (player->state == MediaPlayerState::MEDIA_PLAYER_STATE_##TRIGGER_STATE) \
          this->trigger(); \
      }); \
    } \
  };

MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(PlayAction, PLAY)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(PauseAction, PAUSE)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(StopAction, STOP)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(ToggleAction, TOGGLE)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(VolumeUpAction, VOLUME_UP)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(VolumeDownAction, VOLUME_DOWN)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(NextTrackAction, NEXT_TRACK)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(PreviousTrackAction, PREVIOUS_TRACK)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(TurnOnAction, TURN_ON)
MEDIA_PLAYER_SIMPLE_COMMAND_ACTION(TurnOffAction, TURN_OFF)

template<typename... Ts> class PlayMediaAction : public Action<Ts...>, public Parented<MediaPlayer> {
  TEMPLATABLE_VALUE(std::string, media_url)
  void play(Ts... x) override { this->parent_->make_call().set_media_url(this->media_url_.value(x...)).perform(); }
};

template<typename... Ts> class VolumeSetAction : public Action<Ts...>, public Parented<MediaPlayer> {
  TEMPLATABLE_VALUE(float, volume)
  void play(Ts... x) override { this->parent_->make_call().set_volume(this->volume_.value(x...)).perform(); }
};

class StateTrigger : public Trigger<> {
 public:
  explicit StateTrigger(MediaPlayer *player) {
    player->add_on_state_callback([this]() { this->trigger(); });
  }
};

MEDIA_PLAYER_SIMPLE_STATE_TRIGGER(IdleTrigger, IDLE)
MEDIA_PLAYER_SIMPLE_STATE_TRIGGER(PlayTrigger, PLAYING)
MEDIA_PLAYER_SIMPLE_STATE_TRIGGER(PauseTrigger, PAUSED)
MEDIA_PLAYER_SIMPLE_STATE_TRIGGER(AnnouncementTrigger, ANNOUNCING)
MEDIA_PLAYER_SIMPLE_STATE_TRIGGER(OnTrigger, ON)
MEDIA_PLAYER_SIMPLE_STATE_TRIGGER(OffTrigger, OFF)

template<typename... Ts> class IsIdleCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_IDLE; }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING; }
};

template<typename... Ts> class isOnCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_ON; }
};

template<typename... Ts> class isOffCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_OFF; }
};

}  // namespace media_player
}  // namespace esphome
