#include "dfrobot_mmwave_radar.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace dfrobot_mmwave_radar {

static const char *const TAG = "dfrobot_mmwave_radar";
const char ASCII_CR = 0x0D;
const char ASCII_LF = 0x0A;

#ifdef USE_SWITCH
void DfrobotMmwaveRadarSwitch::set_type(const char *type) {
  if(strncmp(type, "turn_on_sensor", 14) == 0) {
    type_ = TURN_ON_SENSOR;
  }
  else if(strncmp(type, "turn_on_led", 11) == 0) {
    type_ = TURN_ON_LED;
  }
  else if(strncmp(type, "presence_via_uart", 17) == 0) {
    type_ = PRESENCE_VIA_UART;
  }
}
void DfrobotMmwaveRadarSwitch::write_state(bool state) {
  if(type_ == TURN_ON_SENSOR)
    component_->enqueue(make_unique<PowerCommand>(state));
  if(type_ == TURN_ON_LED) {
    bool wasActive = false;
    if(component_->is_active()) {
      wasActive = true;
      component_->enqueue(make_unique<PowerCommand>(0));
    }
    component_->enqueue(make_unique<LedModeCommand>(state));
    component_->enqueue(make_unique<SaveCfgCommand>());
    if(wasActive) {
      component_->enqueue(make_unique<PowerCommand>(1));
    }
  }
  if(type_ == PRESENCE_VIA_UART) {
    bool wasActive = false;
    if(component_->is_active()) {
      wasActive = true;
      component_->enqueue(make_unique<PowerCommand>(0));
    }
    component_->enqueue(make_unique<UartOutputCommand>(state));
    component_->enqueue(make_unique<SaveCfgCommand>());
    if(wasActive) {
      component_->enqueue(make_unique<PowerCommand>(1));
    }
  }
}
#endif

void DfrobotMmwaveRadarComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Dfrobot Mmwave Radar:");
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Registered", this->detected_binary_sensor_);
#endif
#ifdef USE_SWITCH
  if (this->active_switch_ != nullptr)
    LOG_SWITCH("  ", "Registered", this->active_switch_);
  if (this->led_switch_ != nullptr)
    LOG_SWITCH("  ", "Registered", this->led_switch_);
  if (this->uart_switch_ != nullptr)
    LOG_SWITCH("  ", "Registered", this->uart_switch_);
#endif
}

void DfrobotMmwaveRadarComponent::setup() {}

void DfrobotMmwaveRadarComponent::loop() {
  if (cmd_queue_.is_empty()) {
    // Command queue empty. Read sensor state.
    cmd_queue_.enqueue(make_unique<ReadStateCommand>());
  }

  // Commands are non-blocking and need to be called repeatedly.
  if (cmd_queue_.process(this)) {
    // Dequeue if command is done
    cmd_queue_.dequeue();
  }
}

int8_t DfrobotMmwaveRadarComponent::enqueue(std::unique_ptr<Command> cmd) {
  return cmd_queue_.enqueue(std::move(cmd));  // Transfer ownership using std::move
}

uint8_t DfrobotMmwaveRadarComponent::read_message_() {
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    if (this->read_pos_ == MMWAVE_READ_BUFFER_LENGTH)
      this->read_pos_ = 0;

    ESP_LOGVV(TAG, "Buffer pos: %u %d", this->read_pos_, byte);

    if (byte == ASCII_CR)
      continue;
    if (byte >= 0x7F)
      byte = '?';  // needs to be valid utf8 string for log functions.
    this->read_buffer_[this->read_pos_] = byte;

    if (this->read_pos_ == 9 && byte == '>')
      this->read_buffer_[++this->read_pos_] = ASCII_LF;

    if (this->read_buffer_[this->read_pos_] == ASCII_LF) {
      this->read_buffer_[this->read_pos_] = 0;
      this->read_pos_ = 0;
      ESP_LOGV(TAG, "Message: %s", this->read_buffer_);
      return 1;  // Full message in buffer
    } else {
      this->read_pos_++;
    }
  }
  return 0;  // No full message yet
}

uint8_t DfrobotMmwaveRadarComponent::find_prompt_() {
  if (this->read_message_()) {
    std::string message(this->read_buffer_);
    if (message.rfind("leapMMW:/>") != std::string::npos) {
      return 1;  // Prompt found
    }
  }
  return 0;  // Not found yet
}

uint8_t DfrobotMmwaveRadarComponent::send_cmd_(const char *cmd, uint32_t duration) {
  // The interval between two commands must be larger than the specified duration (in ms).
  if (millis() - ts_last_cmd_sent_ > duration) {
    this->write_str(cmd);
    ts_last_cmd_sent_ = millis();
    return 1;  // Command sent
  }
  // Could not send command yet as command duration did not fully pass yet.
  return 0;
}

