#include "dfrobot_sen0395.h"

#include <cmath>
#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::dfrobot_sen0395 {

ESPHOME_LOG_TAG(TAG, "dfrobot_sen0395");
static constexpr float DET_RANGE_STEP_M = 0.15f;
static constexpr float LATENCY_STEP_S = 0.025f;
// Sent after a reset with no line end
static constexpr char PROMPT[] PROGMEM = "leapMMW:/>";

// What each CommandType cannot do while the sensor runs; empty for commands that do not need it stopped
PROGMEM_STRING_TABLE(BusyActions, "", "configure range config", "configure output latency",
                     "configure sensor startup behavior", "factory reset", "", "", "set led mode",
                     "set uart output mode", "set sensitivity");
static_assert(BusyActions::COUNT == static_cast<size_t>(CommandType::COMMAND_TYPE_SENSITIVITY) + 1,
              "BusyActions needs one entry per CommandType");

void DfrobotSen0395Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Dfrobot Mmwave Radar:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Registered", this->detected_binary_sensor_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Sensor Active Switch", this->sensor_active_switch_);
  LOG_SWITCH("  ", "Turn on LED Switch", this->turn_on_led_switch_);
  LOG_SWITCH("  ", "Presence via UART Switch", this->presence_via_uart_switch_);
  LOG_SWITCH("  ", "Start after Boot Switch", this->start_after_boot_switch_);
#endif
}

void DfrobotSen0395Component::loop() {
  if (this->cmd_queue_.empty()) {
    this->read_state_();
    return;
  }
  // Commands are non-blocking and need to be called repeatedly
  if (this->run_command_(this->cmd_queue_.front(), App.get_loop_component_start_time())) {
    this->cmd_queue_.pop();
    this->cmd_sent_ = false;
    this->retries_left_ = 2;
  }
}

bool DfrobotSen0395Component::enqueue(CommandType type, uint8_t arg) {
  if (!this->cmd_queue_.push({type, arg})) {
    ESP_LOGE(TAG, "Command queue is full");
    return false;
  }
  return true;
}

bool DfrobotSen0395Component::has_queue_room(size_t count) const {
  if (COMMAND_QUEUE_SIZE - this->cmd_queue_.size() >= count)
    return true;
  ESP_LOGE(TAG, "Command queue is full");
  return false;
}

void DfrobotSen0395Component::enqueue_det_range_cfg(float min1, float max1, float min2, float max2, float min3,
                                                    float max3, float min4, float max4) {
  // TODO: Print warning when values are rounded
  const float values[8] = {min1, max1, min2, max2, min3, max3, min4, max4};
  int16_t steps[8];
  bool in_use = true;
  for (size_t i = 0; i < 8; i += 2) {
    in_use = in_use && values[i] >= 0 && values[i + 1] >= 0;
    for (size_t j = i; j < i + 2; j++) {
      steps[j] = in_use ? static_cast<int16_t>(clamp(roundf(values[j] / DET_RANGE_STEP_M), 0.0f, 32767.0f)) : -1;
    }
  }
  if (steps[0] < 0) {
    ESP_LOGW(TAG, "Invalid detection range, using 0 0");
    steps[0] = steps[1] = 0;
  }
  if (this->enqueue(CommandType::COMMAND_TYPE_DET_RANGE_CFG))
    memcpy(this->det_range_steps_, steps, sizeof(steps));
}

void DfrobotSen0395Component::enqueue_set_latency(float delay_after_detection, float delay_after_disappear) {
  if (!this->enqueue(CommandType::COMMAND_TYPE_SET_LATENCY))
    return;
  // 1638.375 s is 65535 steps
  this->latency_steps_[0] =
      static_cast<uint16_t>(clamp(std::round(delay_after_detection / LATENCY_STEP_S), 0.0f, 65535.0f));
  this->latency_steps_[1] =
      static_cast<uint16_t>(clamp(std::round(delay_after_disappear / LATENCY_STEP_S), 0.0f, 65535.0f));
}

void DfrobotSen0395Component::enqueue_saved_setting(CommandType type, bool value) {
  const bool was_active = this->active_;
  if (!this->has_queue_room(was_active ? 4 : 2))
    return;
  if (was_active)
    this->enqueue(CommandType::COMMAND_TYPE_POWER, false);
  this->enqueue(type, value);
  this->enqueue(CommandType::COMMAND_TYPE_SAVE_CFG);
  if (was_active)
    this->enqueue(CommandType::COMMAND_TYPE_POWER, true);
}

