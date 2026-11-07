#include "ezo.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/progmem.h"

namespace esphome::ezo {

PROGMEM_STRING_TABLE(EzoCommandTypeStrings, "EZO_READ", "EZO_LED", "EZO_DEVICE_INFORMATION", "EZO_SLOPE",
                     "EZO_CALIBRATION", "EZO_SLEEP", "EZO_I2C", "EZO_T", "EZO_CUSTOM");

// Indexed by EzoCommandId, up to I2C; ids from CAL_LOW to I2C get their value appended
PROGMEM_STRING_TABLE(EzoCommandTextStrings, "i", "Sleep", "Slope,?", "T,?", "Cal,?", "Cal,clear", "L,?", "L,0", "L,1",
                     "R", "Cal,LOW,", "Cal,MID,", "Cal,HIGH,", "Cal,", "T,", "I2C,");
static_assert(EzoCommandTextStrings::COUNT == static_cast<size_t>(EzoCommandId::EZO_COMMAND_ID_CUSTOM));
static_assert(EzoCommandTextStrings::MAX_LENGTH <= EZO_MAX_COMMAND_LENGTH);

// Indexed by EzoCommandId
static constexpr uint8_t REPLY_TYPES[] PROGMEM = {
    EZO_DEVICE_INFORMATION,
    EZO_SLEEP,
    EZO_SLOPE,
    EZO_T,
    EZO_CALIBRATION,
    EZO_CALIBRATION,
    EZO_LED,
    EZO_LED,
    EZO_LED,
    EZO_READ,
    EZO_CALIBRATION,
    EZO_CALIBRATION,
    EZO_CALIBRATION,
    EZO_CALIBRATION,
    EZO_T,
    EZO_I2C,
    EZO_CUSTOM,
};
static_assert(sizeof(REPLY_TYPES) == static_cast<size_t>(EzoCommandId::EZO_COMMAND_ID_CUSTOM) + 1);

static EzoCommandType reply_type(EzoCommandId id) {
  return static_cast<EzoCommandType>(progmem_read_byte(&REPLY_TYPES[static_cast<uint8_t>(id)]));
}

static constexpr uint32_t LONG_DELAY_MS = 900;  // reads and calibration points
static constexpr uint32_t COMMAND_DELAY_MS = 300;
static constexpr size_t MAX_REPLY_LENGTH = 52;

static uint32_t reply_delay_ms(EzoCommandId id) {
  return id >= EzoCommandId::EZO_COMMAND_ID_READ && id <= EzoCommandId::EZO_COMMAND_ID_CAL_GENERIC ? LONG_DELAY_MS
                                                                                                   : COMMAND_DELAY_MS;
}

// Writes a built-in command's text and returns its length. Values get two decimals and the I2C address none;
// the other commands ignore the value.
static size_t render_command(EzoCommandId id, float value, char *text) {
  const int length = ESPHOME_snprintf_P(
      text, EZO_MAX_COMMAND_LENGTH + 1,
      id >= EzoCommandId::EZO_COMMAND_ID_CAL_LOW ? ESPHOME_PSTR("%s%.*f") : ESPHOME_PSTR("%s"),
      LOG_STR_ARG(EzoCommandTextStrings::get_log_str(static_cast<uint8_t>(id), EzoCommandTextStrings::LAST_INDEX)),
      id == EzoCommandId::EZO_COMMAND_ID_I2C ? 0 : 2, value);
  return length < 0 ? 0 : std::min(static_cast<size_t>(length), EZO_MAX_COMMAND_LENGTH);
}

void EZOSensor::dump_config() {
  LOG_SENSOR("", "EZO", this);
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
}

void EZOSensor::update() {
  this->read_pending_ = true;
  this->enable_loop();
}

void EZOSensor::send_next_() {
  const uint32_t now = App.get_loop_component_start_time();
  const bool after_read = this->state_ == EzoState::EZO_STATE_IDLE_AFTER_READ;
  // A read goes right after the command in flight, but never twice in a row while others wait.
  if (this->read_pending_ && (!after_read || this->queue_.empty())) {
    ESP_LOGVV(TAG, "Sending command \"R\"");
    static constexpr uint8_t READ_COMMAND = 'R';
    this->write(&READ_COMMAND, 1);
    this->read_pending_ = false;
    this->state_ = EzoState::EZO_STATE_SENT_READ;
    this->reply_due_ = now + LONG_DELAY_MS;
    return;
  }
  if (this->queue_.empty()) {
    this->disable_loop();
    return;
  }

  const EzoCommand &command = this->queue_.front();
  const EzoCommandId id = command.id;
  char text[EZO_MAX_COMMAND_LENGTH + 1];
  size_t length;
  if (id == EzoCommandId::EZO_COMMAND_ID_CUSTOM) {
    length = command.text_length;
    const size_t first = std::min<size_t>(length, EZO_CUSTOM_TEXT_SIZE - this->custom_text_start_);
    memcpy(text, this->custom_text_ + this->custom_text_start_, first);
    memcpy(text + first, this->custom_text_, length - first);
    text[length] = '\0';
  } else {
    length = render_command(id, command.value, text);
  }
  ESP_LOGVV(TAG, "Sending command \"%s\"", text);
  this->write(reinterpret_cast<const uint8_t *>(text), length);

  if (id == EzoCommandId::EZO_COMMAND_ID_SLEEP || id == EzoCommandId::EZO_COMMAND_ID_I2C) {
    // No reply; each queued address change applies its own target
    if (id == EzoCommandId::EZO_COMMAND_ID_I2C)
      this->address_ = static_cast<uint8_t>(command.value);
    this->queue_.pop();
    this->state_ = EzoState::EZO_STATE_IDLE;
    return;
  }

  this->state_ = EzoState::EZO_STATE_SENT_QUEUED;
  this->reply_due_ = now + reply_delay_ms(id);
}

void EZOSensor::finish_command_() {
  if (this->state_ == EzoState::EZO_STATE_SENT_READ) {
    this->state_ = EzoState::EZO_STATE_IDLE_AFTER_READ;
    return;
  }
  const EzoCommand &command = this->queue_.front();
  if (command.id == EzoCommandId::EZO_COMMAND_ID_CUSTOM) {
    this->custom_text_start_ = (this->custom_text_start_ + command.text_length) % EZO_CUSTOM_TEXT_SIZE;
    this->custom_text_used_ -= command.text_length;
  }
  this->queue_.pop();
  this->state_ = EzoState::EZO_STATE_IDLE;
}

void EZOSensor::loop() {
  if (this->state_ == EzoState::EZO_STATE_IDLE || this->state_ == EzoState::EZO_STATE_IDLE_AFTER_READ) {
    this->send_next_();
    return;
  }

  if (static_cast<int32_t>(App.get_loop_component_start_time() - this->reply_due_) < 0)
    return;

  // Response code, then up to 52 characters (the largest documented reply, EZO-RGB), then a terminator
  // that is always set, since a reply that fills the read has none.
  uint8_t buf[MAX_REPLY_LENGTH + 2];
  buf[0] = 0;
  buf[MAX_REPLY_LENGTH + 1] = 0;

  if (!this->read_bytes_raw(buf, MAX_REPLY_LENGTH + 1)) {
    ESP_LOGE(TAG, "read error");
    this->finish_command_();
    return;
  }

  switch (buf[0]) {
    case 1:
      break;
    case 2:
      ESP_LOGE(TAG, "device returned a syntax error");
      break;
    case 254:
      return;  // keep waiting
    case 255:
      ESP_LOGE(TAG, "device returned no data");
      break;
    default:
      ESP_LOGE(TAG, "device returned an unknown response: %d", buf[0]);
      break;
  }

  const EzoCommandType command_type =
      this->state_ == EzoState::EZO_STATE_SENT_READ ? EzoCommandType::EZO_READ : reply_type(this->queue_.front().id);
  // Free the slot first so the callbacks below can queue a follow-up
  this->finish_command_();
  ESP_LOGV(TAG, "Received buffer \"%s\" for command type %s", &buf[1],
           LOG_STR_ARG(EzoCommandTypeStrings::get_log_str(command_type, EzoCommandTypeStrings::LAST_INDEX)));

  char *payload = reinterpret_cast<char *>(&buf[1]);
  if (buf[0] != 1 || payload[0] == '\0')
    return;
  char *comma = strchr(payload, ',');
  const char *text = comma != nullptr ? comma + 1 : nullptr;
  CallbackManager<void(const std::string &)> *callback = nullptr;
  switch (command_type) {
    case EzoCommandType::EZO_READ: {
      // some sensors return multiple comma-separated values, terminate string after first one
      if (comma != nullptr)
        *comma = '\0';
      auto val = parse_number<float>(payload);
      if (!val.has_value()) {
        ESP_LOGW(TAG, "Can't convert '%s' to number!", payload);
      } else {
        this->publish_state(*val);
      }
      return;
    }
    case EzoCommandType::EZO_LED:
      this->led_callback_.call(payload[strlen(payload) - 1] == '1');
      return;
    case EzoCommandType::EZO_DEVICE_INFORMATION:
      callback = &this->device_infomation_callback_;
      break;
    case EzoCommandType::EZO_SLOPE:
      callback = &this->slope_callback_;
      break;
    case EzoCommandType::EZO_CALIBRATION:
      callback = &this->calibration_callback_;
      break;
    case EzoCommandType::EZO_T:
      callback = &this->t_callback_;
      break;
    case EzoCommandType::EZO_CUSTOM:
      callback = &this->custom_callback_;
      text = payload;
      break;
    default:
      return;
  }
  if (text != nullptr)
    callback->call(std::string(text));
}

void EZOSensor::add_command_(EzoCommandId id, float value) {
  if (!this->queue_.push(EzoCommand{id, {value}})) {
    ESP_LOGW(
        TAG, "Command queue full, dropped \"%s\"",
        LOG_STR_ARG(EzoCommandTextStrings::get_log_str(static_cast<uint8_t>(id), EzoCommandTextStrings::LAST_INDEX)));
    return;
  }
  this->enable_loop();
}

void EZOSensor::set_address(uint8_t address) {
  if (address > 0 && address < 128) {
    this->add_command_(EzoCommandId::EZO_COMMAND_ID_I2C, address);
  } else {
    ESP_LOGE(TAG, "Invalid I2C address");
  }
}

void EZOSensor::get_device_information() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_DEVICE_INFORMATION); }

