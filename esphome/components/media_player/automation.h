#pragma once

#include "esphome/core/automation.h"
#include "media_player.h"

namespace esphome::media_player {

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

template<typename... Ts> class IsIdleCondition final : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_IDLE; }
};

template<typename... Ts> class IsPlayingCondition final : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_PLAYING; }
};

template<typename... Ts> class IsPausedCondition final : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_PAUSED; }
};

template<typename... Ts> class IsAnnouncingCondition final : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override {
    return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_ANNOUNCING;
  }
};

template<typename... Ts> class IsOnCondition final : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_ON; }
};

template<typename... Ts> class IsOffCondition final : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->state == MediaPlayerState::MEDIA_PLAYER_STATE_OFF; }
};

template<typename... Ts> class IsMutedCondition final : public Condition<Ts...>, public Parented<MediaPlayer> {
 public:
  bool check(const Ts &...x) override { return this->parent_->is_muted(); }
};

}  // namespace esphome::media_player
