#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

const size_t DFPLAYER_READ_BUFFER_LENGTH = 25;  // two messages + some extra

namespace esphome {
namespace dfplayer {

enum EqPreset {
  NORMAL = 0,
  POP = 1,
  ROCK = 2,
  JAZZ = 3,
  CLASSIC = 4,
  BASS = 5,
};

enum Device {
  USB = 1,
  TF_CARD = 2,
};

class DFPlayer : public uart::UARTDevice, public Component {
 public:
  void loop() override;

  void next() { this->send_cmd_(0x01); }
  void previous() { this->send_cmd_(0x02); }
  void play_file(uint16_t file) {
    this->ack_set_is_playing_ = true;
    this->send_cmd_(0x03, file);
  }
  void play_file_loop(uint16_t file) { this->send_cmd_(0x08, file); }
  void play_folder(uint16_t folder, uint16_t file);
  void play_folder_loop(uint16_t folder) { this->send_cmd_(0x17, folder); }
  void volume_up() { this->send_cmd_(0x04); }
  void volume_down() { this->send_cmd_(0x05); }
  void set_device(Device device) { this->send_cmd_(0x09, device); }
  void set_volume(uint8_t volume) { this->send_cmd_(0x06, volume); }
  void set_eq(EqPreset preset) { this->send_cmd_(0x07, preset); }
  void sleep() { this->send_cmd_(0x0A); }
  void reset() { this->send_cmd_(0x0C); }
  void start() { this->send_cmd_(0x0D); }
  void pause() {
    this->ack_reset_is_playing_ = true;
    this->send_cmd_(0x0E);
  }
  void stop() { this->send_cmd_(0x16); }
  void random() { this->send_cmd_(0x18); }

  bool is_playing() { return is_playing_; }
  void dump_config() override;

  void add_on_finished_playback_callback(std::function<void()> callback) {
    this->on_finished_playback_callback_.add(std::move(callback));
  }

 protected:
  void send_cmd_(uint8_t cmd, uint16_t argument = 0);
  void send_cmd_(uint8_t cmd, uint16_t high, uint16_t low) {
    this->send_cmd_(cmd, ((high & 0xFF) << 8) | (low & 0xFF));
  }
  uint8_t sent_cmd_{0};

  char read_buffer_[DFPLAYER_READ_BUFFER_LENGTH];
  size_t read_pos_{0};

  bool is_playing_{false};
  bool ack_set_is_playing_{false};
  bool ack_reset_is_playing_{false};

  CallbackManager<void()> on_finished_playback_callback_;
};

#define DFPLAYER_SIMPLE_ACTION(ACTION_CLASS, ACTION_METHOD) \
  template<typename... Ts> class ACTION_CLASS : public Action<Ts...>, public Parented<DFPlayer> { \
   public: \
    void play(Ts... x) override { this->parent_->ACTION_METHOD(); } \
  };

DFPLAYER_SIMPLE_ACTION(NextAction, next)
DFPLAYER_SIMPLE_ACTION(PreviousAction, previous)

template<typename... Ts> class PlayFileAction : public Action<Ts...>, public Parented<DFPlayer> {
 public:
  TEMPLATABLE_VALUE(uint16_t, file)
  TEMPLATABLE_VALUE(boolean, loop)
  void play(Ts... x) override {
    auto file = this->file_.value(x...);
    auto loop = this->loop_.value(x...);
    if (loop) {
      this->parent_->play_file_loop(file);
    } else {
      this->parent_->play_file(file);
    }
  }
};

template<typename... Ts> class PlayFolderAction : public Action<Ts...>, public Parented<DFPlayer> {
 public:
  TEMPLATABLE_VALUE(uint16_t, folder)
  TEMPLATABLE_VALUE(uint16_t, file)
  TEMPLATABLE_VALUE(boolean, loop)
  void play(Ts... x) override {
    auto folder = this->folder_.value(x...);
    auto file = this->file_.value(x...);
    auto loop = this->loop_.value(x...);
    if (loop) {
      this->parent_->play_folder_loop(folder);
    } else {
      this->parent_->play_folder(folder, file);
    }
  }
};

template<typename... Ts> class SetDeviceAction : public Action<Ts...>, public Parented<DFPlayer> {
 public:
  TEMPLATABLE_VALUE(Device, device)
  void play(Ts... x) override {
    auto device = this->device_.value(x...);
    this->parent_->set_device(device);
  }
};

template<typename... Ts> class SetVolumeAction : public Action<Ts...>, public Parented<DFPlayer> {
 public:
  TEMPLATABLE_VALUE(uint8_t, volume)
  void play(Ts... x) override {
    auto volume = this->volume_.value(x...);
    this->parent_->set_volume(volume);
  }
};

template<typename... Ts> class SetEqAction : public Action<Ts...>, public Parented<DFPlayer> {
 public:
  TEMPLATABLE_VALUE(EqPreset, eq)
  void play(Ts... x) override {
    auto eq = this->eq_.value(x...);
    this->parent_->set_eq(eq);
  }
};

DFPLAYER_SIMPLE_ACTION(SleepAction, sleep)
DFPLAYER_SIMPLE_ACTION(ResetAction, reset)
DFPLAYER_SIMPLE_ACTION(StartAction, start)
DFPLAYER_SIMPLE_ACTION(PauseAction, pause)
DFPLAYER_SIMPLE_ACTION(StopAction, stop)
DFPLAYER_SIMPLE_ACTION(RandomAction, random)

template<typename... Ts> class DFPlayerIsPlayingCondition : public Condition<Ts...>, public Parented<DFPlayer> {
 public:
  bool check(Ts... x) override { return this->parent_->is_playing(); }
};

class DFPlayerFinishedPlaybackTrigger : public Trigger<> {
 public:
  explicit DFPlayerFinishedPlaybackTrigger(DFPlayer *parent) {
    parent->add_on_finished_playback_callback([this]() { this->trigger(); });
  }
};

}  // namespace dfplayer
}  // namespace esphome
