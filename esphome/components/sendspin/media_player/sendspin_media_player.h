#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_MEDIA_PLAYER) && defined(USE_SENDSPIN_CONTROLLER)

#include "esphome/components/media_player/media_player.h"
#include "esphome/components/sendspin/sendspin_hub.h"

#include <sendspin/types.h>

namespace esphome::sendspin_ {

class SendspinMediaPlayer final : public SendspinChild, public media_player::MediaPlayer {
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

  // Recomputes `state` from the last known group playback state and (when the metadata role is
  // available) playback speed/duration, and publishes if it changed. A group can report PLAYING
  // while the track itself is paused - the server only surfaces that via metadata's
  // playback_speed, not the group update, so the two have to be combined here rather than each
  // setting state independently.
  void update_state_();

  float volume_increment_{0.05f};
  bool muted_{false};
  sendspin::SendspinPlaybackState group_playback_state_{sendspin::SendspinPlaybackState::STOPPED};
#ifdef USE_SENDSPIN_METADATA
  uint32_t playback_speed_{0};
  uint32_t track_duration_ms_{0};
#endif
};

}  // namespace esphome::sendspin_
#endif
