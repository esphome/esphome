#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

namespace esphome::dfrobot_sen0465_base {

static constexpr uint8_t FRAME_SIZE = 9;

// Scheduler ids, shared with the bus variants
static constexpr uint32_t STEP_TIMEOUT_ID = 0;
static constexpr uint32_t RESPONSE_TIMEOUT_ID = 1;

enum class AlarmType : uint8_t {
  ALARM_TYPE_OFF,
  ALARM_TYPE_LOW,
  ALARM_TYPE_HIGH,
};

/// Settings sent once after boot, before the first reading
enum class SetupStep : uint8_t {
  SETUP_STEP_ACQUIRE_MODE,
  SETUP_STEP_ALARM,
  SETUP_STEP_DONE,
};

/// DFRobot "Factory Calibrated Electrochemical Gas Sensor" series (SEN0465 to SEN0476).
/// The bus variants only send request frames and deliver the answers.
class DFRobotSEN0465Component : public PollingComponent {
  SUB_SENSOR(concentration)
  SUB_SENSOR(temperature)

 public:
  /// @param gas_type gas type code the configured probe reports
  /// @param gas_name name of that gas, used in log messages
  DFRobotSEN0465Component(uint8_t gas_type, const char *gas_name) : gas_name_(gas_name), gas_type_(gas_type) {}

  void setup() override;
  void update() override;
  void dump_config() override;

  /// @param threshold alarm threshold as sent to the sensor, in units of the last decimal place
  /// @param decimals decimal places of the threshold, used for logging
  void set_alarm(AlarmType type, uint16_t threshold, uint8_t decimals) {
    this->alarm_type_ = type;
    this->alarm_threshold_ = threshold;
    this->alarm_decimals_ = decimals;
  }

#ifdef USE_DFROBOT_SEN0465_TEMPERATURE_COMPENSATION
  /// For low < T <= high: compensated = raw / (a * T + b) - (c * T + d)
  void add_compensation_range(int8_t low, int8_t high, float a, float b, float c, float d) {
    this->compensation_.push_back({low, high, a, b, c, d});
  }
#endif

 protected:
  /// Sends one complete request frame.
  virtual bool write_frame(const uint8_t *frame) = 0;
  /// Waits for the answer to a request, then calls handle_response_() or handle_no_response_().
  virtual void read_response() = 0;

  void handle_response_(const uint8_t *frame);
  void handle_no_response_();

  /// Sends the request of the current step and waits for its answer.
  bool send_step_request_();
  /// Sends the request again from a scheduled callback; a failed write counts as a failed attempt.
  void retry_step_request_();
  void step_failed_(const LogString *reason);
  void next_step_();
  void handle_data_(const uint8_t *frame);
  bool send_command_(uint8_t command, uint8_t arg0 = 0, uint8_t arg1 = 0, uint8_t arg2 = 0, uint8_t arg3 = 0);

  const char *gas_name_;
  uint16_t alarm_threshold_{0};
  uint8_t gas_type_;
  AlarmType alarm_type_{AlarmType::ALARM_TYPE_OFF};
  uint8_t alarm_decimals_{0};
  SetupStep step_{SetupStep::SETUP_STEP_ACQUIRE_MODE};
  uint8_t attempts_{0};

#ifdef USE_DFROBOT_SEN0465_TEMPERATURE_COMPENSATION
  struct CompensationRange {
    int8_t low;
    int8_t high;
    float a;
    float b;
    float c;
    float d;
  };

  /// Returns NAN when the temperature is outside every range
  float compensate_(float concentration, float temperature) const;

  StaticVector<CompensationRange, 3> compensation_;
#endif
};

}  // namespace esphome::dfrobot_sen0465_base
