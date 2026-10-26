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
// Gathers entity changes made close together into one flash write
static constexpr uint32_t COALESCE_MS = 1000;
static constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000;
static constexpr uint32_t LONG_TIMEOUT_MS = 2000;
static constexpr uint32_t FACTORY_RESET_TIMEOUT_MS = 3000;
// resetSystem has no answer; this is how long the module takes to boot again
static constexpr uint32_t RESTART_MS = 1500;
static constexpr uint32_t GIVE_UP_BACKOFF_MS = 30000;
// Presence frames come every second (setUartOutput period); this long without one means the module lost them
static constexpr uint32_t FRAME_TIMEOUT_MS = 10000;
static constexpr uint8_t FRAME_FIELDS = 5;

static constexpr uint8_t LINE_END[] = {'\r', '\n'};

// Indexed by Param; empty for parameters that cannot be read
PROGMEM_STRING_TABLE(ReadCommands, "", "", "getRange", "getTrigRange", "getSensitivity", "getLatency", "getInhibit",
                     "getThrFactor", "getMicroMotion", "getGpioMode 1", "", "", "getSWV", "getHWV");

static_assert(PARAM_COUNT <= 16, "pending_writes_ and pending_reads_ hold one bit per parameter");
static constexpr uint16_t param_bit(Param param) { return static_cast<uint16_t>(1U << param); }

// Parameters kept in the module's flash only after saveConfig
static constexpr uint16_t PERSISTENT_PARAMS =
    param_bit(PARAM_RANGE) | param_bit(PARAM_TRIG_RANGE) | param_bit(PARAM_SENSITIVITY) | param_bit(PARAM_LATENCY) |
    param_bit(PARAM_INHIBIT) | param_bit(PARAM_THR_FACTOR) | param_bit(PARAM_MICRO_MOTION) | param_bit(PARAM_GPIO_MODE);
// The frames depend on these, so their writes are kept until they succeed
static constexpr uint16_t FRAME_PARAMS = param_bit(PARAM_RUN_APP) | param_bit(PARAM_UART_OUTPUT);

#ifdef USE_SWITCH
static constexpr bool is_led(Param param) { return param == PARAM_RUN_LED || param == PARAM_OUT_LED; }
static constexpr SwitchSetting led_switch(Param param) {
  return param == PARAM_RUN_LED ? SWITCH_RUN_LED : SWITCH_OUT_LED;
}
#endif

#ifdef USE_NUMBER
static_assert(NUMBER_COUNT <= 16, "changed_numbers_ holds one bit per number");
static constexpr uint8_t NUMBER_PARAMS[NUMBER_COUNT] PROGMEM = {
    PARAM_RANGE,   PARAM_RANGE,   PARAM_TRIG_RANGE, PARAM_SENSITIVITY, PARAM_SENSITIVITY,
    PARAM_LATENCY, PARAM_LATENCY, PARAM_INHIBIT,    PARAM_THR_FACTOR,
};
#endif

static bool starts_with(const char *str, ESPHOME_PGM_P prefix) {
  return ESPHOME_strncmp_P(str, prefix, ESPHOME_strlen_P(prefix)) == 0;
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
      return ESPHOME_strstr_P(line, ESPHOME_PSTR("stopped already")) != nullptr;
    case Operation::OPERATION_START:
      return ESPHOME_strstr_P(line, ESPHOME_PSTR("started already")) != nullptr;
    case Operation::OPERATION_SAVE:
      return starts_with(line, ESPHOME_PSTR("no parameter"));
    default:
      return false;
  }
}

static const LogString *operation_to_str(Operation operation) {
  switch (operation) {
    case Operation::OPERATION_STOP:
      return LOG_STR("stop");
    case Operation::OPERATION_START:
      return LOG_STR("start");
    case Operation::OPERATION_RESTART:
      return LOG_STR("restart");
    case Operation::OPERATION_FACTORY_RESET:
      return LOG_STR("factory reset");
    case Operation::OPERATION_WRITE:
      return LOG_STR("write");
    case Operation::OPERATION_SAVE:
      return LOG_STR("save");
    case Operation::OPERATION_READ:
      return LOG_STR("read");
    default:
      return LOG_STR("none");
  }
}

