#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_MEDIA_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "esphome/components/media_player/media_player.h"
#include "esphome/components/sendspin/sendspin_hub.h"

namespace esphome::sendspin_ {

class SendspinMediaPlayer : public SendspinChild, public media_player::MediaPlayer {
 public:
  void setup() override;
  void dump_config() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;

  void set_volume_increment(float volume_increment) { this->volume_increment_ = volume_increment; }

  bool is_muted() const override { return this->muted_; }

 protected:
  // Receives commands from HA
  void control(const media_player::MediaPlayerCall &call) override;

  float volume_increment_{0.05f};
  bool muted_{false};
};

}  // namespace esphome::sendspin_
#endif
