#ifdef USE_ESP32
#ifndef USE_I2S_LEGACY

#include "esphome_snapclient.h"
#include "esphome/core/log.h"
#include "esphome/components/network/util.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2s_std.h"
#if CONFIG_USE_DSP_PROCESSOR
#include "dsp_processor.h"
#endif

#include "snapclient.h"
#include "player.h"

namespace esphome {
namespace snapclient {

SnapClientComponent *global_snapclient = nullptr;

static void player_set_mute(bool mute) { global_snapclient->set_mute_from_isr(mute, false); }

static void set_mute_state(bool mute) { global_snapclient->set_mute_from_isr(mute, true); }

static void audio_set_volume(int volume) { global_snapclient->set_volume_from_isr(volume); }

static void player_state_changed() {
  if (global_snapclient->playerStateChangedMutex != NULL) {
    xSemaphoreGive(global_snapclient->playerStateChangedMutex);
  }
}

void SnapClientComponent::setup() {
  if (!this->parent_->try_lock()) {
    this->mark_failed();
    return;
  }
  global_snapclient = this;
  ESP_LOGD(TAG, "init player");
  i2s_std_gpio_config_t i2s_pin_config0 = this->parent_->get_pin_config();
  i2s_pin_config0.dout = (gpio_num_t) this->dout_pin_;

  this->audio_dac_semaphore_ = xSemaphoreCreateMutex();
  this->audio_q_hdl_ = xQueueCreate(1, sizeof(audioDACdata_t));
  this->playerStateChangedMutex = xSemaphoreCreateBinary();

#ifdef USE_AUDIO_DAC
  if (this->audio_dac_) {
    this->audio_dac_->set_mute_on();
  }
#endif
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->setup();
    this->mute_pin_->digital_write(false);
  }

  init_player(i2s_pin_config0, this->parent_->get_port(), player_set_mute);
  add_player_state_cb(player_state_changed);
  init_snapcast(audio_set_volume, set_mute_state);

#if CONFIG_USE_DSP_PROCESSOR
  dsp_processor_init();
#endif
  this->network_initialized_ = false;
}

void SnapClientComponent::loop() {
  if (!this->network_initialized_ && network::is_connected()) {
    start_snapcast();
    this->network_initialized_ = true;
  }
  if (xQueueReceive(this->audio_q_hdl_, &(this->dac_data_), 0) == pdTRUE) {
    this->dac_control_();
  }

  if (xSemaphoreTake(this->playerStateChangedMutex, 0) == pdTRUE) {
    player_state_e state_new = get_player_state();
    if (state_new != this->state) {
      ESP_LOGI(TAG, "Player state changed: %d -> %d", this->state, state_new);
      if (state_new == PAUSED) {
        this->parent_->unlock();
      } else if (this->state == PAUSED && !this->parent_->try_lock()) {
        pause_player(true);
      }
      this->state = state_new;
    }
  }
}

void SnapClientComponent::dac_control_() {
  static audioDACdata_t dac_data_old = {
      .playerMute = true,
      .stateMute = true,
      .volume = -1,
  };
  if (this->dac_data_.playerMute != dac_data_old.playerMute || this->dac_data_.stateMute != dac_data_old.stateMute) {
    // if either player or state mute is active, we need to mute the output
    bool mute = this->dac_data_.playerMute || this->dac_data_.stateMute;
    if (mute != this->mute_state_) {
      if (this->mute_pin_ != nullptr) {
        this->mute_pin_->digital_write(!mute);  // for most DACs mute = low
      }
#ifdef USE_AUDIO_DAC
      if (this->audio_dac_) {
        if (mute) {
          this->audio_dac_->set_mute_on();
        } else {
          this->audio_dac_->set_mute_off();
        }
      }
#endif
      this->mute_state_ = mute;
      ESP_LOGD(TAG, "%s", mute ? "Mute" : "Unmute");
    }
  }
  if (this->dac_data_.volume != dac_data_old.volume) {
    this->volume_ = (float) dac_data_.volume / 100;
#ifdef USE_AUDIO_DAC
    if (this->audio_dac_ != nullptr) {
      this->audio_dac_->set_volume(this->volume_);
    }
#endif
  }
  dac_data_old = this->dac_data_;
}

void SnapClientComponent::set_mute_from_isr(bool mute, bool set_state) {
  xSemaphoreTake(this->audio_dac_semaphore_, portMAX_DELAY);
  if (set_state && (mute != this->dac_data_external_.stateMute)) {
    this->dac_data_external_.stateMute = mute;
    xQueueOverwrite(this->audio_q_hdl_, &this->dac_data_external_);
  } else if (!set_state && mute != this->dac_data_external_.playerMute) {
    this->dac_data_external_.playerMute = mute;
    xQueueOverwrite(this->audio_q_hdl_, &this->dac_data_external_);
  }
  xSemaphoreGive(this->audio_dac_semaphore_);
}

void SnapClientComponent::set_volume_from_isr(int volume) {
  xSemaphoreTake(this->audio_dac_semaphore_, portMAX_DELAY);
  if (volume != this->dac_data_external_.volume) {
    this->dac_data_external_.volume = volume;
    xQueueOverwrite(this->audio_q_hdl_, &this->dac_data_external_);
  }
  xSemaphoreGive(this->audio_dac_semaphore_);
}

}  // namespace snapclient
}  // namespace esphome

#endif
#endif
