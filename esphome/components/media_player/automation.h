#pragma once

#include "esphome/core/automation.h"
#include "media_player.h"

namespace esphome {

namespace media_player {

template<MediaPlayerCommand Command, typename... Ts>
class MediaPlayerCommandAction : public Action<Ts...>, public Parented<MediaPlayer> {
 public:
  void play(Ts... x) override { this->parent_->make_call().set_command(Command).perform(); }
};

template<typename... Ts>
using PlayAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_PLAY, Ts...>;
template<typename... Ts>
using PauseAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_PAUSE, Ts...>;
template<typename... Ts>
using StopAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_STOP, Ts...>;
template<typename... Ts>
using ToggleAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_TOGGLE, Ts...>;
template<typename... Ts>
using VolumeUpAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_VOLUME_UP, Ts...>;
template<typename... Ts>
using VolumeDownAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_VOLUME_DOWN, Ts...>;

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

template<MediaPlayerState State> class MediaPlayerStateTrigger : public Trigger<> {
 public:
  explicit MediaPlayerStateTrigger(MediaPlayer *player) {
    player->add_on_state_callback([this, player]() {
      if (player->state == State)
        this->trigger();
    });
  }
};

using IdleTrigger = MediaPlayerStateTrigger<MediaPlayerState::MEDIA_PLAYER_STATE_IDLE>;
using PlayTrigger = MediaPlayerStateTrigger<MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING>;
using PauseTrigger = MediaPlayerStateTrigger<MediaPlayerState::MEDIA_PLAYER_STATE_PAUSED>;
using AnnouncementTrigger = MediaPlayerStateTrigger<MediaPlayerState::MEDIA_PLAYER_STATE_ANNOUNCING>;

template<typename... Ts> class IsIdleCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_IDLE; }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING; }
};

template<typename... Ts> class IsPausedCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_PAUSED; }
};

template<typename... Ts> class IsAnnouncingCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_ANNOUNCING; }
};

}  // namespace media_player
}  // namespace esphome
