#include "dfrobot_sen0609.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace esphome::dfrobot_sen0609 {

ESPHOME_LOG_TAG(TAG, "dfrobot_sen0609");

static constexpr uint8_t MAX_ATTEMPTS = 3;
// The module ignores a command that follows its last answer too closely
static constexpr uint32_t COMMAND_GAP_MS = 100;
static constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000;
static constexpr uint32_t LONG_TIMEOUT_MS = 2000;
static constexpr uint32_t GIVE_UP_BACKOFF_MS = 30000;
static constexpr uint8_t FRAME_FIELDS = 5;

static constexpr uint8_t LINE_END[] = {'\r', '\n'};

// Indexed by Param; empty for parameters that cannot be read
PROGMEM_STRING_TABLE(ReadCommands, "", "", "getSWV", "getHWV");

static_assert(PARAM_COUNT <= 16, "pending_writes_ and pending_reads_ hold one bit per parameter");
static constexpr uint16_t param_bit(Param param) { return static_cast<uint16_t>(1U << param); }

static bool starts_with(const char *str, ESPHOME_PGM_P prefix) {
  return ESPHOME_strncmp_P(str, prefix, ESPHOME_strlen_P(prefix)) == 0;
}

static bool contains(const char *str, ESPHOME_PGM_P needle) {
  const size_t len = ESPHOME_strlen_P(needle);
  for (; *str != '\0'; str++) {
    if (ESPHOME_strncmp_P(str, needle, len) == 0)
      return true;
  }
  return false;
}

// Splits "a,b,c*" in place; returns the number of fields stored in `fields`
static uint8_t split_fields(char *str, char **fields, uint8_t max_fields) {
  uint8_t count = 0;
  char *start = str;
  for (;; str++) {
    if (*str != ',' && *str != '*' && *str != '\0')
      continue;
    const bool last = *str != ',';
    *str = '\0';
    if (count < max_fields)
      fields[count++] = start;
    if (last)
      return count;
    start = str + 1;
  }
}

// Answers that refuse a command whose effect is already in place
static bool answer_means_done(Operation operation, const char *line) {
  switch (operation) {
    case Operation::OPERATION_STOP:
      return contains(line, ESPHOME_PSTR("stopped already"));
    case Operation::OPERATION_START:
      return contains(line, ESPHOME_PSTR("started already"));
    default:
      return false;
  }
}

static bool parse_float(const char *str, float &value) {
  char *end;
  value = strtof(str, &end);
  return end != str && *end == '\0';
}

static const LogString *operation_to_str(Operation operation) {
  switch (operation) {
    case Operation::OPERATION_STOP:
      return LOG_STR("stop");
    case Operation::OPERATION_START:
      return LOG_STR("start");
    case Operation::OPERATION_WRITE:
      return LOG_STR("write");
    case Operation::OPERATION_READ:
      return LOG_STR("read");
    default:
      return LOG_STR("none");
  }
}

DFRobotSEN0609Component::DFRobotSEN0609Component(RunMode mode) : mode_(mode) {}

void DFRobotSEN0609Component::dump_config() {
  ESP_LOGCONFIG(
      TAG,
      "DFRobot SEN0609:\n"
      "  Mode: %s",
      this->mode_ == RunMode::RUN_MODE_PRESENCE ? LOG_STR_LITERAL("presence") : LOG_STR_LITERAL("speed and distance"));
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "HasTarget", this->has_target_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "TargetDistance", this->target_distance_sensor_);
  LOG_SENSOR("  ", "TargetSpeed", this->target_speed_sensor_);
  LOG_SENSOR("  ", "TargetEnergy", this->target_energy_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Version", this->version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "HardwareVersion", this->hardware_version_text_sensor_);
#endif
}

void DFRobotSEN0609Component::setup() { this->queue_configuration_(); }

void DFRobotSEN0609Component::loop() {
  uint8_t buf[MAX_LINE_LENGTH];
  size_t avail = this->available();
  while (avail > 0) {
    const size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read))
      break;
    avail -= to_read;
    for (size_t i = 0; i < to_read; i++)
      this->readline_(buf[i]);
  }
  this->run_sequencer_();
}

void DFRobotSEN0609Component::readline_(uint8_t c) {
  if (c == '\r')
    return;
  if (c == '\n') {
    if (!this->line_overflow_) {
      this->line_[this->line_pos_] = '\0';
      this->handle_line_(this->line_);
    }
    this->line_overflow_ = false;
    this->line_pos_ = 0;
    return;
  }
  if (this->line_overflow_)
    return;
  if (this->line_pos_ >= MAX_LINE_LENGTH - 1) {
    ESP_LOGW(TAG, "Line too long, dropped");
    this->line_overflow_ = true;
    return;
  }
  this->line_[this->line_pos_++] = static_cast<char>(c);
  // The prompt has no line end, and the echo of the next command follows it on the same line
  if (c == '>') {
    this->line_[this->line_pos_] = '\0';
    if (ESPHOME_strcmp_P(this->line_, ESPHOME_PSTR("DFRobot:/>")) == 0)
      this->line_pos_ = 0;
  }
}