bool DfrobotSen0395Component::run_command_(const Command &cmd, uint32_t now) {
  // The interval between two commands must be larger than the command duration
  const uint32_t duration = cmd.type == CommandType::COMMAND_TYPE_SAVE_CFG ? 3000 : 1000;
  if (!this->cmd_sent_) {
    if (now - this->ts_last_cmd_sent_ > duration) {
      char buf[64];  // "detRangeCfg -1" + 8 * " 32767" + null
      this->format_command(cmd, buf, sizeof(buf));
      ESP_LOGD(TAG, "Sending command: %s", buf);
      this->write_str(buf);
      this->ts_last_cmd_sent_ = now;
      this->cmd_sent_ = true;
    }
    return false;  // Command not done yet
  }

  if (const char *message = this->read_message_()) {
    if (ESPHOME_strstr_P(message, ESPHOME_PSTR("is not recognized as a CLI command")) == nullptr) {
      if (!this->handle_reply_(cmd, message))
        return false;         // Command not done yet
      this->read_message_();  // Skip the prompt
      return true;
    }
    ESP_LOGD(TAG, "Command not recognized properly by sensor");
  } else if (now - this->ts_last_cmd_sent_ <= duration + 500) {
    return false;  // Command not done yet
  } else {
    ESP_LOGD(TAG, "Command timeout");
  }

  if (this->retries_left_ == 0) {
    this->read_message_();  // Skip the prompt
    return true;            // Give up
  }
  this->retries_left_--;
  this->cmd_sent_ = false;
  ESP_LOGD(TAG, "Retrying");
  return false;
}

void DfrobotSen0395Component::read_state_() {
  const char *message = this->read_message_();
  if (message == nullptr)
    return;
  if (ESPHOME_strstr_P(message, ESPHOME_PSTR("$JYBSS,0, , , *")) != nullptr) {
    this->set_detected_(false);
    this->set_active(true);
  } else if (ESPHOME_strstr_P(message, ESPHOME_PSTR("$JYBSS,1, , , *")) != nullptr) {
    this->set_detected_(true);
    this->set_active(true);
  }
}

static size_t append_cfg_key(char *buf, size_t size, size_t pos) {
  return buf_append_str(buf, size, pos, " 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89");
}

void DfrobotSen0395Component::format_command(const Command &cmd, char *buf, size_t size) const {
  buf[0] = '\0';
  switch (cmd.type) {
    case CommandType::COMMAND_TYPE_POWER:
      if (cmd.arg) {
        buf_append_str(buf, size, 0, "sensorStart");
      } else {
        buf_append_str(buf, size, 0, "sensorStop");
      }
      break;
    case CommandType::COMMAND_TYPE_DET_RANGE_CFG: {
      size_t pos = buf_append_str(buf, size, 0, "detRangeCfg -1");
      for (size_t i = 0; i < 8 && this->det_range_steps_[i] >= 0; i += 2) {
        pos = buf_append_printf(buf, size, pos, " %d %d", this->det_range_steps_[i], this->det_range_steps_[i + 1]);
      }
      break;
    }
    case CommandType::COMMAND_TYPE_SET_LATENCY:
      buf_append_printf(buf, size, 0, "setLatency %.03f %.03f", this->latency_steps_[0] * LATENCY_STEP_S,
                        this->latency_steps_[1] * LATENCY_STEP_S);
      break;
    case CommandType::COMMAND_TYPE_SENSOR_CFG_START:
      buf_append_printf(buf, size, 0, "sensorCfgStart %d", cmd.arg);
      break;
    case CommandType::COMMAND_TYPE_FACTORY_RESET:
      append_cfg_key(buf, size, buf_append_str(buf, size, 0, "factoryReset"));
      break;
    case CommandType::COMMAND_TYPE_RESET_SYSTEM:
      buf_append_str(buf, size, 0, "resetSystem");
      break;
    case CommandType::COMMAND_TYPE_SAVE_CFG:
      append_cfg_key(buf, size, buf_append_str(buf, size, 0, "saveCfg"));
      break;
    case CommandType::COMMAND_TYPE_LED_MODE:
      buf_append_printf(buf, size, 0, "setLedMode 1 %d", cmd.arg ? 0 : 1);
      break;
    case CommandType::COMMAND_TYPE_UART_OUTPUT:
      buf_append_printf(buf, size, 0, "setUartOutput 1 %d", cmd.arg ? 1 : 0);
      break;
    case CommandType::COMMAND_TYPE_SENSITIVITY:
      buf_append_printf(buf, size, 0, "setSensitivity %d", cmd.arg);
      break;
  }
}

