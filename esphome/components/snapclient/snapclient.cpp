#ifdef USE_ESP32
#ifndef USE_I2S_LEGACY

#include "snapclient.h"
#include "decoder.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2s_std.h"
#if CONFIG_USE_DSP_PROCESSOR
#include "dsp_processor.h"
#endif

#include "player.h"
#include "snapcast.h"
#ifdef CONFIG_WEB_PORT
#include "ui_http_server.h"
#endif

namespace esphome {
namespace snapclient {

void SnapClientComponent::setup() {
  TaskHandle_t t_http_get_task = NULL;
  if (!this->parent_->try_lock()) {
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "init player");
  i2s_std_gpio_config_t i2s_pin_config0 = this->parent_->get_pin_config();
  i2s_pin_config0.dout = (gpio_num_t) this->dout_pin_;

  this->audio_q_hdl_ = xQueueCreate(1, sizeof(audioDACdata_t));

#ifdef USE_AUDIO_DAC
  if (this->audio_dac_) {
    this->audio_dac_->set_mute_on();
  }
#endif
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->setup();
    this->mute_pin_->digital_write(true);
  }

  init_snapcast(this->audio_q_hdl_, this->name_.c_str(), this->host_.c_str(), this->port_);
  init_player(i2s_pin_config0, this->parent_->get_port());

  // http server for control operations and user interface
  // pass "WIFI_STA_DEF", "WIFI_AP_DEF", "ETH_DEF"
#ifdef CONFIG_WEB_PORT
#ifdef USE_ETHERNET
  init_http_server_task("ETH_DEF");
#endif
#ifdef USE_WIFI
  init_http_server_task("WIFI_STA_DEF");  // AP mode not handled
#endif
#endif

#if CONFIG_USE_DSP_PROCESSOR
  dsp_processor_init();
#endif
  ESP_LOGD(TAG, "Create Task");
  xTaskCreatePinnedToCore(&http_get_task, "http", 4 * 1024, NULL, HTTP_TASK_PRIORITY, &t_http_get_task,
                          HTTP_TASK_CORE_ID);
}

void SnapClientComponent::loop() {
  audioDACdata_t dac_data;
  static audioDACdata_t dac_data_old = {
      .mute = true,
      .volume = 100,
  };
  if (xQueueReceive(this->audio_q_hdl_, &dac_data, 0) == pdTRUE) {
    if (dac_data.mute != dac_data_old.mute) {
      if (this->mute_pin_ != nullptr) {
        this->mute_pin_->digital_write(dac_data.mute);
      }
#ifdef USE_AUDIO_DAC
      if (this->audio_dac_) {
        if (dac_data.mute) {
          this->audio_dac_->set_mute_on();
        } else {
          this->audio_dac_->set_mute_off();
        }
      }
#endif
      this->mute_state_ = dac_data.mute;
      ESP_LOGD(TAG, "%s", dac_data.mute ? "Mute" : "Unmute");
    }
    if (dac_data.volume != dac_data_old.volume) {
      this->volume_ = (float) dac_data.volume / 100;
#ifdef USE_AUDIO_DAC
      if (this->audio_dac_ != nullptr) {
        this->audio_dac_->set_volume(this->volume_);
      }
#endif
    }
    dac_data_old = dac_data;
  }
}

}  // namespace snapclient
}  // namespace esphome

#endif
#endif