void EZOSensor::set_sleep() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_SLEEP); }

void EZOSensor::get_state() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_READ); }

void EZOSensor::get_slope() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_SLOPE); }

void EZOSensor::get_t() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_T_GET); }

void EZOSensor::set_t(float value) {
  // A newer value replaces one that is still waiting to be sent
  bool in_flight = this->state_ == EzoState::EZO_STATE_SENT_QUEUED;
  for (EzoCommand &command : this->queue_) {
    if (!in_flight && command.id == EzoCommandId::EZO_COMMAND_ID_T_SET) {
      command.value = value;
      return;
    }
    in_flight = false;
  }
  this->add_command_(EzoCommandId::EZO_COMMAND_ID_T_SET, value);
}

void EZOSensor::set_tempcomp_value(float temp) { this->set_t(temp); }

void EZOSensor::get_calibration() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_CAL_GET); }

void EZOSensor::set_calibration_point_low(float value) {
  this->add_command_(EzoCommandId::EZO_COMMAND_ID_CAL_LOW, value);
}

void EZOSensor::set_calibration_point_mid(float value) {
  this->add_command_(EzoCommandId::EZO_COMMAND_ID_CAL_MID, value);
}

void EZOSensor::set_calibration_point_high(float value) {
  this->add_command_(EzoCommandId::EZO_COMMAND_ID_CAL_HIGH, value);
}

