#pragma once

#include "esphome/core/component.h"

#include "esphome/components/microphone/microphone.h"

#include "../i2s_audio.h"

namespace esphome {
namespace i2s_audio {

class I2SAudioMicrophone : public I2SAudioIn, public microphone::Microphone, public Component {
 public:
  void setup() override;
  void start() override;
  void stop() override;

  void loop() override;

  void set_din_pin(uint8_t pin) { this->din_pin_ = pin; }

 protected:
  void start_();
  void stop_();
  void read_();

  uint8_t din_pin_{0};
  std::vector<uint8_t> buffer_;

  HighFrequencyLoopRequester high_freq_;
};

}  // namespace i2s_audio
}  // namespace esphome
