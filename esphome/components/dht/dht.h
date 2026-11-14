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

/// Convert a DHT22-family temperature reading to tenths of a degree. Sensors encode negative values in one of three
/// ways: sign and magnitude (bit 15 set, bit 14 clear), 16-bit two's complement, or 12-bit two's complement (bit 11).
inline int16_t decode_dht22_temperature(uint16_t raw) {
  if (raw & 0x8000) {
    if (!(raw & 0x4000))
      return static_cast<int16_t>(-static_cast<int16_t>(raw & 0x7FFF));
  } else if (raw & 0x800) {
    raw |= 0xF000;
  }
  return static_cast<int16_t>(raw);
}

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