void EZOSensor::set_calibration_generic(float value) {
  this->add_command_(EzoCommandId::EZO_COMMAND_ID_CAL_GENERIC, value);
}

void EZOSensor::clear_calibration() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_CAL_CLEAR); }

void EZOSensor::get_led_state() { this->add_command_(EzoCommandId::EZO_COMMAND_ID_LED_GET); }

void EZOSensor::set_led_state(bool on) {
  this->add_command_(on ? EzoCommandId::EZO_COMMAND_ID_LED_ON : EzoCommandId::EZO_COMMAND_ID_LED_OFF);
}

void EZOSensor::send_custom(const std::string &to_send) {
  const size_t length = to_send.size();
  if (length > EZO_MAX_COMMAND_LENGTH) {
    ESP_LOGW(TAG, "Command too long (%u > %u), dropped", static_cast<unsigned>(length),
             static_cast<unsigned>(EZO_MAX_COMMAND_LENGTH));
    return;
  }
  if (this->queue_.size() >= EZO_COMMAND_QUEUE_SIZE || this->custom_text_used_ + length > EZO_CUSTOM_TEXT_SIZE) {
    ESP_LOGW(TAG, "Command queue full, dropped \"%s\"", to_send.c_str());
    return;
  }
  // Append after the queued texts, wrapping around the end of the ring
  const size_t end = (this->custom_text_start_ + this->custom_text_used_) % EZO_CUSTOM_TEXT_SIZE;
  const size_t first = std::min(length, EZO_CUSTOM_TEXT_SIZE - end);
  memcpy(this->custom_text_ + end, to_send.data(), first);
  memcpy(this->custom_text_, to_send.data() + first, length - first);
  this->custom_text_used_ += length;
  EzoCommand command{EzoCommandId::EZO_COMMAND_ID_CUSTOM, {}};
  command.text_length = length;
  this->queue_.push(command);
  this->enable_loop();
}

}  // namespace esphome::ezo
