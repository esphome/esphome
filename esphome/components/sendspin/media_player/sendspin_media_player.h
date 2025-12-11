#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_MEDIA_PLAYER)

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

 protected:
  // Receives commands from HA
  void control(const media_player::MediaPlayerCall &call) override;

  bool force_publish_state_{false};
};

}  // namespace sendspin
}  // namespace esphome
#endif
