#pragma once

#include "esphome/core/automation.h"
#include "media_player.h"

namespace esphome {

namespace media_player {

template<typename... Ts> class PlayAction : public Action<Ts...>, public Parented<MediaPlayer> {
  void play(Ts... x) override {
    this->parent_->make_call().set_command(MediaPlayerCommand::MEDIA_PLAYER_COMMAND_PLAY).perform();
  }
};

template<typename... Ts> class ToggleAction : public Action<Ts...>, public Parented<MediaPlayer> {
  void play(Ts... x) override {
    this->parent_->make_call().set_command(MediaPlayerCommand::MEDIA_PLAYER_COMMAND_TOGGLE).perform();
  }
};

template<typename... Ts> class PauseAction : public Action<Ts...>, public Parented<MediaPlayer> {
  void play(Ts... x) override {
    this->parent_->make_call().set_command(MediaPlayerCommand::MEDIA_PLAYER_COMMAND_PAUSE).perform();
  }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<MediaPlayer> {
  void play(Ts... x) override {
    this->parent_->make_call().set_command(MediaPlayerCommand::MEDIA_PLAYER_COMMAND_STOP).perform();
  }
};

}  // namespace media_player
}  // namespace esphome