DFRobotSEN0609Component::DFRobotSEN0609Component(RunMode mode) : mode_(mode) {
#ifdef USE_NUMBER
  this->number_values_.fill(NAN);
#endif
}

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
#ifdef USE_NUMBER
  for (number::Number *n : this->numbers_) {
    LOG_NUMBER("  ", "Number", n);
  }
#endif
#ifdef USE_SWITCH
  for (switch_::Switch *s : this->switches_) {
    LOG_SWITCH("  ", "Switch", s);
  }
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "OutPinLevel", this->out_pin_level_select_);
#endif
#ifdef USE_BUTTON
  LOG_BUTTON("  ", "FactoryReset", this->factory_reset_button_);
  LOG_BUTTON("  ", "Query", this->query_button_);
  LOG_BUTTON("  ", "Restart", this->restart_button_);
#endif
}

void DFRobotSEN0609Component::setup() {
#ifdef USE_SWITCH
  // The module forgets the LED states at power off
  for (uint8_t i = SWITCH_RUN_LED; i <= SWITCH_OUT_LED; i++) {
    switch_::Switch *sw = this->switches_[i];
    if (sw == nullptr)
      continue;
    // A disabled restore mode leaves the switch on, which an inverted switch writes as off
    this->publish_switch_(static_cast<SwitchSetting>(i), sw->get_initial_write_state().value_or(!sw->is_inverted()));
  }
#endif
  this->queue_configuration_();
}

void DFRobotSEN0609Component::loop() {
  uint8_t buf[MAX_LINE_LENGTH];
  size_t avail = this->available();
  while (avail > 0) {
    const size_t to_read = std::min(avail, sizeof(buf));
    if (!this->read_array(buf, to_read)) {
      if (!this->read_failed_) {
        ESP_LOGW(TAG, "UART read failed");
      }
      this->read_failed_ = true;
      break;
    }
    this->read_failed_ = false;
    avail -= to_read;
    for (size_t i = 0; i < to_read; i++)
      this->readline_(buf[i]);
  }
  this->run_sequencer_();
}

