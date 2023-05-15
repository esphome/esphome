#pragma once

#ifdef USE_ESP32

#include <driver/i2s.h>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace i2s_audio {

class I2SAudioComponent;

class I2SAudioIn : public Parented<I2SAudioComponent> {};

class I2SAudioOut : public Parented<I2SAudioComponent> {};

class I2SAudioComponent : public Component {
 public:
  void setup() override;

  i2s_pin_config_t get_pin_config() const {
    return {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = this->bclk_pin_,
        .ws_io_num = this->lrclk_pin_,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };
  }

  void set_bclk_pin(int pin) { this->bclk_pin_ = pin; }
  void set_lrclk_pin(int pin) { this->lrclk_pin_ = pin; }

  void lock() { this->lock_.lock(); }
  bool try_lock() { return this->lock_.try_lock(); }
  void unlock() { this->lock_.unlock(); }

  i2s_port_t get_port() const { return this->port_; }

 protected:
  Mutex lock_;

  I2SAudioIn *audio_in_{nullptr};
  I2SAudioOut *audio_out_{nullptr};

  int bclk_pin_{I2S_PIN_NO_CHANGE};
  int lrclk_pin_;
  i2s_port_t port_{};
};

}  // namespace i2s_audio
}  // namespace esphome

#endif  // USE_ESP32
