#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

const size_t DFPLAYER_READ_BUFFER_LENGTH = 25;  // two messages + some extra

namespace esphome::dfplayer {

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

// See the datasheet here:
// https://github.com/DFRobot/DFRobotDFPlayerMini/blob/master/doc/FN-M16P%2BEmbedded%2BMP3%2BAudio%2BModule%2BDatasheet.pdf
class DFPlayer final : public uart::UARTDevice, public Component {
 public:
  void loop() override;

  void next();
  void previous();
  void play_mp3(uint16_t file);
  void play_file(uint16_t file);
  void play_file_loop(uint16_t file);
  void play_file(uint16_t file, bool loop) { loop ? this->play_file_loop(file) : this->play_file(file); }
  void play_folder(uint16_t folder, uint16_t file);
  void play_folder_loop(uint16_t folder);
  // The loop command plays the whole folder, so file is ignored when loop is set.
  void play_folder(uint16_t folder, uint16_t file, bool loop) {
    loop ? this->play_folder_loop(folder) : this->play_folder(folder, file);
  }
  void volume_up();
  void volume_down();
  void set_device(Device device);
  void set_volume(uint8_t volume);
  void set_eq(EqPreset preset);
  void sleep();
  void reset();
  void start();
  void pause();
  void stop();
  void random();
  void set_current_track_repeat(bool enable);

  bool is_playing() { return is_playing_; }
  void dump_config() override;

  template<typename F> void add_on_finished_playback_callback(F &&callback) {
    this->on_finished_playback_callback_.add(std::forward<F>(callback));
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

}  // namespace esphome::dfplayer
