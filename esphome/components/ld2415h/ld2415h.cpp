#include "ld2415h.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ld2415h {

static const char *const TAG = "ld2415h";

static const uint8_t LD2415H_CMD_SET_SPEED_ANGLE_SENSE[]    = {0x43, 0x46, 0x01, 0x01, 0x00, 0x05, 0x0d, 0x0a};
static const uint8_t LD2415H_CMD_SET_MODE_RATE_UOM[]        = {0x43, 0x46, 0x02, 0x01, 0x01, 0x00, 0x0d, 0x0a};
static const uint8_t LD2415H_CMD_SET_ANTI_VIB_COMP[]        = {0x43, 0x46, 0x03, 0x05, 0x00, 0x00, 0x0d, 0x0a};
static const uint8_t LD2415H_CMD_SET_RELAY_DURATION_SPEED[] = {0x43, 0x46, 0x04, 0x03, 0x01, 0x00, 0x0d, 0x0a};
static const uint8_t LD2415H_CMD_GET_CONFIG[]               = {0x43, 0x46, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/* TODO ::
     * Create controls that expose settings
*/

LD2415HComponent::LD2415HComponent() 
    : cmd_speed_angle_sense_    {0x43, 0x46, 0x01, 0x01, 0x00, 0x05, 0x0d, 0x0a},
      cmd_mode_rate_uom_        {0x43, 0x46, 0x02, 0x01, 0x01, 0x00, 0x0d, 0x0a},
      cmd_anti_vib_comp_        {0x43, 0x46, 0x03, 0x05, 0x00, 0x00, 0x0d, 0x0a},
      cmd_relay_duration_speed_ {0x43, 0x46, 0x04, 0x03, 0x01, 0x00, 0x0d, 0x0a},
      cmd_config_               {0x43, 0x46, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} {}


void LD2415HComponent::setup() {
  // This triggers current sensor configurations to be dumped
  //this->issue_command_(LD2415H_CMD_GET_CONFIG, sizeof(LD2415H_CMD_GET_CONFIG));
  this->update_config_ = true;
#ifdef USE_NUMBER
  this->min_speed_threshold_number_->publish_state(this->min_speed_threshold_);
  this->compensation_angle_number_->publish_state(this->compensation_angle_);
  this->sensitivity_number_->publish_state(this->sensitivity_);
  this->vibration_correction_number_->publish_state(this->vibration_correction_);
  this->relay_trigger_duration_number_->publish_state(this->relay_trigger_duration_);
  this->relay_trigger_speed_number_->publish_state(this->relay_trigger_speed_);
#endif
#ifdef USE_SELECT
  this->sample_rate_selector_->publish_state(i_to_s_(SAMPLE_RATE_STR_TO_INT, this->sample_rate_));
  this->tracking_mode_selector_->publish_state(i_to_s_(TRACKING_MODE_STR_TO_INT, this->tracking_mode_));
#endif
}

void LD2415HComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "LD2415H:");
  ESP_LOGCONFIG(TAG, "  Firmware: %s", this->firmware_);
  ESP_LOGCONFIG(TAG, "  Minimum Speed Threshold: %u KPH", this->min_speed_threshold_);
  ESP_LOGCONFIG(TAG, "  Compensation Angle: %u", this->compensation_angle_);
  ESP_LOGCONFIG(TAG, "  Sensitivity: %u", this->sensitivity_);
  ESP_LOGCONFIG(TAG, "  Tracking Mode: %s", TrackingMode_to_s_(this->tracking_mode_));
  ESP_LOGCONFIG(TAG, "  Sampling Rate: %u", this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  Unit of Measure: %s", UnitOfMeasure_to_s_(this->unit_of_measure_));
  ESP_LOGCONFIG(TAG, "  Vibration Correction: %u", this->vibration_correction_);
  ESP_LOGCONFIG(TAG, "  Relay Trigger Duration: %u", this->relay_trigger_duration_);
  ESP_LOGCONFIG(TAG, "  Relay Trigger Speed: %u KPH", this->relay_trigger_speed_);
  ESP_LOGCONFIG(TAG, "  Negotiation Mode: %s", NegotiationMode_to_s_(this->negotiation_mode_));

  //LOG_SELECT(TAG, "  Sample Rate", this->sample_rate_);
  //LOG_SELECT(TAG, "  Tracking Mode", this->tracking_mode_);
}

