#pragma once
#ifdef USE_ESP32

#include "esphome/core/defines.h"

#ifdef USE_MODEM
#ifdef USE_SENSOR

#include "esphome/core/component.h"
#include "esphome/components/modem/modem_component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace modem {

class ModemSensor : public PollingComponent {
 public:
  void set_rssi_sensor(sensor::Sensor *rssi_sensor) { this->rssi_sensor_ = rssi_sensor; }
  void set_ber_sensor(sensor::Sensor *ber_sensor) { this->ber_sensor_ = ber_sensor; }

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)

  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }
  void setup() override;
  void update() override;
  void dump_config() override {}

 protected:
  sensor::Sensor *rssi_sensor_{nullptr};
  sensor::Sensor *ber_sensor_{nullptr};
  void update_signal_sensors_();
};

}  // namespace modem
}  // namespace esphome

#endif  // USE_MODEM
#endif  // USE_SENSOR
#endif  // USE_ESP32