void DFRobotSEN0609Component::readline_(uint8_t c) {
  char *line = this->line_reader_.feed(c);
  if (line != nullptr) {
    this->handle_line_(line);
    return;
  }
  this->line_reader_.log_if_overflowed(TAG);
  // The prompt has no line end, and the echo of the next command follows it on the same line
  if (c == '>' && ESPHOME_strcmp_P(this->line_reader_.partial(), ESPHOME_PSTR("DFRobot:/>")) == 0) {
    this->line_reader_.reset();
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
  // Version lines name their value, so they are used even when they arrive late
  const bool software = starts_with(line, ESPHOME_PSTR("SoftwareVersion:"));
  if (software || starts_with(line, ESPHOME_PSTR("HardwareVersion:"))) {
    if (this->operation_ == Operation::OPERATION_READ &&
        this->param_ == (software ? PARAM_SW_VERSION : PARAM_HW_VERSION))
      this->version_received_ = true;
#ifdef USE_TEXT_SENSOR
    text_sensor::TextSensor *sens = software ? this->version_text_sensor_ : this->hardware_version_text_sensor_;
    if (sens != nullptr)
      sens->publish_state(line + 16);
#endif
    return;
  }
  if (this->operation_ == Operation::OPERATION_NONE) {
    ESP_LOGD(TAG, "Ignored line, no command pending: %s", line);
    return;
  }
  if (strcmp(line, this->command_) == 0) {
    this->echo_seen_ = true;
    return;
  }
  // The module echoes every command first; lines before the echo belong to an earlier command
  if (!this->echo_seen_) {
    ESP_LOGD(TAG, "Ignored line from an earlier command: %s", line);
    return;
  }

  if (starts_with(line, ESPHOME_PSTR("Done")) || answer_means_done(this->operation_, line)) {
    this->finish_operation_();
  } else if (starts_with(line, ESPHOME_PSTR("Response "))) {
    this->parse_response_(line + 9);
  } else if (ESPHOME_strstr_P(line, ESPHOME_PSTR("not stopped")) != nullptr) {
    // Stop the sensor before the next attempt
    this->stopped_ = false;
    this->fail_operation_();
  } else if (starts_with(line, ESPHOME_PSTR("Error")) ||
             ESPHOME_strstr_P(line, ESPHOME_PSTR("not recognized")) != nullptr) {
    ESP_LOGW(TAG, "Command '%s' rejected: %s", this->command_, line);
    this->fail_operation_();
  } else {
    ESP_LOGD(TAG, "Unexpected answer to '%s': %s", this->command_, line);
  }
}

void DFRobotSEN0609Component::handle_frame_(char *line) {
  // $DFHPD,<presence>, , , *
  if (starts_with(line, ESPHOME_PSTR("$DFHPD,"))) {
    const char presence = line[7];
    if (presence != '0' && presence != '1') {
      this->log_bad_frame_(LOG_STR("Malformed presence frame"), line);
      return;
    }
    this->last_frame_ms_ = App.get_loop_component_start_time();
#ifdef USE_BINARY_SENSOR
    if (this->has_target_binary_sensor_ != nullptr)
      this->has_target_binary_sensor_->publish_state(presence == '1');
#endif
    return;
  }
  // $DFDMD,<targets>,<reserved>,<distance m>,<speed m/s>,<energy>,...*
  if (!starts_with(line, ESPHOME_PSTR("$DFDMD,"))) {
    this->log_bad_frame_(LOG_STR("Unknown frame"), line);
    return;
  }
  char *fields[FRAME_FIELDS];
  if (split_fields(line + 7, fields, FRAME_FIELDS) < FRAME_FIELDS) {
    this->log_bad_frame_(LOG_STR("Malformed target frame"), line);
    return;
  }
  const optional<uint8_t> targets = parse_number<uint8_t>(fields[0]);
  if (!targets.has_value()) {
    this->log_bad_frame_(LOG_STR("Malformed target frame"), line);
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
  const optional<float> distance = parse_number<float>(fields[2]);
  const optional<float> speed = parse_number<float>(fields[3]);
  const optional<float> energy = parse_number<float>(fields[4]);
  if (!distance.has_value() || !speed.has_value() || !energy.has_value()) {
    this->log_bad_frame_(LOG_STR("Malformed target frame"), line);
    return;
  }
#ifdef USE_BINARY_SENSOR
  if (this->has_target_binary_sensor_ != nullptr)
    this->has_target_binary_sensor_->publish_state(true);
#endif
#ifdef USE_SENSOR
  this->publish_target_(*distance, *speed, *energy);
#endif
}

void DFRobotSEN0609Component::log_bad_frame_(const LogString *problem, const char *line) {
  // The module sends frames several times a second, so only the first bad one is logged
  if (this->bad_frame_logged_)
    return;
  this->bad_frame_logged_ = true;
  ESP_LOGW(TAG, "%s: %s", LOG_STR_ARG(problem), line);
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

void DFRobotSEN0609Component::parse_response_(const char *args) {
  this->response_count_ = 0;
  while (this->response_count_ < MAX_RESPONSE_VALUES) {
    char *end;
    const float value = strtof(args, &end);
    // strtof also takes "nan" and "inf", which no setting can hold
    if (end == args || !std::isfinite(value))
      break;
    this->response_[this->response_count_++] = value;
    args = end;
  }
}

void DFRobotSEN0609Component::queue_configuration_() {
  // The mode, the UART output and the LEDs are applied at every boot instead of being saved
  uint16_t writes = param_bit(PARAM_RUN_APP);
  if (this->mode_ == RunMode::RUN_MODE_PRESENCE)
    writes |= param_bit(PARAM_UART_OUTPUT);
#ifdef USE_SWITCH
  if (this->switches_[SWITCH_RUN_LED] != nullptr)
    writes |= param_bit(PARAM_RUN_LED);
  if (this->switches_[SWITCH_OUT_LED] != nullptr)
    writes |= param_bit(PARAM_OUT_LED);
#endif
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
#ifdef USE_NUMBER
  for (uint8_t i = 0; i < NUMBER_COUNT; i++) {
    if (this->numbers_[i] != nullptr)
      reads |= param_bit(static_cast<Param>(progmem_read_byte(&NUMBER_PARAMS[i])));
  }
#endif
#ifdef USE_SWITCH
  if (this->switches_[SWITCH_MICRO_MOTION] != nullptr)
    reads |= param_bit(PARAM_MICRO_MOTION);
#endif
#ifdef USE_SELECT
  if (this->out_pin_level_select_ != nullptr)
    reads |= param_bit(PARAM_GPIO_MODE);
#endif
  return reads;
}

void DFRobotSEN0609Component::check_frames_(uint32_t now) {
  // Speed mode frames are not watched: whether the module sends them without a target is not known
  if (this->mode_ != RunMode::RUN_MODE_PRESENCE || now - this->last_frame_ms_ < FRAME_TIMEOUT_MS)
    return;
  // The module most likely restarted on its own and lost the configuration applied at boot
  ESP_LOGW(TAG, "No presence frames, applying the configuration again");
  this->status_set_warning();
#ifdef USE_BINARY_SENSOR
  if (this->has_target_binary_sensor_ != nullptr)
    this->has_target_binary_sensor_->invalidate_state();
#endif
  this->last_frame_ms_ = now;
  this->queue_configuration_();
}

void DFRobotSEN0609Component::request_write_(Param param) {
  const uint32_t now = App.get_loop_component_start_time();
  this->pending_writes_ |= param_bit(param);
  this->last_change_ms_ = now;
  if (this->operation_ != Operation::OPERATION_NONE || this->stopped_)
    return;
  // Start a new sequence a little later, so changes made together end in one flash write.
  // A longer hold, such as the back-off from a module that does not answer, is not cut short.
  const uint32_t elapsed = now - this->hold_start_ms_;
  if (elapsed >= this->hold_ms_ || this->hold_ms_ - elapsed < COALESCE_MS)
    this->hold_(COALESCE_MS);
}

void DFRobotSEN0609Component::restart() {
  this->restart_pending_ = true;
  this->hold_(COMMAND_GAP_MS);
}

void DFRobotSEN0609Component::factory_reset() {
  this->factory_reset_pending_ = true;
  this->hold_(COMMAND_GAP_MS);
}

void DFRobotSEN0609Component::query_params() {
  this->pending_reads_ |= this->wanted_reads_();
  this->hold_(COMMAND_GAP_MS);
}

#ifdef USE_NUMBER
void DFRobotSEN0609Component::control_number(NumberSetting setting, float value) {
  // The module needs min_range <= trigger_range <= max_range; a value not read yet is NAN and passes
  const auto &values = this->number_values_;
  const float min_range = setting == NUMBER_MIN_RANGE ? value : values[NUMBER_MIN_RANGE];
  const float max_range = setting == NUMBER_MAX_RANGE ? value : values[NUMBER_MAX_RANGE];
  const float trigger_range = setting == NUMBER_TRIGGER_RANGE ? value : values[NUMBER_TRIGGER_RANGE];
  if (setting <= NUMBER_TRIGGER_RANGE &&
      (min_range > max_range || trigger_range < min_range || trigger_range > max_range)) {
    ESP_LOGW(TAG, "Ranges must keep minimum <= trigger <= maximum, not written");
    if (!std::isnan(this->number_values_[setting]))
      this->publish_number_(setting, this->number_values_[setting]);
    return;
  }
  this->changed_numbers_ |= 1U << setting;
  this->publish_number_(setting, value);
  this->request_write_(static_cast<Param>(progmem_read_byte(&NUMBER_PARAMS[setting])));
}

void DFRobotSEN0609Component::forget_number_changes_(Param param, bool unknown) {
  for (uint8_t i = 0; i < NUMBER_COUNT; i++) {
    if (progmem_read_byte(&NUMBER_PARAMS[i]) != param)
      continue;
    this->changed_numbers_ &= ~(1U << i);
    if (unknown)
      this->publish_number_(static_cast<NumberSetting>(i), NAN);
  }
}

void DFRobotSEN0609Component::publish_number_(NumberSetting setting, float value) {
  this->number_values_[setting] = value;
  if (this->numbers_[setting] != nullptr)
    this->numbers_[setting]->publish_state(value);
}
#endif

#ifdef USE_SWITCH
void DFRobotSEN0609Component::control_switch(SwitchSetting setting, bool state) {
  this->publish_switch_(setting, state);
  switch (setting) {
    case SWITCH_MICRO_MOTION:
      this->request_write_(PARAM_MICRO_MOTION);
      break;
    case SWITCH_RUN_LED:
      this->request_write_(PARAM_RUN_LED);
      break;
    default:
      this->request_write_(PARAM_OUT_LED);
      break;
  }
}

void DFRobotSEN0609Component::publish_switch_(SwitchSetting setting, bool state) {
  this->switch_states_[setting] = state;
  if (this->switches_[setting] != nullptr)
    this->switches_[setting]->publish_state(state);
}
#endif

#ifdef USE_SELECT
void DFRobotSEN0609Component::control_out_pin_level(size_t index) {
  this->out_pin_level_ = static_cast<uint8_t>(index);
  this->out_pin_level_select_->publish_state(index);
  this->request_write_(PARAM_GPIO_MODE);
}
#endif

void DFRobotSEN0609Component::hold_(uint32_t duration_ms) {
  this->hold_start_ms_ = App.get_loop_component_start_time();
  this->hold_ms_ = duration_ms;
}

void DFRobotSEN0609Component::run_sequencer_() {
  const uint32_t now = App.get_loop_component_start_time();
  if (this->operation_ != Operation::OPERATION_NONE) {
    uint32_t timeout = DEFAULT_TIMEOUT_MS;
    switch (this->operation_) {
      case Operation::OPERATION_RESTART:
        timeout = RESTART_MS;
        break;
      case Operation::OPERATION_FACTORY_RESET:
        timeout = FACTORY_RESET_TIMEOUT_MS;
        break;
      case Operation::OPERATION_STOP:
      case Operation::OPERATION_SAVE:
        timeout = LONG_TIMEOUT_MS;
        break;
      default:
        break;
    }
    if (now - this->sent_ms_ < timeout)
      return;
    if (this->operation_ == Operation::OPERATION_RESTART) {
      this->finish_operation_();
    } else {
      ESP_LOGD(TAG, "No answer to '%s'", this->command_);
      this->fail_operation_();
    }
    return;
  }
  if (now - this->hold_start_ms_ < this->hold_ms_)
    return;

  const bool work_pending =
      this->factory_reset_pending_ || this->save_pending_ || this->pending_writes_ != 0 || this->pending_reads_ != 0;
  Operation operation;
  Param param = PARAM_COUNT;
  if (this->restart_pending_) {
    // The reboot drops a write the module took but did not save; writes not sent yet are kept for after it
    operation = this->stopped_ && this->save_pending_ ? Operation::OPERATION_SAVE : Operation::OPERATION_RESTART;
  } else if (!work_pending) {
    if (!this->stopped_) {
      this->check_frames_(now);
      // Work given up on is tried again in a later sequence, so the sensor runs in between
      if ((this->deferred_writes_ != 0 || this->deferred_save_) && now - this->deferred_ms_ >= GIVE_UP_BACKOFF_MS) {
        this->pending_writes_ |= this->deferred_writes_;
        this->save_pending_ = this->save_pending_ || this->deferred_save_;
        this->deferred_writes_ = 0;
        this->deferred_save_ = false;
      }
      return;
    }
    operation = Operation::OPERATION_START;
  } else if (!this->stopped_) {
    operation = Operation::OPERATION_STOP;
  } else if (this->factory_reset_pending_) {
    operation = Operation::OPERATION_FACTORY_RESET;
  } else if (this->pending_writes_ != 0) {
    operation = Operation::OPERATION_WRITE;
    param = static_cast<Param>(__builtin_ctz(this->pending_writes_));
  } else if (this->save_pending_) {
    // Changes made within a second of each other share one flash write, also while a sequence runs
    if (now - this->last_change_ms_ < COALESCE_MS)
      return;
    operation = Operation::OPERATION_SAVE;
  } else {
    operation = Operation::OPERATION_READ;
    param = static_cast<Param>(__builtin_ctz(this->pending_reads_));
  }

  const uint16_t bit = param_bit(param);
  bool built = this->build_command_(operation, param);
  if (!built && operation == Operation::OPERATION_WRITE) {
    // The value this write is paired with is unknown: read it first, once, and keep the write pending
    if ((this->paired_reads_ & bit) == 0)
      this->pending_reads_ |= this->wanted_reads_() & bit;
    if ((this->pending_reads_ & bit) != 0) {
      this->paired_reads_ |= bit;
      operation = Operation::OPERATION_READ;
      built = this->build_command_(operation, param);
    }
  }
  if (!built) {
    if (operation == Operation::OPERATION_WRITE) {
      ESP_LOGW(TAG, "Setting not written: the value it is paired with could not be read");
      this->pending_writes_ &= ~bit;
      this->paired_reads_ &= ~bit;
#ifdef USE_NUMBER
      // Both values come from the read that failed, so what the module holds is unknown
      this->forget_number_changes_(param, true);
#endif
    } else {
      ESP_LOGE(TAG, "No command for %s of parameter %u", LOG_STR_ARG(operation_to_str(operation)),
               static_cast<unsigned>(param));
      this->pending_reads_ &= ~bit;
    }
    this->sequence_failed_ = true;
    this->status_set_warning();
    return;
  }
  // Cleared now, so a change made while the write is on its way is written again
  if (operation == Operation::OPERATION_WRITE) {
    this->pending_writes_ &= ~bit;
    this->paired_reads_ &= ~bit;
#ifdef USE_NUMBER
    this->forget_number_changes_(param, false);
#endif
  }
  this->operation_ = operation;
  this->param_ = param;
  this->response_count_ = 0;
  this->echo_seen_ = false;
  this->version_received_ = false;
  this->sent_ms_ = now;
  ESP_LOGV(TAG, "TX: %s", this->command_);
  this->write_str(this->command_);
  this->write_array(LINE_END, sizeof(LINE_END));
}

bool DFRobotSEN0609Component::build_command_(Operation operation, Param param) {
  char *buf = this->command_;
  constexpr size_t len = MAX_COMMAND_LENGTH;
  switch (operation) {
    case Operation::OPERATION_STOP:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("sensorStop"), len);
      return true;
    case Operation::OPERATION_START:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("sensorStart"), len);
      return true;
    case Operation::OPERATION_RESTART:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("resetSystem"), len);
      return true;
    case Operation::OPERATION_FACTORY_RESET:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("resetCfg"), len);
      return true;
    case Operation::OPERATION_SAVE:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("saveConfig"), len);
      return true;
    case Operation::OPERATION_READ:
      ESPHOME_strncpy_P(buf, reinterpret_cast<ESPHOME_PGM_P>(ReadCommands::get_progmem_str(param, 0)), len);
      return buf[0] != '\0';
    default:
      break;
  }

  switch (param) {
    case PARAM_RUN_APP:
      ESPHOME_snprintf_P(buf, len, ESPHOME_PSTR("setRunApp %u"), static_cast<unsigned>(this->mode_));
      return true;
    case PARAM_UART_OUTPUT:
      ESPHOME_strncpy_P(buf, ESPHOME_PSTR("setUartOutput 1 1 1 1.0"), len);
      return true;
#ifdef USE_SWITCH
    case PARAM_MICRO_MOTION:
      ESPHOME_snprintf_P(buf, len, ESPHOME_PSTR("setMicroMotion %u"),
                         static_cast<unsigned>(this->switch_states_[SWITCH_MICRO_MOTION]));
      return true;
    case PARAM_RUN_LED:
    case PARAM_OUT_LED: {
      // 0 turns the LED on
      const bool on = this->switch_states_[led_switch(param)];
      this->sent_led_on_ = on;
      ESPHOME_snprintf_P(buf, len, ESPHOME_PSTR("setLedMode %u %u"), param == PARAM_RUN_LED ? 1U : 2U, on ? 0U : 1U);
      return true;
    }
#endif
#ifdef USE_SELECT
    case PARAM_GPIO_MODE:
      ESPHOME_snprintf_P(buf, len, ESPHOME_PSTR("setGpioMode 1 %u"), static_cast<unsigned>(this->out_pin_level_));
      return true;
#endif
    default:
      break;
  }