void LD2415HComponent::loop() {
  // Process the stream from the sensor UART
  while (this->available()) {
    if (this->fill_buffer_(this->read())) {
      this->parse_buffer_();
    }
  }

  if(this->update_speed_angle_sense_) {
    ESP_LOGD(TAG, "LD2415H_CMD_SET_SPEED_ANGLE_SENSE: ");
    this->cmd_speed_angle_sense_[3] = this->min_speed_threshold_;
    this->cmd_speed_angle_sense_[4] = this->compensation_angle_;
    this->cmd_speed_angle_sense_[5] = this->sensitivity_;

    this->issue_command_(this->cmd_speed_angle_sense_, sizeof(this->cmd_speed_angle_sense_));
    this->update_speed_angle_sense_ = false;
    return;
  }

  if(this->update_mode_rate_uom_) {
    ESP_LOGD(TAG, "LD2415H_CMD_SET_MODE_RATE_UOM: ");
    this->cmd_mode_rate_uom_[3] = static_cast<uint8_t>(this->tracking_mode_);
    this->cmd_mode_rate_uom_[4] = this->sample_rate_;

    this->issue_command_(this->cmd_mode_rate_uom_, sizeof(this->cmd_mode_rate_uom_));
    this->update_mode_rate_uom_ = false;
    return;
  }

  if(this->update_anti_vib_comp_) {
    ESP_LOGD(TAG, "LD2415H_CMD_SET_ANTI_VIB_COMP: ");
    this->cmd_anti_vib_comp_[3] = this->vibration_correction_;

    this->issue_command_(this->cmd_anti_vib_comp_, sizeof(this->cmd_anti_vib_comp_));
    this->update_anti_vib_comp_ = false;
    return;
  }

  if(this->update_relay_duration_speed_) {
    ESP_LOGD(TAG, "LD2415H_CMD_SET_RELAY_DURATION_SPEED: ");
    this->cmd_relay_duration_speed_[3] = this->relay_trigger_duration_;
    this->cmd_relay_duration_speed_[4] = this->relay_trigger_speed_;

    this->issue_command_(this->cmd_relay_duration_speed_, sizeof(this->cmd_relay_duration_speed_));
    this->update_relay_duration_speed_ = false;
    return;
  }

  if(this->update_config_) {
    ESP_LOGD(TAG, "LD2415H_CMD_GET_CONFIG: ");

    this->issue_command_(this->cmd_config_, sizeof(this->cmd_config_));
    this->update_config_ = false;
    return;
  }
}

void LD2415HComponent::set_min_speed_threshold(uint8_t speed) {  
  this->min_speed_threshold_ = speed;
  this->update_speed_angle_sense_ = true;
}

void LD2415HComponent::set_compensation_angle(uint8_t angle) {
  this->compensation_angle_ = angle;
  this->update_speed_angle_sense_ = true;
}

void LD2415HComponent::set_sensitivity(uint8_t sensitivity) {
  this->sensitivity_ = sensitivity;
  this->update_speed_angle_sense_ = true;
}

void LD2415HComponent::set_tracking_mode(const std::string &state) {
  uint8_t mode = TRACKING_MODE_STR_TO_INT.at(state);
  this->set_tracking_mode(mode);
  this->tracking_mode_selector_->publish_state(state);
}

void LD2415HComponent::set_tracking_mode(TrackingMode mode) {
  this->tracking_mode_ = mode;
  this->update_mode_rate_uom_ = true;
}

void LD2415HComponent::set_tracking_mode(uint8_t mode) {
  this->set_tracking_mode(i_to_TrackingMode_(mode));
}

void LD2415HComponent::set_sample_rate(const std::string &state) {
  uint8_t rate = SAMPLE_RATE_STR_TO_INT.at(state);
  this->set_sample_rate(rate);
  this->sample_rate_selector_->publish_state(state);
}

void LD2415HComponent::set_sample_rate(uint8_t rate) {
  ESP_LOGD(TAG, "set_sample_rate: %i", rate);
  this->sample_rate_ = rate;
  this->update_mode_rate_uom_ = true;
}

void LD2415HComponent::set_vibration_correction(uint8_t correction) {
  this->vibration_correction_ = correction;
  this->update_anti_vib_comp_ = true;
}

void LD2415HComponent::set_relay_trigger_duration(uint8_t duration) {
  this->relay_trigger_duration_ = duration;
  this->update_relay_duration_speed_ = true;
}

void LD2415HComponent::set_relay_trigger_speed(uint8_t speed) {
  this->relay_trigger_speed_ = speed;
  this->update_relay_duration_speed_ = true;
}

void LD2415HComponent::issue_command_(const uint8_t cmd[], const uint8_t size) {
  for(uint8_t i = 0; i < size; i++)
    ESP_LOGD(TAG, "  0x%02x", cmd[i]);

  // Don't assume the response buffer is empty, clear it before issuing a command.
  clear_remaining_buffer_(0);
  this->write_array(cmd, size);
}

