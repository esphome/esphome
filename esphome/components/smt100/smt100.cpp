#include "smt100.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::smt100 {

ESPHOME_LOG_TAG(TAG, "smt100");

// The sensor measures for about 400 ms before it answers
static constexpr uint32_t RESPONSE_TIMEOUT_MS = 1000;
// Longest text of one value, "-20.000", plus room to spare and the NUL
static constexpr size_t MAX_FIELD_SIZE = 12;

bool parse_measurements(const char *line, float *values) {
  for (uint8_t field = 0; field < FIELD_COUNT; field++) {
    // Every value but the last one ends with a comma; a comma left in the last one fails parse_number()
    const char *end = field < FIELD_COUNT - 1 ? strchr(line, ',') : line + strlen(line);
    if (end == nullptr)
      return false;
    const size_t len = end - line;
    if (len >= MAX_FIELD_SIZE)
      return false;
    char text[MAX_FIELD_SIZE];
    memcpy(text, line, len);
    text[len] = '\0';
    const optional<float> value = parse_number<float>(text);
    // parse_number() also accepts "inf" and "nan"
    if (!value.has_value() || !std::isfinite(*value))
      return false;
    values[field] = *value;
    line = end + 1;
  }
  return true;
}

void SMT100Component::setup() {
  // loop() only runs while a request is due or its answer is expected
  this->disable_loop();
  // An update() that ran before this, from a lambda or a log automation, could not enable the loop yet
  if (this->state_ != State::STATE_IDLE)
    this->enable_loop();
}

void SMT100Component::update() {
  if (this->state_ != State::STATE_IDLE) {
    ESP_LOGV(TAG, "Previous request not finished, skipping update");
    return;
  }
  // loop() sends the request, so that its timeout runs on the loop clock
  this->state_ = State::STATE_REQUEST_DUE;
  this->enable_loop();
}

void SMT100Component::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->state_ == State::STATE_REQUEST_DUE) {
    this->send_request_(now);
    return;
  }

  const char *line = nullptr;
  this->read_available([this, &line](uint8_t c) {
    // A line that is low while the sensor powers up reads as NUL bytes
    if (c == 0)
      return true;
    line = this->line_reader_.feed(c);
    this->line_reader_.log_if_overflowed(TAG);
    if (line != nullptr && line[0] == '\0')
      line = nullptr;
    return line == nullptr;
  });

  if (line != nullptr || now - this->request_sent_ms_ >= RESPONSE_TIMEOUT_MS)
    this->finish_request_(line);
}

void SMT100Component::send_request_(uint32_t now) {
  // Drop anything left over, such as a late answer or noise from the sensor being plugged in
  this->discard_available();
  this->line_reader_.reset();
  ESP_LOGV(TAG, "Sending measurement request");
  this->write_str("GetAllMeasurements!\r");
  this->request_sent_ms_ = now;
  this->state_ = State::STATE_AWAITING_ANSWER;
}

void SMT100Component::finish_request_(const char *answer) {
  // The loop goes off first: disable_loop() can log, and an update() from a log automation that already saw
  // the idle state would queue a request with no loop to send it
  this->disable_loop();
  this->state_ = State::STATE_IDLE;

  float values[FIELD_COUNT];
  const bool valid = answer != nullptr && parse_measurements(answer, values);
  if (!valid) {
    if (answer != nullptr) {
      ESP_LOGW(TAG, "Unexpected answer '%s'", answer);
    } else {
      ESP_LOGW(TAG, "No answer from the sensor");
    }
    // A frozen reading would hide a sensor that stopped answering, so the entities go unknown
    std::fill_n(values, static_cast<size_t>(FIELD_COUNT), NAN);
  }

  PUBLISH_STATE(this->counts_sensor_, values[FIELD_COUNTS]);
  PUBLISH_STATE(this->permittivity_sensor_, values[FIELD_PERMITTIVITY]);
  PUBLISH_STATE(this->moisture_sensor_, values[FIELD_MOISTURE]);
  PUBLISH_STATE(this->temperature_sensor_, values[FIELD_TEMPERATURE]);
  PUBLISH_STATE(this->voltage_sensor_, values[FIELD_VOLTAGE]);
  this->status_comm_result(valid);
}

void SMT100Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SMT100:");
  LOG_SENSOR("  ", "Counts", this->counts_sensor_);
  LOG_SENSOR("  ", "Permittivity", this->permittivity_sensor_);
  LOG_SENSOR("  ", "Moisture", this->moisture_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::smt100
