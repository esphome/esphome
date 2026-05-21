#pragma once

#include "esphome/core/automation.h"
#include "media_player.h"

namespace esphome::media_player {

template<MediaPlayerCommand Command, typename... Ts>
class MediaPlayerCommandAction : public Action<Ts...>, public Parented<MediaPlayer> {
 public:
  TEMPLATABLE_VALUE(bool, announcement);
  void play(const Ts &...x) override {
    this->parent_->make_call().set_command(Command).set_announcement(this->announcement_.value(x...)).perform();
  }
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
template<typename... Ts>
using TurnOnAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_TURN_ON, Ts...>;
template<typename... Ts>
using TurnOffAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_TURN_OFF, Ts...>;
template<typename... Ts>
using NextAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_NEXT, Ts...>;
template<typename... Ts>
using PreviousAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_PREVIOUS, Ts...>;
template<typename... Ts>
using MuteAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_MUTE, Ts...>;
template<typename... Ts>
using UnmuteAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_UNMUTE, Ts...>;
template<typename... Ts>
using RepeatOffAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_REPEAT_OFF, Ts...>;
template<typename... Ts>
using RepeatOneAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_REPEAT_ONE, Ts...>;
template<typename... Ts>
using RepeatAllAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_REPEAT_ALL, Ts...>;
template<typename... Ts>
using ShuffleAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_SHUFFLE, Ts...>;
template<typename... Ts>
using UnshuffleAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_UNSHUFFLE, Ts...>;
template<typename... Ts>
using GroupJoinAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_GROUP_JOIN, Ts...>;
template<typename... Ts>
using ClearPlaylistAction = MediaPlayerCommandAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST, Ts...>;

template<MediaPlayerCommand Command, typename... Ts>
class MediaPlayerMediaAction : public Action<Ts...>, public Parented<MediaPlayer> {
  TEMPLATABLE_VALUE(std::string, media_url)
  TEMPLATABLE_VALUE(bool, announcement)
  void play(const Ts &...x) override {
    auto call = this->parent_->make_call();
    if constexpr (Command != MediaPlayerCommand::MEDIA_PLAYER_COMMAND_PLAY)
      call.set_command(Command);
    call.set_media_url(this->media_url_.value(x...)).set_announcement(this->announcement_.value(x...)).perform();
  }
};

template<typename... Ts>
using PlayMediaAction = MediaPlayerMediaAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_PLAY, Ts...>;
template<typename... Ts>
using EnqueueMediaAction = MediaPlayerMediaAction<MediaPlayerCommand::MEDIA_PLAYER_COMMAND_ENQUEUE, Ts...>;

template<typename... Ts> class VolumeSetAction : public Action<Ts...>, public Parented<MediaPlayer> {
  TEMPLATABLE_VALUE(float, volume)
  void play(const Ts &...x) override { this->parent_->make_call().set_volume(this->volume_.value(x...)).perform(); }
};

/// Callback forwarder that triggers an Automation<> on any state change.
/// Pointer-sized (single Automation* field) to fit inline in Callback::ctx_.
struct StateAnyForwarder {
  Automation<> *automation;
  void operator()(MediaPlayerState /*state*/) const { this->automation->trigger(); }
};

/// Callback forwarder that triggers an Automation<> only when a specific media player state is entered.
/// Pointer-sized (single Automation* field) to fit inline in Callback::ctx_.
template<MediaPlayerState State> struct StateEnterForwarder {
  Automation<> *automation;
  void operator()(MediaPlayerState state) const {
    if (state == State)
      this->automation->trigger();
  }
};

static_assert(sizeof(StateAnyForwarder) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<StateAnyForwarder>);
static_assert(sizeof(StateEnterForwarder<MediaPlayerState::MEDIA_PLAYER_STATE_IDLE>) <= sizeof(void *));
static_assert(std::is_trivially_copyable_v<StateEnterForwarder<MediaPlayerState::MEDIA_PLAYER_STATE_IDLE>>);

template<typename... Ts> class IsIdleCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_IDLE; }
};

template<typename... Ts> class IsPlayingCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING; }
};

template<typename... Ts> class IsPausedCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_PAUSED; }
};

template<typename... Ts> class IsAnnouncingCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override {
    return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_ANNOUNCING;
  }
};

template<typename... Ts> class IsOnCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_ON; }
};

template<typename... Ts> class IsOffCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_OFF; }
};

template<typename... Ts> class IsMutedCondition : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_muted(); }
};

}  // namespace esphome::media_player
