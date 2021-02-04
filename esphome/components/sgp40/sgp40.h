#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
extern "C" {
#include "sensirion_arch_config.h"
#include "sensirion_voc_algorithm.h"
};
#include <cmath>

namespace esphome {
namespace sgp40 {

// the i2c address
static const uint8_t SGP40_I2CADDR_DEFAULT = 0x59;

// commands and constants
static const uint8_t SGP40_FEATURESET = 0x0020;     ///< The required set for this library
static const uint8_t SGP40_CRC8_POLYNOMIAL = 0x31;  ///< Seed for SGP40's CRC polynomial
static const uint8_t SGP40_CRC8_INIT = 0xFF;        ///< Init value for CRC
static const uint8_t SGP40_WORD_LEN = 2;            ///< 2 bytes per word

// Commands

static const uint16_t SGP40_CMD_GET_SERIAL_ID = 0x3682;
static const uint16_t SGP40_CMD_GET_FEATURESET = 0x202f;
static const uint16_t SGP40_CMD_SELF_TEST = 0x280e;

class SGP40Component;

/// This class implements support for the Sensirion sgp40 i2c GAS (VOC) sensors.
class SGP40Component : public PollingComponent, public sensor::Sensor, public i2c::I2CDevice {
 public:
  void set_humidity_sensor(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_temperature_sensor(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  /// Input sensor for humidity and temperature compensation.
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  bool write_command_(uint16_t command);
  bool read_data_(uint16_t *data, uint8_t len);
  int16_t sensirion_init_sensors_();
  int16_t sgp40_probe_();
  uint8_t sht_crc_(uint8_t data1, uint8_t data2);
  uint64_t serial_number_;
  uint16_t featureset_;
  int32_t measureVocIndex_();
  uint8_t generateCRC(uint8_t *data, uint8_t datalen);
  uint16_t measure_raw_();
  VocAlgorithmParams voc_algorithm_params_;

  /**
   * @brief Request the sensor to perform a self-test, returning the result
   *
   * @return true: success false:failure
   */
  bool self_test_();
  enum ErrorCode {
    COMMUNICATION_FAILED,
    MEASUREMENT_INIT_FAILED,
    INVALID_ID,
    UNSUPPORTED_ID,
    UNKNOWN
  } error_code_{UNKNOWN};
};
}  // namespace sgp40
}  // namespace esphome