#ifdef USE_NUMBER
  const auto &values = this->number_values_;
  float first = NAN;
  float second = 0.0f;
  ESPHOME_PGM_P format = nullptr;
  switch (param) {
    case PARAM_RANGE:
      first = values[NUMBER_MIN_RANGE];
      second = values[NUMBER_MAX_RANGE];
      format = ESPHOME_PSTR("setRange %.2f %.2f");
      break;
    case PARAM_TRIG_RANGE:
      first = values[NUMBER_TRIGGER_RANGE];
      format = ESPHOME_PSTR("setTrigRange %.2f");
      break;
    case PARAM_SENSITIVITY:
      first = values[NUMBER_HOLD_SENSITIVITY];
      second = values[NUMBER_TRIGGER_SENSITIVITY];
      format = ESPHOME_PSTR("setSensitivity %.0f %.0f");
      break;
    case PARAM_LATENCY:
      first = values[NUMBER_ON_LATENCY];
      second = values[NUMBER_OFF_LATENCY];
      format = ESPHOME_PSTR("setLatency %.2f %.1f");
      break;
    case PARAM_INHIBIT:
      first = values[NUMBER_INHIBIT_TIME];
      format = ESPHOME_PSTR("setInhibit %.2f");
      break;
    case PARAM_THR_FACTOR:
      first = values[NUMBER_THRESHOLD_FACTOR];
      format = ESPHOME_PSTR("setThrFactor %.0f");
      break;
    default:
      return false;
  }
  // Commands that take two values need both read from the module first
  if (std::isnan(first) || std::isnan(second))
    return false;
  ESPHOME_snprintf_P(buf, len, format, first, second);
  return true;