void DfrobotMmwaveRadarComponent::set_detected_(bool detected) {
  this->detected_ = detected;
#ifdef USE_BINARY_SENSOR
  if (this->detected_binary_sensor_ != nullptr)
    this->detected_binary_sensor_->publish_state(detected);
#endif
}

int8_t CircularCommandQueue::enqueue(std::unique_ptr<Command> cmd) {
  if (this->is_full()) {
    ESP_LOGE(TAG, "Command queue is full");
    return -1;
  } else if (this->is_empty())
    front_++;
  rear_ = (rear_ + 1) % COMMAND_QUEUE_SIZE;
  commands_[rear_] = std::move(cmd);  // Transfer ownership using std::move
  return 1;
}

std::unique_ptr<Command> CircularCommandQueue::dequeue() {
  if (this->is_empty())
    return nullptr;
  std::unique_ptr<Command> dequeued_cmd = std::move(commands_[front_]);
  if (front_ == rear_) {
    front_ = -1;
    rear_ = -1;
  } else
    front_ = (front_ + 1) % COMMAND_QUEUE_SIZE;

  return dequeued_cmd;
}

bool CircularCommandQueue::is_empty() { return front_ == -1; }

bool CircularCommandQueue::is_full() { return (rear_ + 1) % COMMAND_QUEUE_SIZE == front_; }

// Run execute method of first in line command.
// Execute is non-blocking and has to be called until it returns 1.
uint8_t CircularCommandQueue::process(DfrobotMmwaveRadarComponent *component) {
  if (!is_empty()) {
    return commands_[front_]->execute(component);
  } else {
    return 1;
  }
}

uint8_t Command::execute(DfrobotMmwaveRadarComponent *component) {
  component_ = component;
  if (cmd_sent_) {
    if (component->read_message_()) {
      std::string message(component->read_buffer_);
      if (message.rfind("is not recognized as a CLI command") != std::string::npos) {
        ESP_LOGD(TAG, "Command not recognized properly by sensor");
        if (retries_left_ > 0) {
          retries_left_ -= 1;
          cmd_sent_ = false;
          ESP_LOGD(TAG, "Retrying...");
          return 0;
        } else {
          component->find_prompt_();
          return 1;  // Command done
        }
      }
      uint8_t rc = on_message(message);
      if (rc == 2) {
        if (retries_left_ > 0) {
          retries_left_ -= 1;
          cmd_sent_ = false;
          ESP_LOGD(TAG, "Retrying...");
          return 0;
        } else {
          component->find_prompt_();
          return 1;  // Command done
        }
      } else if (rc == 0) {
        return 0;
      } else {
        component->find_prompt_();
        return 1;
      }
    }
    if (millis() - component->ts_last_cmd_sent_ > timeout_ms_) {
      ESP_LOGD(TAG, "Command timeout");
      if (retries_left_ > 0) {
        retries_left_ -= 1;
        cmd_sent_ = false;
        ESP_LOGD(TAG, "Retrying...");
      } else {
        return 1;  // Command done
      }
    }
  } else if (component->send_cmd_(cmd_.c_str(), cmd_duration_ms_)) {
    cmd_sent_ = true;
  }
  return 0;  // Command not done yet
}

uint8_t ReadStateCommand::execute(DfrobotMmwaveRadarComponent *component) {
  component_ = component;
  if (component->read_message_()) {
    std::string message(component->read_buffer_);
    if (message.rfind("$JYBSS,0, , , *") != std::string::npos) {
      component->set_detected_(false);
      component_->set_active(true);
      return 1;  // Command done
    } else if (message.rfind("$JYBSS,1, , , *") != std::string::npos) {
      component->set_detected_(true);
      component_->set_active(true);
      return 1;  // Command done
    }
  }
  if (millis() - component->ts_last_cmd_sent_ > timeout_ms_) {
    return 1;  // Command done, timeout
  }
  return 0;  // Command not done yet.
}

uint8_t ReadStateCommand::on_message(std::string &message) { return 1; }

uint8_t PowerCommand::on_message(std::string &message) {
  if (message == "sensor stopped already") {
    component_->set_active(false);
    ESP_LOGI(TAG, "Stopped sensor (already stopped)");
    return 1;  // Command done
  } else if (message == "sensor started already") {
    component_->set_active(true);
    ESP_LOGI(TAG, "Started sensor (already started)");
    return 1;  // Command done
  } else if (message == "new parameter isn't save, can't startSensor") {
    component_->set_active(false);
    ESP_LOGE(TAG, "Can't start sensor! (Use SaveCfgCommand to save config first)");
    return 1;  // Command done
  } else if (message == "Done") {
    component_->set_active(power_on_);
    if (power_on_) {
      ESP_LOGI(TAG, "Started sensor");
    } else {
      ESP_LOGI(TAG, "Stopped sensor");
    }
    return 1;  // Command done
  }
  return 0;  // Command not done yet.
}

