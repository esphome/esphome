#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_MEDIA_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "esphome/components/media_player/media_player.h"
#include "esphome/components/sendspin/sendspin_hub.h"

#include "esphome/core/component.h"

namespace esphome {
namespace sendspin {

class SendspinMediaPlayer : public Component, public media_player::MediaPlayer, public Parented<SendspinHub> {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_CONNECTION; }
  void setup() override;
  void loop() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;

  void set_volume_increment(uint8_t volume_increment) { this->volume_increment_ = volume_increment; }

 protected:
  // Receives commands from HA
  void control(const media_player::MediaPlayerCall &call) override;

  uint8_t volume_increment_{5};
  bool force_publish_state_{false};
};

}  // namespace sendspin
}  // namespace esphome
#endif
