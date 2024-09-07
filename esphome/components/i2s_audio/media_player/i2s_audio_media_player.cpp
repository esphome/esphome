#include "i2s_audio_media_player.h"

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

#include "esphome/core/log.h"

namespace esphome {
namespace i2s_audio {

static const char *const TAG = "audio";

void I2SAudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  media_player::MediaPlayerState play_state = media_player::MEDIA_PLAYER_STATE_PLAYING;
  if (call.get_announcement().has_value()) {
    play_state = call.get_announcement().value() ? media_player::MEDIA_PLAYER_STATE_ANNOUNCING
                                                 : media_player::MEDIA_PLAYER_STATE_PLAYING;
  }
  if (call.get_media_url().has_value()) {
    this->current_url_ = call.get_media_url();
    if (this->i2s_state_ != I2S_STATE_STOPPED && this->audio_ != nullptr) {
      if (this->audio_->isRunning()) {
        this->audio_->stopSong();
      }
      if (this->connecttouri_(this->current_url_.value())) {
        this->state = play_state;
      } else {
        this->stop_();
        ESP_LOGD(TAG, "connecttouri_ failed");
      }
    } else {
      this->start();
    }
  }

  if (play_state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING) {
    this->is_announcement_ = true;
  }

  if (call.get_volume().has_value()) {
    this->volume = call.get_volume().value();
    this->set_volume_(volume);
    this->unmute_();
  }
  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->unmute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP: {
        float new_volume = this->volume + 0.1f;
        if (new_volume > 1.0f)
          new_volume = 1.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: {
        float new_volume = this->volume - 0.1f;
        if (new_volume < 0.0f)
          new_volume = 0.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
      default:
        break;
    }
    if (this->i2s_state_ != I2S_STATE_RUNNING) {
      return;
    }
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_PLAY:
        if (!this->audio_->isRunning())
          this->audio_->pauseResume();
        this->state = play_state;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE:
        if (this->audio_->isRunning())
          this->audio_->pauseResume();
        this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        break;
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        this->stop();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        this->audio_->pauseResume();
        if (this->audio_->isRunning()) {
          this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
        } else {
          this->state = media_player::MEDIA_PLAYER_STATE_PAUSED;
        }
        break;
      default:
        break;
    }
  }
  this->publish_state();
}

void I2SAudioMediaPlayer::mute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(true);
  } else {
    this->set_volume_(0.0f, false);
  }
  this->muted_ = true;
}
void I2SAudioMediaPlayer::unmute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(false);
  } else {
    this->set_volume_(this->volume, false);
  }
  this->muted_ = false;
}
void I2SAudioMediaPlayer::set_volume_(float volume, bool publish) {
  if (this->audio_ != nullptr)
    this->audio_->setVolume(remap<uint8_t, float>(volume, 0.0f, 1.0f, 0, this->audio_->maxVolume()));
  if (publish)
    this->volume = volume;
}

void I2SAudioMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Audio...");
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

void I2SAudioMediaPlayer::loop() {
  switch (this->i2s_state_) {
    case I2S_STATE_STARTING:
      this->start_();
      break;
    case I2S_STATE_RUNNING:
      this->play_();
      break;
    case I2S_STATE_STOPPING:
      this->stop_();
      break;
    case I2S_STATE_STOPPED:
      break;
  }
}

void I2SAudioMediaPlayer::play_() {
  this->audio_->loop();
  if ((this->state == media_player::MEDIA_PLAYER_STATE_PLAYING ||
       this->state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING) &&
      !this->audio_->isRunning()) {
    this->stop();
  }
}

void I2SAudioMediaPlayer::start() { this->i2s_state_ = I2S_STATE_STARTING; }
void I2SAudioMediaPlayer::start_() {
  if (!this->parent_->try_lock()) {
    return;  // Waiting for another i2s to return lock
  }

#if SOC_I2S_SUPPORTS_DAC
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    this->audio_ = make_unique<Audio>(true, this->internal_dac_mode_, this->parent_->get_port());
  } else {
#endif
    this->audio_ = make_unique<Audio>(false, 3, this->parent_->get_port());

    i2s_pin_config_t pin_config = this->parent_->get_pin_config();
    pin_config.data_out_num = this->dout_pin_;
    i2s_set_pin(this->parent_->get_port(), &pin_config);

    this->audio_->setI2SCommFMT_LSB(this->i2s_comm_fmt_lsb_);
    this->audio_->forceMono(this->external_dac_channels_ == 1);
    if (this->mute_pin_ != nullptr) {
      this->mute_pin_->setup();
      this->mute_pin_->digital_write(false);
    }
#if SOC_I2S_SUPPORTS_DAC
  }
