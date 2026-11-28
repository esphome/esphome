#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/line_reader.h"

namespace esphome::smt100 {

/// Order of the values in the answer to GetAllMeasurements.
enum Field : uint8_t {
  FIELD_COUNTS,
  FIELD_PERMITTIVITY,
  FIELD_MOISTURE,
  FIELD_TEMPERATURE,
  FIELD_VOLTAGE,
  FIELD_COUNT,
};

/// Read "counts,permittivity,moisture,temperature,voltage" into values[FIELD_COUNT].
/// Returns false, with values unspecified, unless the line holds exactly FIELD_COUNT finite numbers.
bool parse_measurements(const char *line, float *values);

class SMT100Component final : public PollingComponent, public uart::UARTDevice {
  // The longest answer, "65535,100.00,100.00,-20.000,15.00", is 33 characters
  static constexpr uint16_t LINE_BUFFER_SIZE = 48;

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  void update() override;

  SUB_SENSOR(counts)
  SUB_SENSOR(permittivity)
  SUB_SENSOR(temperature)
  SUB_SENSOR(moisture)
  SUB_SENSOR(voltage)

 protected:
  enum class State : uint8_t {
    STATE_IDLE,
    STATE_REQUEST_DUE,
    STATE_AWAITING_ANSWER,
  };

  void send_request_(uint32_t now);
  /// End the request; answer is the line the sensor sent, or nullptr when it sent none in time.
  void finish_request_(const char *answer);

  uint32_t request_sent_ms_{0};
  LineReader<LINE_BUFFER_SIZE> line_reader_;
  State state_{State::STATE_IDLE};
};

}  // namespace esphome::smt100
