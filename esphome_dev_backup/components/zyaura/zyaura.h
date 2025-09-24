#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace zyaura {

static const uint8_t ZA_MAX_MS = 2;
static const uint8_t ZA_MSG_LEN = 5;
static const uint8_t ZA_FRAME_SIZE = 40;
static const uint8_t ZA_MSG_DELIMETER = 0x0D;

static const uint8_t ZA_BYTE_TYPE = 0;
static const uint8_t ZA_BYTE_HIGH = 1;
static const uint8_t ZA_BYTE_LOW = 2;
static const uint8_t ZA_BYTE_SUM = 3;
static const uint8_t ZA_BYTE_END = 4;

enum ZaDataType {
  HUMIDITY = 0x41,
  TEMPERATURE = 0x42,
  CO2 = 0x50,
};

struct ZaMessage {
  ZaDataType type;
  uint16_t value;
};

class ZaDataProcessor {
 public:
  bool decode(uint32_t ms, bool data);
  ZaMessage *message = new ZaMessage;

 protected:
  uint8_t buffer_[ZA_MSG_LEN];
  int num_bits_ = 0;
  uint32_t prev_ms_;
};

class ZaSensorStore {
 public:
  uint16_t co2 = -1;
  uint16_t temperature = -1;
  uint16_t humidity = -1;

  void setup(InternalGPIOPin *pin_clock, InternalGPIOPin *pin_data);
  static void interrupt(ZaSensorStore *arg);

 protected:
  ISRInternalGPIOPin pin_clock_;
  ISRInternalGPIOPin pin_data_;
  ZaDataProcessor processor_;

  void set_data_(ZaMessage *message);
};

/// Component for reading temperature/co2/humidity measurements from ZyAura sensors.
class ZyAuraSensor : public PollingComponent {
 public:
  void set_pin_clock(InternalGPIOPin *pin) { pin_clock_ = pin; }
  void set_pin_data(InternalGPIOPin *pin) { pin_data_ = pin; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }

  void setup() override { this->store_.setup(this->pin_clock_, this->pin_data_); }
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  ZaSensorStore store_;
  InternalGPIOPin *pin_clock_;
  InternalGPIOPin *pin_data_;
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

  bool publish_state_(ZaDataType data_type, sensor::Sensor *sensor, uint16_t *data_value);
};

}  // namespace zyaura
}  // namespace esphome