DetRangeCfgCommand::DetRangeCfgCommand(float min1, float max1, float min2, float max2, float min3, float max3,
                                       float min4, float max4) {
  // TODO: Print warning when values are rounded
  char tmp_cmd[46] = {0};

  if (min1 < 0 || max1 < 0) {
    min1_ = min1 = 0;
    max1_ = max1 = 0;
    min2_ = min2 = max2_ = max2 = min3_ = min3 = max3_ = max3 = min4_ = min4 = max4_ = max4 = -1;

    ESP_LOGW(TAG, "DetRangeCfgCommand invalid input parameters. Using range config 0 0.");

    sprintf(tmp_cmd, "detRangeCfg -1 0 0");
  } else if (min2 < 0 || max2 < 0) {
    min1_ = min1 = round(min1 / 0.15) * 0.15;
    max1_ = max1 = round(max1 / 0.15) * 0.15;
    min2_ = min2 = max2_ = max2 = min3_ = min3 = max3_ = max3 = min4_ = min4 = max4_ = max4 = -1;

    sprintf(tmp_cmd, "detRangeCfg -1 %.0f %.0f", min1 / 0.15, max1 / 0.15);
  } else if (min3 < 0 || max3 < 0) {
    min1_ = min1 = round(min1 / 0.15) * 0.15;
    max1_ = max1 = round(max1 / 0.15) * 0.15;
    min2_ = min2 = round(min2 / 0.15) * 0.15;
    max2_ = max2 = round(max2 / 0.15) * 0.15;
    min3_ = min3 = max3_ = max3 = min4_ = min4 = max4_ = max4 = -1;

    sprintf(tmp_cmd, "detRangeCfg -1 %.0f %.0f %.0f %.0f", min1 / 0.15, max1 / 0.15, min2 / 0.15, max2 / 0.15);
  } else if (min4 < 0 || max4 < 0) {
    min1_ = min1 = round(min1 / 0.15) * 0.15;
    max1_ = max1 = round(max1 / 0.15) * 0.15;
    min2_ = min2 = round(min2 / 0.15) * 0.15;
    max2_ = max2 = round(max2 / 0.15) * 0.15;
    min3_ = min3 = round(min3 / 0.15) * 0.15;
    max3_ = max3 = round(max3 / 0.15) * 0.15;
    min4_ = min4 = max4_ = max4 = -1;

    sprintf(tmp_cmd,
            "detRangeCfg -1 "
            "%.0f %.0f %.0f %.0f %.0f %.0f",
            min1 / 0.15, max1 / 0.15, min2 / 0.15, max2 / 0.15, min3 / 0.15, max3 / 0.15);
  } else {
    min1_ = min1 = round(min1 / 0.15) * 0.15;
    max1_ = max1 = round(max1 / 0.15) * 0.15;
    min2_ = min2 = round(min2 / 0.15) * 0.15;
    max2_ = max2 = round(max2 / 0.15) * 0.15;
    min3_ = min3 = round(min3 / 0.15) * 0.15;
    max3_ = max3 = round(max3 / 0.15) * 0.15;
    min4_ = min4 = round(min4 / 0.15) * 0.15;
    max4_ = max4 = round(max4 / 0.15) * 0.15;

    sprintf(tmp_cmd,
            "detRangeCfg -1 "
            "%.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f",
            min1 / 0.15, max1 / 0.15, min2 / 0.15, max2 / 0.15, min3 / 0.15, max3 / 0.15, min4 / 0.15, max4 / 0.15);
  }

  min1_ = min1;
  max1_ = max1;
  min2_ = min2;
  max2_ = max2;
  min3_ = min3;
  max3_ = max3;
  min4_ = min4;
  max4_ = max4;

  cmd_ = std::string(tmp_cmd);
};

uint8_t DetRangeCfgCommand::on_message(std::string &message) {
  if (message == "sensor is not stopped") {
    ESP_LOGE(TAG, "Cannot configure range config. Sensor is not stopped!");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Updated detection area config:");
    ESP_LOGI(TAG, "Detection area 1 from %.02fm to %.02fm.", min1_, max1_);
    if (min2_ >= 0 && max2_ >= 0) {
      ESP_LOGI(TAG, "Detection area 2 from %.02fm to %.02fm.", min2_, max2_);
    }
    if (min3_ >= 0 && max3_ >= 0) {
      ESP_LOGI(TAG, "Detection area 3 from %.02fm to %.02fm.", min3_, max3_);
    }
    if (min4_ >= 0 && max4_ >= 0) {
      ESP_LOGI(TAG, "Detection area 4 from %.02fm to %.02fm.", min4_, max4_);
    }
    ESP_LOGD(TAG, "Used command: %s", cmd_.c_str());
    return 1;  // Command done
  }
  return 0;  // Command not done yet.
}