bool LD2415HComponent::fill_buffer_(char c) {
  switch(c) {
    case 0x00:
    case 0xFF:
    case '\r':
      // Ignore these characters
      break;

    case '\n':
      // End of response
      if(this->response_buffer_index_ == 0)
        break;

      clear_remaining_buffer_(this->response_buffer_index_);
      ESP_LOGV(TAG, "Response Received:: %s", this->response_buffer_);
      return true;

    default:
      // Append to response
      this->response_buffer_[this->response_buffer_index_] = c;
      this->response_buffer_index_++;
      break;
  }

  return false;
}

void LD2415HComponent::clear_remaining_buffer_(uint8_t pos) {
  while(pos < sizeof(this->response_buffer_)) {
        this->response_buffer_[pos] = 0x00;
        pos++;
  }

  this->response_buffer_index_ = 0;
}

void LD2415HComponent::parse_buffer_() {
  char c = this->response_buffer_[0];

  switch(c) {
    case 'N':
      // Firmware Version
      this->parse_firmware_();
      break;
    case 'X':
      // Config Response
      this->parse_config_();
      break;
    case 'V':
      // Speed
      this->parse_speed_();
      break;

    default:
      ESP_LOGE(TAG, "Unknown Response: %s", this->response_buffer_);
      break;
  }
}

void LD2415HComponent::parse_config_() {
  // Example: "X1:01 X2:00 X3:05 X4:01 X5:00 X6:00 X7:05 X8:03 X9:01 X0:01"

  const char* delim = ": ";
  uint8_t token_len = 2;
  char* key;
  char* val;

  char* token = strtok(this->response_buffer_, delim);
  
  while (token != NULL)
  {
    if(std::strlen(token) != token_len) {
      ESP_LOGE(TAG, "Configuration key length invalid.");
      break;
    }
    key = token;

    token = strtok(NULL, delim);
    if(std::strlen(token) != token_len) {
      ESP_LOGE(TAG, "Configuration value length invalid.");
      break;
    }
    val = token;
    
    this->parse_config_param_(key, val);

    token = strtok(NULL, delim);
  }

  ESP_LOGD(TAG, "Configuration received:");
  ESP_LOGCONFIG(TAG, "LD2415H:");
  ESP_LOGCONFIG(TAG, "  Firmware: %s", this->firmware_);
  ESP_LOGCONFIG(TAG, "  Minimum Speed Threshold: %u KPH", this->min_speed_threshold_);
  ESP_LOGCONFIG(TAG, "  Compensation Angle: %u", this->compensation_angle_);
  ESP_LOGCONFIG(TAG, "  Sensitivity: %u", this->sensitivity_);
  ESP_LOGCONFIG(TAG, "  Tracking Mode: %s", TrackingMode_to_s_(this->tracking_mode_));
  ESP_LOGCONFIG(TAG, "  Sampling Rate: %u", this->sample_rate_);
  ESP_LOGCONFIG(TAG, "  Unit of Measure: %s", UnitOfMeasure_to_s_(this->unit_of_measure_));
  ESP_LOGCONFIG(TAG, "  Vibration Correction: %u", this->vibration_correction_);
  ESP_LOGCONFIG(TAG, "  Relay Trigger Duration: %u", this->relay_trigger_duration_);
  ESP_LOGCONFIG(TAG, "  Relay Trigger Speed: %u KPH", this->relay_trigger_speed_);
  ESP_LOGCONFIG(TAG, "  Negotiation Mode: %s", NegotiationMode_to_s_(this->negotiation_mode_));

}

void LD2415HComponent::parse_firmware_() {
  // Example: "No.:20230801E v5.0"

  const char* fw = strchr(this->response_buffer_, ':');

  if (fw != nullptr) {
    // Move p to the character after ':'
    ++fw;

    // Copy string into firmware
    std::strcpy(this->firmware_, fw);
  } else {
    ESP_LOGE(TAG, "Firmware value invalid.");
  }
}

void LD2415HComponent::parse_speed_() {
  // Example: "V+001.9"

  const char* p = strchr(this->response_buffer_, 'V');

  if (p != nullptr) {
    ++p;
    this->approaching_ = (*p == '+');
    ++p;
    this->speed_ = atof(p);

    ESP_LOGV(TAG, "Speed updated: %f KPH", this->speed_);

    for (auto &listener : this->listeners_) {
      listener->on_speed(this->speed_);
      //listener->on_approaching(this->approaching_);
    }
    
    if (this->speed_sensor_ != nullptr)
      this->speed_sensor_->publish_state(this->speed_);

  } else {
    ESP_LOGE(TAG, "Firmware value invalid.");
  }
}

