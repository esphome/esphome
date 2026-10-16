#include "commands.h"

#include <cmath>
#include <cstring>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "dfrobot_sen0395.h"

namespace esphome::dfrobot_sen0395 {

ESPHOME_LOG_TAG(TAG, "dfrobot_sen0395.commands");

static constexpr float DET_RANGE_STEP_M = 0.15f;
static constexpr float LATENCY_STEP_S = 0.025f;

void DfrobotSen0395Component::enqueue_det_range_cfg(float min1, float max1, float min2, float max2, float min3,
                                                    float max3, float min4, float max4) {
  // TODO: Print warning when values are rounded
  const float values[8] = {min1, max1, min2, max2, min3, max3, min4, max4};
  int16_t steps[8];
  // Segments are used up to the first one with a negative bound
  bool in_use = true;
  for (size_t i = 0; i < 8; i += 2) {
    in_use = in_use && values[i] >= 0 && values[i + 1] >= 0;
    for (size_t j = i; j < i + 2; j++) {
      steps[j] = in_use ? static_cast<int16_t>(clamp(rintf(values[j] / DET_RANGE_STEP_M), 0.0f, 32767.0f)) : -1;
    }
  }
  if (steps[0] < 0) {
    ESP_LOGW(TAG, "DetRangeCfgCommand invalid input parameters. Using range config 0 0.");
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

void DfrobotSen0395Component::format_command_(const Command &cmd, char *buf, size_t size) {
  const char *text = "";
  switch (cmd.type) {
    case CommandType::COMMAND_TYPE_POWER:
      text = cmd.arg ? "sensorStart" : "sensorStop";
      break;
    case CommandType::COMMAND_TYPE_DET_RANGE_CFG: {
      size_t pos = buf_append_printf(buf, size, 0, "detRangeCfg -1");
      for (size_t i = 0; i < 8 && this->det_range_steps_[i] >= 0; i += 2) {
        pos = buf_append_printf(buf, size, pos, " %d %d", this->det_range_steps_[i], this->det_range_steps_[i + 1]);
      }
      return;
    }
    case CommandType::COMMAND_TYPE_SET_LATENCY:
      snprintf(buf, size, "setLatency %.03f %.03f", this->latency_steps_[0] * LATENCY_STEP_S,
               this->latency_steps_[1] * LATENCY_STEP_S);
      return;
    case CommandType::COMMAND_TYPE_SENSOR_CFG_START:
      snprintf(buf, size, "sensorCfgStart %d", cmd.arg);
      return;
    case CommandType::COMMAND_TYPE_FACTORY_RESET:
      text = "factoryReset 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";
      break;
    case CommandType::COMMAND_TYPE_RESET_SYSTEM:
      text = "resetSystem";
      break;
    case CommandType::COMMAND_TYPE_SAVE_CFG:
      text = "saveCfg 0x45670123 0xCDEF89AB 0x956128C6 0xDF54AC89";
      break;
    case CommandType::COMMAND_TYPE_LED_MODE:
      text = cmd.arg ? "setLedMode 1 0" : "setLedMode 1 1";
      break;
    case CommandType::COMMAND_TYPE_UART_OUTPUT:
      text = cmd.arg ? "setUartOutput 1 1" : "setUartOutput 1 0";
      break;
    case CommandType::COMMAND_TYPE_SENSITIVITY:
      snprintf(buf, size, "setSensitivity %d", cmd.arg);
      return;
    case CommandType::COMMAND_TYPE_READ_STATE:
      break;
  }
  snprintf(buf, size, "%s", text);
}

bool DfrobotSen0395Component::handle_reply_(const Command &cmd) {
  const char *message = this->read_buffer_;
  auto is = [message](const char *text) { return strcmp(message, text) == 0; };
  const bool not_stopped = is("sensor is not stopped");
  const bool done = is("Done");
  char used[72];

  switch (cmd.type) {
    case CommandType::COMMAND_TYPE_READ_STATE:
      return true;

    case CommandType::COMMAND_TYPE_POWER:
      if (is("sensor stopped already")) {
        this->set_active(false);
        ESP_LOGI(TAG, "Stopped sensor (already stopped)");
      } else if (is("sensor started already")) {
        this->set_active(true);
        ESP_LOGI(TAG, "Started sensor (already started)");
      } else if (is("new parameter isn't save, can't startSensor")) {
        this->set_active(false);
        ESP_LOGE(TAG, "Can't start sensor! (Use SaveCfgCommand to save config first)");
      } else if (done) {
        this->set_active(cmd.arg);
        if (cmd.arg) {
          ESP_LOGI(TAG, "Started sensor");
        } else {
          ESP_LOGI(TAG, "Stopped sensor");
        }
      } else {
        return false;
      }
      return true;

    case CommandType::COMMAND_TYPE_DET_RANGE_CFG:
      if (not_stopped) {
        ESP_LOGE(TAG, "Cannot configure range config. Sensor is not stopped!");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG,
               "Updated detection area config:\n"
               "  Detection area 1 from %.02fm to %.02fm.",
               this->det_range_steps_[0] * DET_RANGE_STEP_M, this->det_range_steps_[1] * DET_RANGE_STEP_M);
      for (size_t i = 2; i < 8 && this->det_range_steps_[i] >= 0; i += 2) {
        ESP_LOGI(TAG, "  Detection area %u from %.02fm to %.02fm.", static_cast<unsigned>(i / 2 + 1),
                 this->det_range_steps_[i] * DET_RANGE_STEP_M, this->det_range_steps_[i + 1] * DET_RANGE_STEP_M);
      }
      break;

    case CommandType::COMMAND_TYPE_SET_LATENCY:
      if (not_stopped) {
        ESP_LOGE(TAG, "Cannot configure output latency. Sensor is not stopped!");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG,
               "Updated output latency config:\n"
               "  Signal that someone was detected is delayed by %.03f s.\n"
               "  Signal that nobody is detected anymore is delayed by %.03f s.",
               this->latency_steps_[0] * LATENCY_STEP_S, this->latency_steps_[1] * LATENCY_STEP_S);
      break;

    case CommandType::COMMAND_TYPE_SENSOR_CFG_START:
      if (not_stopped) {
        ESP_LOGE(TAG, "Cannot configure sensor startup behavior. Sensor is not stopped!");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG, "Updated sensor startup behavior:");
      this->set_start_after_boot(cmd.arg);
      if (cmd.arg) {
        ESP_LOGI(TAG, "Sensor will start automatically after power-on.");
      } else {
        ESP_LOGI(TAG, "Sensor needs to be started manually after power-on.");
      }
      break;

    case CommandType::COMMAND_TYPE_FACTORY_RESET:
      if (not_stopped) {
        ESP_LOGE(TAG, "Cannot factory reset. Sensor is not stopped!");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG, "Sensor factory reset done.");
      return true;

    case CommandType::COMMAND_TYPE_RESET_SYSTEM:
      if (!is("leapMMW:/>"))
        return false;
      ESP_LOGI(TAG, "Restarted sensor.");
      return true;

    case CommandType::COMMAND_TYPE_SAVE_CFG:
      if (is("no parameter has changed")) {
        ESP_LOGI(TAG, "Not saving config (no parameter changed).");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG, "Saved config. Saving a lot may damage the sensor.");
      return true;

    case CommandType::COMMAND_TYPE_LED_MODE:
      if (not_stopped) {
        ESP_LOGE(TAG, "Cannot set led mode. Sensor is not stopped!");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG, "Set led mode done.");
      this->set_led_active(cmd.arg);
      if (cmd.arg) {
        ESP_LOGI(TAG, "Sensor LED will blink.");
      } else {
        ESP_LOGI(TAG, "Turned off LED.");
      }
      break;

    case CommandType::COMMAND_TYPE_UART_OUTPUT:
      if (not_stopped) {
        ESP_LOGE(TAG, "Cannot set uart output mode. Sensor is not stopped!");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG, "Set uart mode done.");
      this->set_uart_presence_active(cmd.arg);
      if (cmd.arg) {
        ESP_LOGI(TAG, "Presence information is sent via UART and GPIO.");
      } else {
        ESP_LOGI(TAG, "Presence information is only sent via GPIO.");
      }
      break;

    case CommandType::COMMAND_TYPE_SENSITIVITY:
      if (not_stopped) {
        ESP_LOGE(TAG, "Cannot set sensitivity. Sensor is not stopped!");
        return true;
      }
      if (!done)
        return false;
      ESP_LOGI(TAG, "Set sensitivity done. Set to value %d.", cmd.arg);
      break;
  }

  this->format_command_(cmd, used, sizeof(used));
  ESP_LOGD(TAG, "Used command: %s", used);
  return true;
}

}  // namespace esphome::dfrobot_sen0395