OutputLatencyCommand::OutputLatencyCommand(float delay_after_detection, float delay_after_disappear) {
  delay_after_detection = round(delay_after_detection / 0.025) * 0.025;
  delay_after_disappear = round(delay_after_disappear / 0.025) * 0.025;
  if (delay_after_detection < 0)
    delay_after_detection = 0;
  if (delay_after_detection > 1638.375)
    delay_after_detection = 1638.375;
  if (delay_after_disappear < 0)
    delay_after_disappear = 0;
  if (delay_after_disappear > 1638.375)
    delay_after_disappear = 1638.375;

  delay_after_detection_ = delay_after_detection;
  delay_after_disappear_ = delay_after_disappear;

  char tmp_cmd[30] = {0};
  sprintf(tmp_cmd, "outputLatency -1 %.0f %.0f", delay_after_detection / 0.025, delay_after_disappear / 0.025);
  cmd_ = std::string(tmp_cmd);
};

uint8_t OutputLatencyCommand::on_message(std::string &message) {
  if (message == "sensor is not stopped") {
    ESP_LOGE(TAG, "Cannot configure output latency. Sensor is not stopped!");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Updated output latency config:");
    ESP_LOGI(TAG, "Signal that someone was detected is delayed by %.02fs.", delay_after_detection_);
    ESP_LOGI(TAG, "Signal that nobody is detected anymore is delayed by %.02fs.", delay_after_disappear_);
    ESP_LOGD(TAG, "Used command: %s", cmd_.c_str());
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

uint8_t SensorCfgStartCommand::on_message(std::string &message) {
  if (message == "sensor is not stopped") {
    ESP_LOGE(TAG, "Cannot configure sensor startup behavior. Sensor is not stopped!");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Updated sensor startup behavior:");
    if (startup_mode_) {
      ESP_LOGI(TAG, "Sensor will start automatically after power-on.");
    } else {
      ESP_LOGI(TAG, "Sensor needs to be started manually after power-on.");
    }
    ESP_LOGD(TAG, "Used command: %s", cmd_.c_str());
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

uint8_t FactoryResetCommand::on_message(std::string &message) {
  if (message == "sensor is not stopped") {
    ESP_LOGE(TAG, "Cannot factory reset. Sensor is not stopped!");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Sensor factory reset done.");
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

uint8_t ResetSystemCommand::on_message(std::string &message) {
  if (message == "leapMMW:/>") {
    ESP_LOGI(TAG, "Restarted sensor.");
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

uint8_t SaveCfgCommand::on_message(std::string &message) {
  if (message == "no parameter has changed") {
    ESP_LOGI(TAG, "Not saving config (no parameter changed).");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Saved config. Saving a lot may damage the sensor.");
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

uint8_t LedModeCommand::on_message(std::string &message) {
  if (message == "sensor is not stopped") {
    ESP_LOGE(TAG, "Cannot set led mode. Sensor is not stopped!");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Set led mode done.");
    if (active_) {
      component_->set_led_active(true);
      ESP_LOGI(TAG, "Sensor LED will blink.");
    } else {
      component_->set_led_active(false);
      ESP_LOGI(TAG, "Turned off LED.");
    }
    ESP_LOGD(TAG, "Used command: %s", cmd_.c_str());
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

uint8_t UartOutputCommand::on_message(std::string &message) {
  if (message == "sensor is not stopped") {
    ESP_LOGE(TAG, "Cannot set uart output mode. Sensor is not stopped!");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Set uart mode done.");
    if (active_) {
      component_->set_uart_presence_active(true);
      ESP_LOGI(TAG, "Presence information is sent via UART and GPIO.");
    } else {
      component_->set_uart_presence_active(false);
      ESP_LOGI(TAG, "Presence information is only sent via GPIO.");
    }
    ESP_LOGD(TAG, "Used command: %s", cmd_.c_str());
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

uint8_t SensitivityCommand::on_message(std::string &message) {
  if (message == "sensor is not stopped") {
    ESP_LOGE(TAG, "Cannot set sensitivity. Sensor is not stopped!");
    return 1;  // Command done
  } else if (message == "Done") {
    ESP_LOGI(TAG, "Set sensitivity done. Set to value %d.", sensitivity_);
    ESP_LOGD(TAG, "Used command: %s", cmd_.c_str());
    return 1;  // Command done
  }
  return 0;  // Command not done yet
}

}  // namespace dfrobot_mmwave_radar
}  // namespace esphome
