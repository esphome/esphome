#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::dht {

enum DHTModel : uint8_t {
  DHT_MODEL_AUTO_DETECT = 0,
  DHT_MODEL_DHT11,
  DHT_MODEL_DHT22,
  DHT_MODEL_AM2120,
  DHT_MODEL_AM2302,
  DHT_MODEL_RHT03,
  DHT_MODEL_SI7021,
  DHT_MODEL_DHT22_TYPE2
};

/// Component for reading temperature/humidity measurements from DHT11/DHT22 sensors.
class DHT final : public PollingComponent {
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 public:
  /** Manually select the DHT model.
   *
   * Valid values are:
   *
   *  - DHT_MODEL_AUTO_DETECT (default)
   *  - DHT_MODEL_DHT11
   *  - DHT_MODEL_DHT22
   *  - DHT_MODEL_AM2120
   *  - DHT_MODEL_AM2302
   *  - DHT_MODEL_RHT03
   *  - DHT_MODEL_SI7021
   *  - DHT_MODEL_DHT22_TYPE2
   *
   * @param model The DHT model.
   */
  void set_dht_model(DHTModel model);

  void set_pin(InternalGPIOPin *pin) {
    this->t_pin_ = pin;
    this->pin_ = pin->to_isr();
  }

  /// Set up the pins and check connection.
  void setup() override;
  void dump_config() override;
  /// Update sensor values and push them to the frontend.
  void update() override;

 protected:
  bool read_sensor_(float *temperature, float *humidity, bool report_errors);

  InternalGPIOPin *t_pin_;
  ISRInternalGPIOPin pin_;
  DHTModel model_{DHT_MODEL_AUTO_DETECT};
  bool is_auto_detect_{false};
};

}  // namespace esphome::dht