bool DfrobotSen0395Component::handle_reply_(const Command &cmd, const char *message) {
  auto is = [message](ESPHOME_PGM_P text) { return ESPHOME_strcmp_P(message, text) == 0; };

  // Commands with their own replies
  switch (cmd.type) {
    case CommandType::COMMAND_TYPE_POWER:
      if (is(ESPHOME_PSTR("sensor stopped already"))) {
        this->set_active(false);
        ESP_LOGI(TAG, "Stopped sensor (already stopped)");
      } else if (is(ESPHOME_PSTR("sensor started already"))) {
        this->set_active(true);
        ESP_LOGI(TAG, "Started sensor (already started)");
      } else if (is(ESPHOME_PSTR("new parameter isn't save, can't startSensor"))) {
        this->set_active(false);
        ESP_LOGE(TAG, "Can't start sensor! (Use SaveCfgCommand to save config first)");
      } else if (is(ESPHOME_PSTR("Done"))) {
        this->set_active(cmd.arg);
        ESP_LOGI(TAG, "%s", cmd.arg ? LOG_STR_LITERAL("Started sensor") : LOG_STR_LITERAL("Stopped sensor"));
      } else {
        return false;
      }
      return true;
    case CommandType::COMMAND_TYPE_RESET_SYSTEM:
      if (!is(PROMPT))
        return false;
      ESP_LOGI(TAG, "Restarted sensor.");
      return true;
    case CommandType::COMMAND_TYPE_SAVE_CFG:
      if (is(ESPHOME_PSTR("no parameter has changed"))) {
        ESP_LOGI(TAG, "Not saving config (no parameter changed).");
      } else if (is(ESPHOME_PSTR("Done"))) {
        ESP_LOGI(TAG, "Saved config. Saving a lot may damage the sensor.");
      } else {
        return false;
      }
      return true;
    default:
      break;
  }

  // Configuration commands, which need the sensor stopped
  if (is(ESPHOME_PSTR("sensor is not stopped"))) {
    ESP_LOGE(TAG, "Cannot %s. Sensor is not stopped!",
             LOG_STR_ARG(BusyActions::get_log_str(static_cast<uint8_t>(cmd.type), 0)));
    return true;
  }
  if (!is(ESPHOME_PSTR("Done")))
    return false;

  switch (cmd.type) {
    case CommandType::COMMAND_TYPE_DET_RANGE_CFG:
      ESP_LOGI(TAG, "Updated detection area config:");
      for (size_t i = 0; i < 8 && this->det_range_steps_[i] >= 0; i += 2) {
        ESP_LOGI(TAG, "  Detection area %u from %.02fm to %.02fm.", static_cast<unsigned>(i / 2 + 1),
                 this->det_range_steps_[i] * DET_RANGE_STEP_M, this->det_range_steps_[i + 1] * DET_RANGE_STEP_M);
      }
      break;
    case CommandType::COMMAND_TYPE_SET_LATENCY:
      ESP_LOGI(TAG,
               "Updated output latency config:\n"
               "  Signal that someone was detected is delayed by %.03f s.\n"
               "  Signal that nobody is detected anymore is delayed by %.03f s.",
               this->latency_steps_[0] * LATENCY_STEP_S, this->latency_steps_[1] * LATENCY_STEP_S);
      break;
    case CommandType::COMMAND_TYPE_SENSOR_CFG_START:
      this->set_start_after_boot(cmd.arg);
      ESP_LOGI(TAG, "Updated sensor startup behavior: %s",
               cmd.arg ? LOG_STR_LITERAL("sensor will start automatically after power-on.")
                       : LOG_STR_LITERAL("sensor needs to be started manually after power-on."));
      break;
    case CommandType::COMMAND_TYPE_FACTORY_RESET:
      ESP_LOGI(TAG, "Sensor factory reset done.");
      break;
    case CommandType::COMMAND_TYPE_LED_MODE:
      this->set_led_active(cmd.arg);
      ESP_LOGI(TAG, "Set led mode done. %s",
               cmd.arg ? LOG_STR_LITERAL("Sensor LED will blink.") : LOG_STR_LITERAL("Turned off LED."));
      break;
    case CommandType::COMMAND_TYPE_UART_OUTPUT:
      this->set_uart_presence_active(cmd.arg);
      ESP_LOGI(TAG, "Set uart mode done. %s",
               cmd.arg ? LOG_STR_LITERAL("Presence information is sent via UART and GPIO.")
                       : LOG_STR_LITERAL("Presence information is only sent via GPIO."));
      break;
    case CommandType::COMMAND_TYPE_SENSITIVITY:
      ESP_LOGI(TAG, "Set sensitivity done. Set to value %d.", cmd.arg);
      break;
    default:
      break;
  }
  return true;
}

const char *DfrobotSen0395Component::read_message_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    if (byte >= 0x7F)
      byte = '?';  // needs to be valid utf8 string for log functions.
    const char *line = this->line_reader_.feed(byte);
    if (line == nullptr && byte == '>' && this->line_reader_.length() == sizeof(PROMPT) - 1) {
      line = this->line_reader_.partial();
      this->line_reader_.reset();
    }
    if (line != nullptr) {
      ESP_LOGV(TAG, "Message: %s", line);
      return line;
    }
    this->line_reader_.log_if_overflowed(TAG);
  }
  return nullptr;
}

void DfrobotSen0395Component::set_detected_(bool detected) {
  this->detected_ = detected;
#ifdef USE_BINARY_SENSOR
  PUBLISH_STATE(this->detected_binary_sensor_, detected);
#endif
}

}  // namespace esphome::dfrobot_sen0395
