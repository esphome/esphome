#pragma once

#include "esphome/core/component.h"
#include "esphome/core/esphal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace zyaura {

#define ZA_MAX_MS         2
#define ZA_MSG_LEN        5
#define ZA_FRAME_SIZE     40
#define ZA_MSG_DELIMETER  0x0D

#define ZA_BYTE_TYPE      0
#define ZA_BYTE_HIGH      1
#define ZA_BYTE_LOW       2
#define ZA_BYTE_SUM       3
#define ZA_BYTE_END       4

typedef enum {
  HUMIDITY = 0x41,
  TEMPERATURE = 0x42,
  CO2 = 0x50,
} ZaDataType;

typedef struct {
  ZaDataType type;
  uint16_t value;
  bool checksumIsValid;
} ZaMessage;

class ZaDataProcessor {
 public:
  ZaMessage* process(unsigned long ms, bool data);

 protected:
  uint8_t buffer_[ZA_MSG_LEN];
  int num_bits_ = 0;
  unsigned long prev_ms_;
  ZaMessage *msg_ = &_msg;

  void decode_();

 private:
  ZaMessage _msg;
};

class ZaSensorStore {
 public:
  float co2 = NAN;
  float temperature = NAN;
  float humidity = NAN;
  bool isValid = false;

  void setup(GPIOPin *pin_clock, GPIOPin *pin_data);
  static void interrupt(ZaSensorStore *arg);

 protected:
  ISRInternalGPIOPin *pin_clock_;
  ISRInternalGPIOPin *pin_data_;
  ZaDataProcessor processor_;

  void set_data_(ZaMessage *message);
};

/// Component for reading temperature/co2/humidity measurements from ZyAura sensors.
class ZyAuraSensor : public PollingComponent {
 public:
  void set_pin_clock(GPIOPin *pin) { pin_clock_ = pin; }
  void set_pin_data(GPIOPin *pin) { pin_data_ = pin; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  void setup() override { this->store_.setup(this->pin_clock_, this->pin_data_); }
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  ZaSensorStore store_;
  GPIOPin *pin_clock_;
  GPIOPin *pin_data_;
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  bool publish_state_(sensor::Sensor *sensor, float value);
};

}  // namespace zyaura
}  // namespace esphome