void DFRobotSEN0609Component::handle_line_(char *line) {
  if (*line == '\0')
    return;
  ESP_LOGV(TAG, "RX: %s", line);
  if (line[0] == '$') {
    this->handle_frame_(line);
    return;
  }
  if (this->operation_ == Operation::OPERATION_NONE)
    return;
  if (strcmp(line, this->command_) == 0) {
    this->echo_seen_ = true;
    return;
  }
  // The module echoes every command first; lines before the echo belong to an earlier command
  if (!this->echo_seen_)
    return;

  if (starts_with(line, ESPHOME_PSTR("Done")) || answer_means_done(this->operation_, line)) {
    this->finish_operation_();
  } else if (starts_with(line, ESPHOME_PSTR("SoftwareVersion:"))) {
#ifdef USE_TEXT_SENSOR
    if (this->version_text_sensor_ != nullptr)
      this->version_text_sensor_->publish_state(line + 16);
#endif
  } else if (starts_with(line, ESPHOME_PSTR("HardwareVersion:"))) {
#ifdef USE_TEXT_SENSOR
    if (this->hardware_version_text_sensor_ != nullptr)
      this->hardware_version_text_sensor_->publish_state(line + 16);
#endif
  } else if (contains(line, ESPHOME_PSTR("not stopped"))) {
    // Stop the sensor before the next attempt
    this->stopped_ = false;
    this->fail_operation_();
  } else if (starts_with(line, ESPHOME_PSTR("Error")) || contains(line, ESPHOME_PSTR("not recognized"))) {
    ESP_LOGW(TAG, "Command '%s' rejected: %s", this->command_, line);
    this->fail_operation_();
  }
}

void DFRobotSEN0609Component::handle_frame_(char *line) {
  // $DFHPD,<presence>, , , *
  if (starts_with(line, ESPHOME_PSTR("$DFHPD,"))) {
    const char presence = line[7];
    if (presence != '0' && presence != '1') {
      ESP_LOGW(TAG, "Malformed presence frame: %s", line);
      return;
    }
#ifdef USE_BINARY_SENSOR
    if (this->has_target_binary_sensor_ != nullptr)
      this->has_target_binary_sensor_->publish_state(presence == '1');
#endif
    return;
  }
  // $DFDMD,<targets>,<reserved>,<distance m>,<speed m/s>,<energy>,...*
  if (!starts_with(line, ESPHOME_PSTR("$DFDMD,")))
    return;
  char *fields[FRAME_FIELDS];
  float distance;
  float speed;
  float energy;
  if (split_fields(line + 7, fields, FRAME_FIELDS) < FRAME_FIELDS) {
    ESP_LOGW(TAG, "Malformed target frame");
    return;
  }
  const optional<uint8_t> targets = parse_number<uint8_t>(fields[0]);
  if (!targets.has_value()) {
    ESP_LOGW(TAG, "Malformed target frame");
    return;
  }
  if (*targets == 0) {
#ifdef USE_BINARY_SENSOR
    if (this->has_target_binary_sensor_ != nullptr)
      this->has_target_binary_sensor_->publish_state(false);
#endif
#ifdef USE_SENSOR
    this->publish_target_(NAN, NAN, NAN);
#endif
    return;
  }
  if (!parse_float(fields[2], distance) || !parse_float(fields[3], speed) || !parse_float(fields[4], energy)) {
    ESP_LOGW(TAG, "Malformed target frame");
    return;
  }
#ifdef USE_BINARY_SENSOR
  if (this->has_target_binary_sensor_ != nullptr)
    this->has_target_binary_sensor_->publish_state(true);
#endif
#ifdef USE_SENSOR
  this->publish_target_(distance, speed, energy);
#endif
}

#ifdef USE_SENSOR
static void publish_deduplicated(sensor::Sensor *sens, Deduplicator<float> &dedup, float value) {
  if (sens == nullptr)
    return;
  if (std::isnan(value) ? dedup.next_unknown() : dedup.next(value))
    sens->publish_state(value);
}

void DFRobotSEN0609Component::publish_target_(float distance, float speed, float energy) {
  publish_deduplicated(this->target_distance_sensor_, this->distance_dedup_, distance);
  publish_deduplicated(this->target_speed_sensor_, this->speed_dedup_, speed);
  publish_deduplicated(this->target_energy_sensor_, this->energy_dedup_, energy);
}
#endif

void DFRobotSEN0609Component::queue_configuration_() {
  // The mode and the UART output are applied at every boot instead of being saved
  uint16_t writes = param_bit(PARAM_RUN_APP);
  if (this->mode_ == RunMode::RUN_MODE_PRESENCE)
    writes |= param_bit(PARAM_UART_OUTPUT);
  this->pending_writes_ |= writes;
  this->pending_reads_ |= this->wanted_reads_();
}

uint16_t DFRobotSEN0609Component::wanted_reads_() const {
  uint16_t reads = 0;
#ifdef USE_TEXT_SENSOR
  if (this->version_text_sensor_ != nullptr)
    reads |= param_bit(PARAM_SW_VERSION);
  if (this->hardware_version_text_sensor_ != nullptr)
    reads |= param_bit(PARAM_HW_VERSION);
#endif
  return reads;
}

