#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"

#define DFPLAYER_READ_BUFFER_LENGTH 25  // two messages + some extra

namespace esphome {
namespace dfplayer {

class DFPlayerComponent : public uart::UARTDevice, public PollingComponent {
 public:
  void setup() override;
  void update() override;
  void loop() override;

  void play_track(uint16_t track);

 protected:
  void send_cmd(uint8_t cmd, uint16_t argument = 0);

  char read_buffer_[DFPLAYER_READ_BUFFER_LENGTH];
  size_t read_pos_{0};
  uint8_t parse_index_{0};
  uint8_t watch_dog_{0};
  bool expect_ack_{false};
  bool registered_{false};

  std::string recipient_;
  std::string outgoing_message_;
  bool send_pending_;
};

template<typename... Ts> class PlayTrackAction : public Action<Ts...> {
 public:
  PlayTrackAction(DFPlayerComponent *dfplayer) : dfplayer_(dfplayer) {}
  TEMPLATABLE_VALUE(uint16_t, track)

  void play(Ts... x) override {
    auto track = this->track_.value(x...);
    this->dfplayer_->play_track(track);
  }

 protected:
  DFPlayerComponent *dfplayer_;
};

}  // namespace dfplayer
}  // namespace esphome
