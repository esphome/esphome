#pragma once

#ifdef USE_ESP32
#ifdef USE_USB_AUDIO

#include "esphome/components/speaker/media_player/speaker_media_player.h"

#include "esphome/components/usb_audio/usb_audio.h"

namespace esphome {
namespace usb_audio {

class USBAudioMediaPlayer : public speaker::SpeakerMediaPlayer, public Parented<USBAudioComponent> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

 protected:
  void handle_connection_state_();
  void stop_due_to_disconnect_();

  bool last_connected_{false};
  bool stop_sent_{false};
};

}  // namespace usb_audio
}  // namespace esphome

#endif  // USE_USB_AUDIO
#endif  // USE_ESP32