#endif

  this->i2s_state_ = I2S_STATE_RUNNING;
  this->high_freq_.start();
  this->audio_->setVolumeSteps(255);  // use 255 steps for smoother volume control
  this->audio_->setVolume(remap<uint8_t, float>(this->volume, 0.0f, 1.0f, 0, this->audio_->maxVolume()));
  if (this->current_url_.has_value()) {
    if (this->connecttouri_(this->current_url_.value())) {
      if (this->is_announcement_) {
        this->state = media_player::MEDIA_PLAYER_STATE_ANNOUNCING;
      } else {
        this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
      }

      this->publish_state();
    } else {
      this->stop_();
      ESP_LOGD(TAG, "connecttouri_ failed");
    }
  }
}
void I2SAudioMediaPlayer::stop() {
  if (this->i2s_state_ == I2S_STATE_STOPPED) {
    return;
  }
  if (this->i2s_state_ == I2S_STATE_STARTING) {
    this->i2s_state_ = I2S_STATE_STOPPED;
    return;
  }
  this->i2s_state_ = I2S_STATE_STOPPING;
}
void I2SAudioMediaPlayer::stop_() {
  if (this->audio_->isRunning()) {
    this->audio_->stopSong();
    return;
  }

  this->audio_ = nullptr;
  this->current_url_ = {};
  this->parent_->unlock();
  this->i2s_state_ = I2S_STATE_STOPPED;

  this->high_freq_.stop();
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->publish_state();
  this->is_announcement_ = false;
}

media_player::MediaPlayerTraits I2SAudioMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(true);
  return traits;
};

void I2SAudioMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "Audio:");
  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "Audio failed to initialize!");
    return;
  }
#if SOC_I2S_SUPPORTS_DAC
  if (this->internal_dac_mode_ != I2S_DAC_CHANNEL_DISABLE) {
    switch (this->internal_dac_mode_) {
      case I2S_DAC_CHANNEL_LEFT_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Left");
        break;
      case I2S_DAC_CHANNEL_RIGHT_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Right");
        break;
      case I2S_DAC_CHANNEL_BOTH_EN:
        ESP_LOGCONFIG(TAG, "  Internal DAC mode: Left & Right");
        break;
      default:
        break;
    }
  } else {
#endif
    ESP_LOGCONFIG(TAG, "  External DAC channels: %d", this->external_dac_channels_);
    ESP_LOGCONFIG(TAG, "  I2S DOUT Pin: %d", this->dout_pin_);
    LOG_PIN("  Mute Pin: ", this->mute_pin_);
#if SOC_I2S_SUPPORTS_DAC
  }
#endif
}

bool I2SAudioMediaPlayer::connecttouri_(const std::string uri) {
  if (this->audio_ == nullptr) {
    return false;
  }

  // web stream?
  if (uri.find("http://", 0) == 0 || uri.find("https://", 0) == 0) {
    ESP_LOGD(TAG, "ConnectTo WebStream '%s'", uri.c_str());

    if (uri.size() > 2048) {
      // ESP32-audioI2S limits the URI to 2048 characters
      // (since https://github.com/schreibfaul1/ESP32-audioI2S/commit/b1b89a9f64b73b0b171493bf55e6158f37a0bccd)
      ESP_LOGE(TAG, "WebStream URI too long");
      return false;
    }

    return this->audio_->connecttohost(uri.c_str());
  }

  // local file?
  if (uri.find("file://", 0) == 0) {
    // format: file://<path>
    // const std::string path = uri.substr(7);
    // ESP_LOGD(TAG, "ConnectTo File '%s'", path.c_str());
    // return this->audio_->connecttoFS(?, uri.c_str() + 7);
    return false;
  }

  // text to speech?
  if (uri.find("tts://", 0) == 0) {
    // format: tts://<lang>:<text>
    const size_t colon = uri.find(':', 6);
    if (colon == std::string::npos || colon > 10) {
      // language code is expected to be 2-5 characters
      ESP_LOGW(TAG, "Invalid TTS URI");
      return false;
    }

    const std::string lang = uri.substr(6, colon - 6);
    const std::string text = uri.substr(colon + 1);

    ESP_LOGD(TAG, "ConnectTo TTS: lang='%s', text='%s'", lang.c_str(), text.c_str());
    return this->audio_->connecttospeech(text.c_str(), lang.c_str());
  }

  return false;
}

}  // namespace i2s_audio
}  // namespace esphome

void audio_info(const char *info) {
  using namespace esphome;
  ESP_LOGD("audio_info", "%s", info);
}

#endif  // USE_ESP32_FRAMEWORK_ARDUINO