#else
  return false;
#endif
}

bool DFRobotSEN0609Component::apply_read_(Param param) {
  const float *r = this->response_;
  const uint8_t count = this->response_count_;
  // A change still waiting to be written is newer than what the module holds
  const bool write_pending = (this->pending_writes_ & param_bit(param)) != 0;
#ifdef USE_NUMBER
  // Checked for each number: publishing runs automations, which may change the next one
  auto read_number = [this](NumberSetting setting, float value) {
    if ((this->changed_numbers_ & (1U << setting)) == 0)
      this->publish_number_(setting, value);
  };
#endif
  switch (param) {
#ifdef USE_NUMBER
    case PARAM_RANGE:
      if (count < 2)
        return false;
      read_number(NUMBER_MIN_RANGE, r[0]);
      read_number(NUMBER_MAX_RANGE, r[1]);
      return true;
    case PARAM_TRIG_RANGE:
      if (count < 1)
        return false;
      read_number(NUMBER_TRIGGER_RANGE, r[0]);
      return true;
    case PARAM_SENSITIVITY:
      if (count < 2)
        return false;
      read_number(NUMBER_HOLD_SENSITIVITY, r[0]);
      read_number(NUMBER_TRIGGER_SENSITIVITY, r[1]);
      return true;
    case PARAM_LATENCY:
      if (count < 2)
        return false;
      read_number(NUMBER_ON_LATENCY, r[0]);
      read_number(NUMBER_OFF_LATENCY, r[1]);
      return true;
    case PARAM_INHIBIT:
      if (count < 1)
        return false;
      read_number(NUMBER_INHIBIT_TIME, r[0]);
      return true;
    case PARAM_THR_FACTOR:
      if (count < 1)
        return false;
      read_number(NUMBER_THRESHOLD_FACTOR, r[0]);
      return true;
#endif
#ifdef USE_SWITCH
    case PARAM_MICRO_MOTION:
      if (count < 1)
        return false;
      if (!write_pending)
        this->publish_switch_(SWITCH_MICRO_MOTION, r[0] != 0.0f);
      return true;
#endif
#ifdef USE_SELECT
    case PARAM_GPIO_MODE:
      // Response <pin> <level>
      if (count < 2 || (r[1] != 0.0f && r[1] != 1.0f))
        return false;
      if (!write_pending) {
        this->out_pin_level_ = static_cast<uint8_t>(r[1]);
        this->out_pin_level_select_->publish_state(this->out_pin_level_);
      }
      return true;
#endif
    case PARAM_SW_VERSION:
    case PARAM_HW_VERSION:
      // The versions are published as their lines arrive
      return this->version_received_;
    default:
      return true;
  }
}

