#include "dfrobot_c4001.h"

#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace dfrobot_c4001 {
static const char *const TAG = "dfrobot_c4001";
const char ASCII_CR = 0x0D;
const char ASCII_LF = 0x0A;

static inline const char *mode_to_str(DFRobotMode mode) {
  switch (mode) {
    case MODE_PRESENCE:
      return "PRESENCE";
    case MODE_SPEED_AND_DISTANCE:
      return "SPEED_AND_DISTANCE";
    default:
      return "UNKNOWN";
  }
}

static inline const char *model_to_str(DFRobotModel model) {
  switch (model) {
    case MODEL_SEN0609:
      return "SEN0609";
    case MODEL_SEN0610:
      return "SEN0610";
    default:
      return "UNKNOWN";
  }
}

void DFRobotC4001Hub::set_occupancy(bool occupancy) {
  this->occupancy_ = occupancy;
#ifdef USE_BINARY_SENSOR
  if (this->occupancy_binary_sensor_ != nullptr) {
    this->occupancy_binary_sensor_->publish_state(occupancy);
  }
#endif
}

void DFRobotC4001Hub::set_target_distance(float value) {
  this->target_distance_ = value;
#ifdef USE_SENSOR
  if (this->target_distance_sensor_ != nullptr) {
    this->target_distance_sensor_->publish_state(value);
  }
#endif
}

void DFRobotC4001Hub::set_target_speed(float value) {
  this->target_speed_ = value;
#ifdef USE_SENSOR
  if (this->target_speed_sensor_ != nullptr) {
    this->target_speed_sensor_->publish_state(value);
  }
#endif
}

void DFRobotC4001Hub::set_target_energy(float value) {
  this->target_energy_ = value;
#ifdef USE_SENSOR
  if (this->target_energy_sensor_ != nullptr) {
    this->target_energy_sensor_->publish_state(value);
  }
#endif
}

