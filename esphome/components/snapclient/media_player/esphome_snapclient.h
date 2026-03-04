#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifndef USE_I2S_LEGACY

#include <string>

#include "esphome/core/component.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/i2s_audio/i2s_audio.h"
#include "esphome/core/gpio.h"
#include "player.h"

#ifdef USE_AUDIO_DAC
#include "esphome/components/audio_dac/audio_dac.h"
#endif

namespace esphome::snapclient {

using audioDACdata_t = struct AudioDaCdataS {
  bool playerMute;
  bool stateMute;
  int volume;
};

static const char *const TAG = "snapclient";

class SnapClientComponent : public i2s_audio::I2SAudioOut, public media_player::MediaPlayer, public Component {
 public:
  void set_mute_pin(GPIOPin *mute_pin) { this->mute_pin_ = mute_pin; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
  void set_snapserver_hostname(const std::string &hostname) { this->snapserver_hostname_ = hostname; }
  void set_snapserver_port(uint16_t port) { this->snapserver_port_ = port; }
  void set_snapserver_use_mdns(bool use_mdns) { this->snapserver_use_mdns_ = use_mdns; }
#ifdef USE_AUDIO_DAC
  void set_audio_dac(audio_dac::AudioDac *audio_dac) { this->audio_dac_ = audio_dac; }
#endif

  media_player::MediaPlayerTraits get_traits() override;
  bool is_muted() const override { return this->mute_state_; }

  void set_mute_from_isr(bool mute, bool set_state);
  void set_volume_from_isr(int volume);
  SemaphoreHandle_t playerStateChangedMutex;
  player_state_e player_state{IDLE};

 protected:
  media_player::MediaPlayerState get_state_from_player_state_(player_state_e state);
  void control(const media_player::MediaPlayerCall &call) override;

  void set_mute_(bool mute);
  void set_volume_(float volume, bool publish = true);

  void dac_control_();
  bool has_lock_{false};
  bool lock_() {
    if (!this->has_lock_ && this->parent_->try_lock()) {
      this->has_lock_ = true;
    }
    return this->has_lock_;
  }
  void unlock_() {
    if (this->has_lock_) {
      this->parent_->unlock();
      this->has_lock_ = false;
    }
  }
  float volume_{1.0f};
  bool mute_state_{true};
  GPIOPin *mute_pin_{nullptr};
  uint8_t dout_pin_;
  std::string snapserver_hostname_{};
  uint16_t snapserver_port_{1704};
  bool snapserver_use_mdns_{true};
  bool network_initialized_{false};
  audioDACdata_t dac_data_;
  audioDACdata_t dac_data_external_;
  SemaphoreHandle_t audio_dac_semaphore_;
  QueueHandle_t audio_q_hdl_;
#ifdef USE_AUDIO_DAC
  audio_dac::AudioDac *audio_dac_{nullptr};
#endif
};

}  // namespace esphome::snapclient

#endif
#endif