void DFRobotSEN0609Component::finish_operation_() {
  switch (this->operation_) {
    case Operation::OPERATION_STOP:
      this->stopped_ = true;
      break;
    case Operation::OPERATION_START:
      this->stopped_ = false;
      this->last_frame_ms_ = App.get_loop_component_start_time();
      if (!this->sequence_failed_)
        this->status_clear_warning();
      this->sequence_failed_ = false;
      break;
    case Operation::OPERATION_RESTART:
      // The module starts sensing on its own after a reboot
      this->restart_pending_ = false;
      this->stopped_ = false;
      this->queue_configuration_();
      break;
    case Operation::OPERATION_FACTORY_RESET:
      this->factory_reset_pending_ = false;
      this->queue_configuration_();
      break;
    case Operation::OPERATION_WRITE:
      this->deferred_writes_ &= ~param_bit(this->param_);
      if (PERSISTENT_PARAMS & param_bit(this->param_))
        this->save_pending_ = true;
#ifdef USE_SWITCH
      if (is_led(this->param_)) {
        const uint8_t bit = 1U << led_switch(this->param_);
        this->led_known_ |= bit;
        this->led_on_ = this->sent_led_on_ ? (this->led_on_ | bit) : (this->led_on_ & ~bit);
      }
#endif
      break;
    case Operation::OPERATION_SAVE:
      this->save_pending_ = false;
      this->deferred_save_ = false;
      break;
    case Operation::OPERATION_READ:
      if (!this->apply_read_(this->param_)) {
        ESP_LOGW(TAG, "Malformed answer to '%s'", this->command_);
        this->fail_operation_();
        return;
      }
      this->pending_reads_ &= ~param_bit(this->param_);
      this->paired_reads_ &= ~param_bit(this->param_);
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
  const bool keep_write = operation == Operation::OPERATION_WRITE && (FRAME_PARAMS & param_bit(this->param_)) != 0;
  const bool retry = ++attempts < MAX_ATTEMPTS;
  if (operation == Operation::OPERATION_WRITE && (retry || keep_write))
    this->pending_writes_ |= param_bit(this->param_);
  if (retry)
    return;

  attempts = 0;
  this->status_set_warning();
  if (aux || keep_write) {
    // Without stop, start or the configuration the sensor cannot run as set up; try again later
    ESP_LOGW(TAG, "Command '%s' failed, trying again later", this->command_);
    this->hold_ms_ = GIVE_UP_BACKOFF_MS;
    return;
  }
  ESP_LOGW(TAG, "Giving up on %s '%s'", LOG_STR_ARG(operation_to_str(operation)), this->command_);
  this->sequence_failed_ = true;
  switch (operation) {
    case Operation::OPERATION_RESTART:
      this->restart_pending_ = false;
      break;
    case Operation::OPERATION_FACTORY_RESET:
      this->factory_reset_pending_ = false;
      break;
    case Operation::OPERATION_WRITE:
#ifdef USE_SWITCH
      if (is_led(this->param_)) {
        // A newer change is written anyway
        if ((this->pending_writes_ & param_bit(this->param_)) != 0)
          break;
        // The LEDs cannot be read: show the state the module last accepted, or write this one again later
        const uint8_t bit = 1U << led_switch(this->param_);
        if ((this->led_known_ & bit) != 0) {
          this->publish_switch_(led_switch(this->param_), (this->led_on_ & bit) != 0);
        } else {
          this->deferred_writes_ |= param_bit(this->param_);
          this->deferred_ms_ = App.get_loop_component_start_time();
        }
        break;
      }
#endif
      // The entity shows a value the module never got; show the module's value again
      this->pending_reads_ |= this->wanted_reads_() & param_bit(this->param_);
      break;
    case Operation::OPERATION_SAVE:
      // The entities show the changes as kept, but unsaved they are lost at power off: save in a later sequence
      this->save_pending_ = false;
      this->deferred_save_ = true;
      this->deferred_ms_ = App.get_loop_component_start_time();
      break;
    case Operation::OPERATION_READ:
      this->pending_reads_ &= ~param_bit(this->param_);
      break;
    default:
      break;
  }
}

}  // namespace esphome::dfrobot_sen0609