void DFRobotC4001Hub::set_max_range(float max, bool needs_save) {
#ifdef USE_NUMBER
  if (this->max_range_number_ != nullptr) {
    this->max_range_number_->publish_state(max);
  }
  if (this->min_range_number_ != nullptr) {
    if (this->min_range_ > max) {
      this->min_range_ = max;
      this->min_range_number_->publish_state(max);
    }
  }
  if (this->trigger_range_number_ != nullptr) {
    if (this->trigger_range_ > max) {
      this->trigger_range_ = max;
      this->trigger_range_number_->publish_state(max);
    }
  }
#endif
  this->max_range_ = max;
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_min_range(float min, bool needs_save) {
#ifdef USE_NUMBER
  if (this->min_range_number_ != nullptr) {
    this->min_range_number_->publish_state(min);
  }
  if (this->max_range_number_ != nullptr) {
    if (this->max_range_ < min) {
      this->max_range_ = min;
      this->max_range_number_->publish_state(min);
    }
  }
  if (this->trigger_range_number_ != nullptr) {
    if (this->trigger_range_ < min) {
      this->trigger_range_ = min;
      this->trigger_range_number_->publish_state(min);
    }
  }
#endif
  this->min_range_ = min;
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_trigger_range(float trig, bool needs_save) {
#ifdef USE_NUMBER
  if (this->trigger_range_number_ != nullptr) {
    this->trigger_range_number_->publish_state(trig);
  }
  if (this->min_range_number_ != nullptr) {
    if (this->min_range_ > trig) {
      this->min_range_ = trig;
      this->min_range_number_->publish_state(trig);
    }
  }
  if (this->max_range_number_ != nullptr) {
    if (this->max_range_ < trig) {
      this->max_range_ = trig;
      this->max_range_number_->publish_state(trig);
    }
  }
#endif
  this->trigger_range_ = trig;
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_hold_sensitivity(float value, bool needs_save) {
  this->hold_sensitivity_ = value;
#ifdef USE_NUMBER
  if (this->hold_sensitivity_number_ != nullptr) {
    this->hold_sensitivity_number_->publish_state(value);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_trigger_sensitivity(float value, bool needs_save) {
  this->trigger_sensitivity_ = value;
#ifdef USE_NUMBER
  if (this->trigger_sensitivity_number_ != nullptr) {
    this->trigger_sensitivity_number_->publish_state(value);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_on_latency(float value, bool needs_save) {
  this->on_latency_ = value;
#ifdef USE_NUMBER
  if (this->on_latency_number_ != nullptr) {
    this->on_latency_number_->publish_state(value);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_off_latency(float value, bool needs_save) {
  this->off_latency_ = value;
#ifdef USE_NUMBER
  if (this->off_latency_number_ != nullptr) {
    this->off_latency_number_->publish_state(value);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_inhibit_time(float value, bool needs_save) {
  this->inhibit_time_ = value;
#ifdef USE_NUMBER
  if (this->inhibit_time_number_ != nullptr) {
    this->inhibit_time_number_->publish_state(value);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_threshold_factor(float value, bool needs_save) {
  this->threshold_factor_ = value;
#ifdef USE_NUMBER
  if (this->threshold_factor_number_ != nullptr) {
    this->threshold_factor_number_->publish_state(this->threshold_factor_);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_out_led_enable(bool value, bool needs_save) {
  this->out_led_enable_ = value;
#ifdef USE_SWITCH
  if (this->out_led_enable_switch_ != nullptr) {
    this->out_led_enable_switch_->publish_state(value);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_run_led_enable(bool value, bool needs_save) {
  this->run_led_enable_ = value;
#ifdef USE_SWITCH
  if (this->run_led_enable_switch_ != nullptr) {
    this->run_led_enable_switch_->publish_state(value);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_micro_motion_enable(bool enable, bool needs_save) {
  this->micro_motion_enable_ = enable;
#ifdef USE_SWITCH
  if (this->micro_motion_enable_switch_ != nullptr) {
    this->micro_motion_enable_switch_->publish_state(enable);
  }
#endif
  if (needs_save) {
    this->set_needs_save(true);
  }
}

void DFRobotC4001Hub::set_software_version(char *version) {
  std::string new_string(version);
#ifdef USE_TEXT_SENSOR
  if (this->software_version_text_sensor_ != nullptr) {
    this->software_version_text_sensor_->publish_state(new_string);
  }
#endif
  this->sw_version_ = std::move(new_string);
}

void DFRobotC4001Hub::set_hardware_version(char *version) {
  std::string new_string(version);
  if (str_startswith(new_string, "JYSJ_428")) {
    this->hw_model_ = MODEL_SEN0609;
  } else if (str_startswith(new_string, "JYSJ_426")) {
    this->hw_model_ = MODEL_SEN0610;
  } else {
    this->hw_model_ = MODEL_UNKNOWN;
  }
#ifdef USE_TEXT_SENSOR
  if (this->hardware_version_text_sensor_ != nullptr) {
    this->hardware_version_text_sensor_->publish_state(new_string);
  }
#endif
  this->hw_version_ = std::move(new_string);
}

void DFRobotC4001Hub::flash_run_led_enable() {
#ifdef USE_SWITCH
  // Save LED Enable preferences (to flash storage)
  if (this->run_led_enable_switch_ != nullptr) {
    ESP_LOGD(TAG, "Writing RUN LED Enable setting to flash");
    this->pref_.save(&this->run_led_enable_);
  }
#endif
}

void DFRobotC4001Hub::set_mode(DFRobotMode value) { this->mode_ = value; }

void DFRobotC4001Hub::set_model(DFRobotModel value) { this->model_ = value; }

void DFRobotC4001Hub::set_needs_save(bool needs_save) {
  this->needs_save_ = needs_save;
#ifdef USE_BINARY_SENSOR
  if (this->config_changed_binary_sensor_ != nullptr) {
    this->config_changed_binary_sensor_->publish_state(needs_save);
  }
#endif
}

// initial setup of module
void DFRobotC4001Hub::setup_module() {
#ifdef USE_NUMBER
  if (this->model_ == MODEL_SEN0610) {
    if (this->min_range_number_ != nullptr) {
      this->min_range_number_->traits.set_max_value(12.0);
    }
    if (this->max_range_number_ != nullptr) {
      this->max_range_number_->traits.set_max_value(12.0);
    }
    if (this->trigger_range_number_ != nullptr) {
      this->trigger_range_number_->traits.set_max_value(12.0);
    }
  }
#endif
  // stop the module so that configuration can be set
  this->enqueue(make_unique<PowerCommand>(false));
  // put the module is the requested mode
  this->enqueue(make_unique<SetRunAppCommand>(this->mode_));
  // the module doesn't remember the RUN led state so it need to be set here
  // this->run_led_enable_ comes from flash which does remember between power cycles
  this->enqueue(make_unique<SetLedModeCommand>(this->run_led_enable_));
  // the module remembers the OUT led state so it does not need to be set here
  // make sure the module output presence via uart
  if (this->mode_ == MODE_PRESENCE) {
    this->enqueue(make_unique<SetUartOutputCommand>(true));
  }
  // start the module
  this->enqueue(make_unique<PowerCommand>(true));
}

void DFRobotC4001Hub::config_load() {
  // get dfrobot_c4001 current configuration
  // have to be in the right mode to read that mode's parameters
  this->enqueue(make_unique<GetHWVCommand>());
  this->enqueue(make_unique<GetSWVCommand>());
  // the module doesn't remember the RUN led state (controlled by LedMode command) so loading from module won't work
  // the OUT led state is controlled by GpioMode command and it is remembered by the module so loading does work
  this->enqueue(make_unique<GetGpioModeCommand>());
#ifdef USE_NUMBER
  if (this->min_range_number_ != nullptr)
    this->enqueue(make_unique<GetRangeCommand>());
#endif
  if (this->mode_ == MODE_PRESENCE) {
#ifdef USE_NUMBER
    if (this->trigger_range_number_ != nullptr)
      this->enqueue(make_unique<GetTrigRangeCommand>());
    if (this->hold_sensitivity_number_ != nullptr)
      this->enqueue(make_unique<GetSensitivityCommand>());
    if (this->on_latency_number_ != nullptr)
      this->enqueue(make_unique<GetLatencyCommand>());
    if (this->inhibit_time_number_ != nullptr)
      this->enqueue(make_unique<GetInhibitTimeCommand>());
#endif
  } else {
#ifdef USE_NUMBER
    if (this->threshold_factor_number_ != nullptr)
      this->enqueue(make_unique<GetThrFactorCommand>());
#endif
#ifdef USE_SWITCH
    if (this->micro_motion_enable_switch_ != nullptr)
      this->enqueue(make_unique<GetMicroMotionCommand>());
#endif
  }
  this->set_needs_save(false);
}

void DFRobotC4001Hub::config_save() {
  if (this->needs_save_) {
    this->flash_run_led_enable();
    this->enqueue(make_unique<PowerCommand>(false));
    this->enqueue(make_unique<SetLedModeCommand>(this->run_led_enable_));
    this->enqueue(make_unique<SetGpioModeCommand>(this->out_led_enable_));
#ifdef USE_NUMBER
    if (this->min_range_number_ != nullptr)
      this->enqueue(make_unique<SetRangeCommand>(this->min_range_, this->max_range_));
#endif
    if (this->mode_ == MODE_PRESENCE) {
#ifdef USE_NUMBER
      if (this->trigger_range_number_ != nullptr)
        this->enqueue(make_unique<SetTrigRangeCommand>(this->trigger_range_));
      if (this->hold_sensitivity_number_ != nullptr)
        this->enqueue(make_unique<SetSensitivityCommand>(this->hold_sensitivity_, this->trigger_sensitivity_));
      if (this->on_latency_number_ != nullptr)
        this->enqueue(make_unique<SetLatencyCommand>(this->on_latency_, this->off_latency_));
      if (this->inhibit_time_number_ != nullptr)
        this->enqueue(make_unique<SetInhibitTimeCommand>(this->inhibit_time_));
#endif
    } else {
#ifdef USE_NUMBER
      if (this->threshold_factor_number_ != nullptr)
        this->enqueue(make_unique<SetThrFactorCommand>(this->threshold_factor_));
#endif
#ifdef USE_SWITCH
      if (this->micro_motion_enable_switch_ != nullptr)
        this->enqueue(make_unique<SetMicroMotionCommand>(this->micro_motion_enable_));
#endif
    }
    this->enqueue(make_unique<SaveCfgCommand>());
    this->enqueue(make_unique<ResetSystemCommand>(true));
    this->enqueue(make_unique<PowerCommand>(true));
    this->set_needs_save(false);
  }
}

void DFRobotC4001Hub::factory_reset() {
  ESP_LOGD(TAG, "Factory Reset Started");
  this->enqueue(make_unique<PowerCommand>(false));
  this->enqueue(make_unique<FactoryResetCommand>());
}

void DFRobotC4001Hub::restart() {
  ESP_LOGD(TAG, "Restart Started");
  this->enqueue(make_unique<PowerCommand>(false));
  this->enqueue(make_unique<ResetSystemCommand>(true));
}

void DFRobotC4001Hub::dump_config() {
  ESP_LOGCONFIG(TAG,
                "DFRobot C4001 mmWave Radar:\n"
                "  SW Version: %s\n"
                "  HW Version: %s\n"
                "  Model: %s\n"
                "  Mode: %s\n",
                this->sw_version_.c_str(), this->hw_version_.c_str(), model_to_str(this->model_),
                mode_to_str(this->mode_));
  bool models_good = this->model_ == this->hw_model_;
  if (this->hw_model_ == MODEL_UNKNOWN) {
    models_good = true;
  }
  if (!models_good) {
    ESP_LOGW(TAG, "HW Version indicates a %s, could be an issue", model_to_str(this->model_));
  }
#ifdef USE_BUTTON
  ESP_LOGCONFIG(TAG, "Buttons:");
  LOG_BUTTON("  ", "Config Save", this->config_save_button_);
  LOG_BUTTON("  ", "Restart", this->restart_button_);
  LOG_BUTTON("  ", "Factory Reset", this->factory_reset_button_);
#endif
#ifdef USE_BINARY_SENSOR
  ESP_LOGCONFIG(TAG, "Binary Sensors:");
  LOG_BINARY_SENSOR("  ", "Occupancy", this->occupancy_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Config Changed", this->config_changed_binary_sensor_);
#endif
#ifdef USE_NUMBER
  ESP_LOGCONFIG(TAG, "Numbers:");
  LOG_NUMBER("  ", "Maximum Range", this->max_range_number_);
  LOG_NUMBER("  ", "Minimum Range", this->min_range_number_);
  LOG_NUMBER("  ", "Trigger Range", this->trigger_range_number_);
  LOG_NUMBER("  ", "Hold Sensitivity", this->hold_sensitivity_number_);
  LOG_NUMBER("  ", "Trigger Sensitivity", this->trigger_sensitivity_number_);
  LOG_NUMBER("  ", "On Latency", this->on_latency_number_);
  LOG_NUMBER("  ", "Off Latency", this->off_latency_number_);
  LOG_NUMBER("  ", "Inhibit Time", this->inhibit_time_number_);
  LOG_NUMBER("  ", "Threshold Factor", this->threshold_factor_number_);
#endif
#ifdef USE_SWITCH
  ESP_LOGCONFIG(TAG, "Switches:");
  LOG_SWITCH("  ", "RUN LED Enable", this->run_led_enable_switch_);
  LOG_SWITCH("  ", "OUT LED Enable", this->out_led_enable_switch_);
  LOG_SWITCH("  ", "Micro Motion Enable", this->micro_motion_enable_switch_);
#endif
#ifdef USE_TEXT_SENSOR
  ESP_LOGCONFIG(TAG, "Text Sensors:");
  LOG_TEXT_SENSOR("  ", "Software Version", this->software_version_text_sensor_);
  LOG_TEXT_SENSOR("  ", "Hardware Version", this->hardware_version_text_sensor_);
#endif
}

void DFRobotC4001Hub::setup() {
  bool value;

#ifdef USE_SWITCH
  if (this->run_led_enable_switch_ != nullptr) {
    // Restore LED Enable preferences (from flash storage)
    this->pref_ = global_preferences->make_preference<bool>(this->run_led_enable_switch_->get_object_id_hash());
    if (!this->pref_.load(&value)) {
      ESP_LOGCONFIG(TAG, "Defaulting flash settings");
      value = false;
    } else {
      ESP_LOGCONFIG(TAG, "Load flash settings");
    }
    this->set_run_led_enable(value, false);
  }
#endif
  // setup the module
  this->enqueue(make_unique<ResetSystemCommand>(false));
  this->setup_module();
  this->config_load();
}

void DFRobotC4001Hub::loop() {
  if (this->is_failed()) {
    return;
  }
  if (this->cmd_queue_.is_empty()) {
    // Command queue empty, first time this happens setup is complete
    if (!this->is_setup_) {
      this->is_setup_ = true;
      if (this->ts_cmd_error_cnt_ > 3) {
        this->mark_failed("Too many errors");
      }
    }
    // Read sensor state
    this->enqueue(make_unique<ReadStateCommand>());
  }
  // Commands are non-blocking and need to be called repeatedly.
  int8_t result = this->cmd_queue_.process(this);
  if (result) {
    if (this->cmd_queue_.is_retry_power_stop()) {
      // add PowerCommand to the beginning of the queue to stop the sensor
      this->enqueue(make_unique<PowerCommand>(false), true);
      ESP_LOGV(TAG, "Queue: Retrying command after stopping sensor");
    } else {
      // negative result means errors occurred magnitude is number of errors
      if (result < 0) {
        this->ts_cmd_error_cnt_ -= result;
        ESP_LOGV(TAG, "Queue: Error Count: %d", this->ts_cmd_error_cnt_);
      }
      // dequeue the command
      this->cmd_queue_.dequeue();
    }
  }
}

int8_t DFRobotC4001Hub::enqueue(std::unique_ptr<Command> cmd, bool first) {
  return this->cmd_queue_.enqueue(std::move(cmd), first);  // Transfer ownership using std::move
}

uint8_t DFRobotC4001Hub::read_message_() {
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

    if (byte == '>') {
      this->read_buffer_[++this->read_pos_] = ASCII_LF;
    }

    if (this->read_buffer_[this->read_pos_] == ASCII_LF) {
      this->read_buffer_[this->read_pos_] = 0;
      this->read_pos_ = 0;
#ifdef ESPHOME_LOG_HAS_VERBOSE
      std::string message(this->read_buffer_);
      if (!(str_startswith(message, "$DFHPD")) && !(str_startswith(message, "$DFDMD")))
        ESP_LOGV(TAG, "Recv Msg: %s", this->read_buffer_);
#endif
      // ESP_LOGV(TAG, "Recv Msg: %s", this->read_buffer_);
      return true;  // Full message in buffer
    } else {
      this->read_pos_++;
    }
  }
  return false;  // No full message yet
}

uint8_t DFRobotC4001Hub::send_cmd_(const char *cmd, uint32_t duration) {
  // The interval between two commands must be larger than the specified duration (in ms).
  if (millis() - this->ts_last_cmd_sent_ > duration) {
    this->write_str(cmd);
    this->write_byte(ASCII_CR);
    this->write_byte(ASCII_LF);
    this->ts_last_cmd_sent_ = millis();
    ESP_LOGV(TAG, "Send Cmd: %s", cmd);
    return true;  // Command sent
  }
  // Could not send command yet as command duration did not fully pass yet.
  return false;
}

int8_t CircularCommandQueue::enqueue(std::unique_ptr<Command> cmd, bool first) {
  if (this->is_full()) {
    ESP_LOGE(TAG, "Command queue is full");
    return -1;
  }
  if (first) {
    if (this->is_empty())
      this->rear_ = 0;
    this->front_ = (this->front_ - 1) % COMMAND_QUEUE_SIZE;
    this->commands_[this->front_] = std::move(cmd);  // Transfer ownership using std::move
    return 1;
  } else {
    if (this->is_empty())
      this->front_ = 0;
    this->rear_ = (this->rear_ + 1) % COMMAND_QUEUE_SIZE;
    this->commands_[this->rear_] = std::move(cmd);  // Transfer ownership using std::move
    return 1;
  }
}

bool CircularCommandQueue::is_retry_power_stop() {
  if (this->is_empty()) {
    return false;
  } else {
    return this->commands_[this->front_]->retry_power_stop;
  }
}

std::unique_ptr<Command> CircularCommandQueue::dequeue() {
  if (this->is_empty())
    return nullptr;
  std::unique_ptr<Command> dequeued_cmd = std::move(this->commands_[this->front_]);
  if (this->front_ == this->rear_) {
    this->front_ = -1;
    this->rear_ = -1;
  } else {
    this->front_ = (this->front_ + 1) % COMMAND_QUEUE_SIZE;
  }
  return dequeued_cmd;
}

bool CircularCommandQueue::is_empty() { return this->front_ == -1; }

// queue is full when there is 1 space left
bool CircularCommandQueue::is_full() { return (this->rear_ + 2) % COMMAND_QUEUE_SIZE == this->front_; }

// Run execute method of first in line command.
// Execute is non-blocking and has to be called until it returns 1.
int8_t CircularCommandQueue::process(DFRobotC4001Hub *parent) {
  if (!is_empty()) {
    return this->commands_[this->front_]->execute(parent);
  } else {
    return true;
  }
}

}  // namespace dfrobot_c4001
}  // namespace esphome