void DFRobotSEN0609Component::hold_(uint32_t duration_ms) {
  this->hold_start_ms_ = App.get_loop_component_start_time();
  this->hold_ms_ = duration_ms;
}

void DFRobotSEN0609Component::run_sequencer_() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->operation_ != Operation::OPERATION_NONE) {
    const bool slow = this->operation_ == Operation::OPERATION_STOP;
    if (now - this->sent_ms_ < (slow ? LONG_TIMEOUT_MS : DEFAULT_TIMEOUT_MS))
      return;
    ESP_LOGD(TAG, "No answer to '%s'", this->command_);
    this->fail_operation_();
    return;
  }
  if (now - this->hold_start_ms_ < this->hold_ms_)
    return;

  const bool work_pending = this->pending_writes_ != 0 || this->pending_reads_ != 0;
  Operation operation;
  Param param = PARAM_COUNT;
  if (!work_pending) {
    if (!this->stopped_)
      return;
    operation = Operation::OPERATION_START;
  } else if (!this->stopped_) {
    operation = Operation::OPERATION_STOP;
  } else if (this->pending_writes_ != 0) {
    operation = Operation::OPERATION_WRITE;
    param = static_cast<Param>(__builtin_ctz(this->pending_writes_));
  } else {
    operation = Operation::OPERATION_READ;
    param = static_cast<Param>(__builtin_ctz(this->pending_reads_));
  }

  this->build_command_(operation, param);
  if (operation == Operation::OPERATION_WRITE)
    this->pending_writes_ &= ~param_bit(param);
  this->operation_ = operation;
  this->param_ = param;
  this->echo_seen_ = false;
  this->sent_ms_ = now;
  ESP_LOGV(TAG, "TX: %s", this->command_);
  this->write_str(this->command_);
  this->write_array(LINE_END, sizeof(LINE_END));
}

void DFRobotSEN0609Component::build_command_(Operation operation, Param param) {
  char *buf = this->command_;
  constexpr size_t len = MAX_COMMAND_LENGTH;
  switch (operation) {
    case Operation::OPERATION_STOP:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("sensorStop"), len);
      return;
    case Operation::OPERATION_START:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("sensorStart"), len);
      return;
    case Operation::OPERATION_READ:
      ESPHOME_strncpy_P(buf, reinterpret_cast<ESPHOME_PGM_P>(ReadCommands::get_progmem_str(param, 0)), len);
      return;
    default:
      break;
  }

  switch (param) {
    case PARAM_RUN_APP:
      ESPHOME_snprintf_P(buf, len, ESPHOME_PSTR("setRunApp %u"), static_cast<unsigned>(this->mode_));
      return;
    case PARAM_UART_OUTPUT:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("setUartOutput 1 1 1 1.0"), len);
      return;
    default:
      break;
  }
}

void DFRobotSEN0609Component::finish_operation_() {
  switch (this->operation_) {
    case Operation::OPERATION_STOP:
      this->stopped_ = true;
      break;
    case Operation::OPERATION_START:
      this->stopped_ = false;
      if (!this->sequence_failed_)
        this->status_clear_warning();
      this->sequence_failed_ = false;
      break;
    case Operation::OPERATION_WRITE:
      break;
    case Operation::OPERATION_READ:
      // The versions are published as their lines arrive
      this->pending_reads_ &= ~param_bit(this->param_);
      break;
    default:
      return;
  }
  if (this->is_aux_operation_()) {
    this->aux_attempts_ = 0;
  } else {
    this->attempts_ = 0;
  }
  this->operation_ = Operation::OPERATION_NONE;
  this->hold_(COMMAND_GAP_MS);
}

void DFRobotSEN0609Component::fail_operation_() {
  const bool aux = this->is_aux_operation_();
  uint8_t &attempts = aux ? this->aux_attempts_ : this->attempts_;
  const Operation operation = this->operation_;
  this->operation_ = Operation::OPERATION_NONE;
  this->hold_(COMMAND_GAP_MS);
  if (++attempts < MAX_ATTEMPTS) {
    if (operation == Operation::OPERATION_WRITE)
      this->pending_writes_ |= param_bit(this->param_);
    return;
  }

  attempts = 0;
  this->status_set_warning();
  if (aux) {
    // Without stop or start nothing else can run; keep the work and try again later
    ESP_LOGW(TAG, "Module not answering to %s, trying again later", LOG_STR_ARG(operation_to_str(operation)));
    this->hold_ms_ = GIVE_UP_BACKOFF_MS;
    return;
  }
  ESP_LOGW(TAG, "Giving up on %s '%s'", LOG_STR_ARG(operation_to_str(operation)), this->command_);
  this->sequence_failed_ = true;
  if (operation == Operation::OPERATION_READ)
    this->pending_reads_ &= ~param_bit(this->param_);
}

}  // namespace esphome::dfrobot_sen0609
