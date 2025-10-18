#include "usb_audio_media_player.h"

#ifdef USE_ESP32
#ifdef USE_USB_AUDIO

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/core/log.h"

namespace esphome {
namespace usb_audio {

static const char *const TAG = "usb_audio.media_player";

void USBAudioMediaPlayer::setup() {
  speaker::SpeakerMediaPlayer::setup();
  this->last_connected_ = this->parent_ != nullptr && this->parent_->device_connected();
  this->stop_sent_ = false;
}

void USBAudioMediaPlayer::loop() {
  this->handle_connection_state_();
  speaker::SpeakerMediaPlayer::loop();
}

void USBAudioMediaPlayer::dump_config() {
  speaker::SpeakerMediaPlayer::dump_config();
  if (this->parent_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  USB device connected: %s", YESNO(this->parent_->device_connected()));
  }
}

void USBAudioMediaPlayer::handle_connection_state_() {
  const bool connected = this->parent_ != nullptr && this->parent_->device_connected();
  if (!connected && this->last_connected_) {
    this->stop_due_to_disconnect_();
  }
  if (connected) {
    this->stop_sent_ = false;
  }
  this->last_connected_ = connected;
}

void USBAudioMediaPlayer::stop_due_to_disconnect_() {
  if (this->stop_sent_) {
    return;
  }
  if (this->state == media_player::MEDIA_PLAYER_STATE_PLAYING ||
      this->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING ||
      this->state == media_player::MEDIA_PLAYER_STATE_PAUSED) {
    auto call = this->make_call();
    call.set_command(media_player::MEDIA_PLAYER_COMMAND_STOP);
    call.perform();
    ESP_LOGW(TAG, "USB device disconnected, stopping playback");
  }
  this->stop_sent_ = true;
}

}  // namespace usb_audio
}  // namespace esphome

#endif  // USE_USB_AUDIO
#endif  // USE_ESP32
