#pragma once
#ifdef USE_ESP_IDF

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_TEXT_SENSOR

#include "esphome/core/component.h"
#include "esphome/components/modem/modem_component.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace modem {

class ModemTextSensor : public PollingComponent {
 public:
  void set_network_type_text_sensor(text_sensor::TextSensor *network_type_text_sensor) {
    this->network_type_text_sensor_ = network_type_text_sensor;
  }
  void set_signal_strength_text_sensor(text_sensor::TextSensor *signal_strength_text_sensor) {
    this->signal_strength_text_sensor_ = signal_strength_text_sensor;
  }
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void setup() override;
  void update() override;
  void dump_config() override {}

 protected:
  text_sensor::TextSensor *network_type_text_sensor_{nullptr};
  text_sensor::TextSensor *signal_strength_text_sensor_{nullptr};
  void update_network_type_text_sensor_();
  void update_signal_strength_text_sensor_();
};

}  // namespace modem
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_TEXT_SENSOR
#endif  // USE_ESP_IDF
