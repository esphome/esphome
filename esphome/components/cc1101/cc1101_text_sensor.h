#pragma once

#include "esphome/core/defines.h"

#ifdef USE_TEXT_SENSOR

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"
#include "cc1101.h"
#include <string>
#include <cstdio>

namespace esphome::cc1101 {

class CC1101TextSensor : public text_sensor::TextSensor,
                         public PollingComponent,
                         public Parented<CC1101Component>,
                         public CC1101ConfigListener {
 public:
  enum CC1101TextSensorType {
    RX_ATTENUATION,
    MODULATION_TYPE,
    FREQUENCY,
    CHIP_ID,
  };

  void set_type(CC1101TextSensorType type) { type_ = type; }

  void setup() override {
    if (this->parent_ != nullptr) {
      this->parent_->register_config_listener(this);
    }
  }

  void on_config_change() override { this->update(); }

  struct Option {
    const char *name;
    int value;
  };

  static constexpr Option RX_ATTENUATION_OPTIONS[4] = {{"0dB", 0}, {"6dB", 1}, {"12dB", 2}, {"18dB", 3}};

  static constexpr Option MODULATION_OPTIONS[5] = {{"2-FSK", 0}, {"GFSK", 1}, {"ASK/OOK", 3}, {"4-FSK", 4}, {"MSK", 7}};

  void update() override {
    if (this->parent_ == nullptr)
      return;
    std::string value;
    switch (this->type_) {
      case RX_ATTENUATION:
        value = get_option_name(static_cast<int>(this->parent_->get_rx_attenuation()), RX_ATTENUATION_OPTIONS, 4);
        break;
      case MODULATION_TYPE:
        value = get_option_name(static_cast<int>(this->parent_->get_modulation_type()), MODULATION_OPTIONS, 5);
        break;
      case FREQUENCY:
        char buffer[32];
        sprintf(buffer, "%.2f MHz", this->parent_->get_frequency() / 1000000.0f);
        value = buffer;
        break;
      case CHIP_ID:
        char id_buffer[8];
        sprintf(id_buffer, "0x%04X", this->parent_->get_chip_id());
        value = id_buffer;
        break;
    }
    this->publish_state(value);
  }

 protected:
  CC1101TextSensorType type_;

  std::string get_option_name(int val, const Option *opts, size_t size) {
    for (size_t i = 0; i < size; i++) {
      if (opts[i].value == val)
        return opts[i].name;
    }
    return "";
  }
};

}  // namespace esphome::cc1101

#endif