void LD2415HComponent::parse_config_param_(char* key, char* value) {
  if(std::strlen(key) != 2 || std::strlen(value) != 2 || key[0] != 'X') {
      ESP_LOGE(TAG, "Invalid Parameter %s:%s", key, value);
      return;
  }

  uint8_t v = std::stoi(value, nullptr, 16);

  switch(key[1]) {
    case '1':
      this->min_speed_threshold_ = v;
      break;
    case '2':
      this->compensation_angle_ = std::stoi(value, nullptr, 16);
      break;
    case '3':
      this->sensitivity_ = std::stoi(value, nullptr, 16);
      break;
    case '4':
      this->tracking_mode_ = this->i_to_TrackingMode_(v);
      break;
    case '5':
      this->sample_rate_ = v;
      break;
    case '6':
      this->unit_of_measure_ = this->i_to_UnitOfMeasure_(v);
      break;
    case '7':
      this->vibration_correction_ = v;
      break;
    case '8':
      this->relay_trigger_duration_ = v;
      break;
    case '9':
      this->relay_trigger_speed_ = v;
      break;
    case '0':
      this->negotiation_mode_ = this->i_to_NegotiationMode_(v);
      break;
    default:
      ESP_LOGD(TAG, "Unknown Parameter %s:%s", key, value);
      break;
  }
}

TrackingMode LD2415HComponent::i_to_TrackingMode_(uint8_t value) {
  TrackingMode u = TrackingMode(value);
  ESP_LOGD(TAG, "i_to_TrackingMode_: %i, %i", value, static_cast<uint8_t>(u));
  switch (u)
  {
    case TrackingMode::APPROACHING_AND_RETREATING:
      return TrackingMode::APPROACHING_AND_RETREATING;
    case TrackingMode::APPROACHING:
      return TrackingMode::APPROACHING;
    case TrackingMode::RETREATING:
      return TrackingMode::RETREATING;
    default:
      ESP_LOGE(TAG, "Invalid TrackingMode:%u", value);
      return TrackingMode::APPROACHING_AND_RETREATING;
  }
}

UnitOfMeasure LD2415HComponent::i_to_UnitOfMeasure_(uint8_t value) {
  UnitOfMeasure u = UnitOfMeasure(value);
  switch (u)
  {
    case UnitOfMeasure::MPS:
      return UnitOfMeasure::MPS;
    case UnitOfMeasure::MPH:
      return UnitOfMeasure::MPH;
    case UnitOfMeasure::KPH:
      return UnitOfMeasure::KPH;
    default:
      ESP_LOGE(TAG, "Invalid UnitOfMeasure:%u", value);
      return UnitOfMeasure::KPH;
  }
}

NegotiationMode LD2415HComponent::i_to_NegotiationMode_(uint8_t value) {
  NegotiationMode u = NegotiationMode(value);
  
  switch (u)
  {
    case NegotiationMode::CUSTOM_AGREEMENT:
      return NegotiationMode::CUSTOM_AGREEMENT;
    case NegotiationMode::STANDARD_PROTOCOL:
      return NegotiationMode::STANDARD_PROTOCOL;
    default:
      ESP_LOGE(TAG, "Invalid UnitOfMeasure:%u", value);
      return NegotiationMode::CUSTOM_AGREEMENT;
  }
}

const char* LD2415HComponent::TrackingMode_to_s_(TrackingMode value) {
  switch (value)
  {
    case TrackingMode::APPROACHING_AND_RETREATING:
      return "APPROACHING_AND_RETREATING";
    case TrackingMode::APPROACHING:
      return "APPROACHING";
    case TrackingMode::RETREATING:
    default:
      return "RETREATING";
  }
}

const char* LD2415HComponent::UnitOfMeasure_to_s_(UnitOfMeasure value) {
  switch (value)
  {
    case UnitOfMeasure::MPS:
      return "MPS";
    case UnitOfMeasure::MPH:
      return "MPH";
    case UnitOfMeasure::KPH:
    default:
      return "KPH";
  }
}

const char* LD2415HComponent::NegotiationMode_to_s_(NegotiationMode value) {
  switch (value)
  {
    case NegotiationMode::CUSTOM_AGREEMENT:
      return "CUSTOM_AGREEMENT";
    case NegotiationMode::STANDARD_PROTOCOL:
    default:
      return "STANDARD_PROTOCOL";
  }
}

const char* i_to_s_(std::map<std::string, uint8_t> map, uint8_t i) {
  for (const auto& pair : map) {
        if (pair.second == i) {
            return pair.first;
        }
    }
    return "Unknown";
}

}  // namespace ld2415h
}  // namespace esphome