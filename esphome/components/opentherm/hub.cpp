#include "hub.h"
#include "esphome/core/helpers.h"

#include <string>

namespace esphome {
namespace opentherm {

static const char *const TAG = "opentherm";
namespace message_data {
bool parse_flag8_lb_0(OpenthermData &data) { return read_bit(data.valueLB, 0); }
bool parse_flag8_lb_1(OpenthermData &data) { return read_bit(data.valueLB, 1); }
bool parse_flag8_lb_2(OpenthermData &data) { return read_bit(data.valueLB, 2); }
bool parse_flag8_lb_3(OpenthermData &data) { return read_bit(data.valueLB, 3); }
bool parse_flag8_lb_4(OpenthermData &data) { return read_bit(data.valueLB, 4); }
bool parse_flag8_lb_5(OpenthermData &data) { return read_bit(data.valueLB, 5); }
bool parse_flag8_lb_6(OpenthermData &data) { return read_bit(data.valueLB, 6); }
bool parse_flag8_lb_7(OpenthermData &data) { return read_bit(data.valueLB, 7); }
bool parse_flag8_hb_0(OpenthermData &data) { return read_bit(data.valueHB, 0); }
bool parse_flag8_hb_1(OpenthermData &data) { return read_bit(data.valueHB, 1); }
bool parse_flag8_hb_2(OpenthermData &data) { return read_bit(data.valueHB, 2); }
bool parse_flag8_hb_3(OpenthermData &data) { return read_bit(data.valueHB, 3); }
bool parse_flag8_hb_4(OpenthermData &data) { return read_bit(data.valueHB, 4); }
bool parse_flag8_hb_5(OpenthermData &data) { return read_bit(data.valueHB, 5); }
bool parse_flag8_hb_6(OpenthermData &data) { return read_bit(data.valueHB, 6); }
bool parse_flag8_hb_7(OpenthermData &data) { return read_bit(data.valueHB, 7); }
uint8_t parse_u8_lb(OpenthermData &data) { return data.valueLB; }
uint8_t parse_u8_hb(OpenthermData &data) { return data.valueHB; }
int8_t parse_s8_lb(OpenthermData &data) { return (int8_t) data.valueLB; }
int8_t parse_s8_hb(OpenthermData &data) { return (int8_t) data.valueHB; }
uint16_t parse_u16(OpenthermData &data) { return data.u16(); }
uint16_t parse_u8_lb_60(OpenthermData &data) { return data.valueLB * 60; }
uint16_t parse_u8_hb_60(OpenthermData &data) { return data.valueHB * 60; }
int16_t parse_s16(OpenthermData &data) { return data.s16(); }
float parse_f88(OpenthermData &data) { return data.f88(); }

void write_flag8_lb_0(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 0, value); }
void write_flag8_lb_1(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 1, value); }
void write_flag8_lb_2(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 2, value); }
void write_flag8_lb_3(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 3, value); }
void write_flag8_lb_4(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 4, value); }
void write_flag8_lb_5(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 5, value); }
void write_flag8_lb_6(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 6, value); }
void write_flag8_lb_7(const bool value, OpenthermData &data) { data.valueLB = write_bit(data.valueLB, 7, value); }
void write_flag8_hb_0(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 0, value); }
void write_flag8_hb_1(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 1, value); }
void write_flag8_hb_2(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 2, value); }
void write_flag8_hb_3(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 3, value); }
void write_flag8_hb_4(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 4, value); }
void write_flag8_hb_5(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 5, value); }
void write_flag8_hb_6(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 6, value); }
void write_flag8_hb_7(const bool value, OpenthermData &data) { data.valueHB = write_bit(data.valueHB, 7, value); }
void write_u8_lb(const uint8_t value, OpenthermData &data) { data.valueLB = value; }
void write_u8_hb(const uint8_t value, OpenthermData &data) { data.valueHB = value; }
void write_s8_lb(const int8_t value, OpenthermData &data) { data.valueLB = (uint8_t) value; }
void write_s8_hb(const int8_t value, OpenthermData &data) { data.valueHB = (uint8_t) value; }
void write_u16(const uint16_t value, OpenthermData &data) { data.u16(value); }
void write_s16(const int16_t value, OpenthermData &data) { data.s16(value); }
void write_f88(const float value, OpenthermData &data) { data.f88(value); }

}  // namespace message_data

OpenthermData OpenthermHub::build_request_(MessageId request_id) const {
  OpenthermData data;
  data.type = 0;
  data.id = 0;
  data.valueHB = 0;
  data.valueLB = 0;

  // We need this special logic for STATUS message because we have two options for specifying boiler modes:
  // with static config values in the hub, or with separate switches.
  if (request_id == MessageId::STATUS) {
    // NOLINTBEGIN
    bool const ch_enabled = this->ch_enable && OPENTHERM_READ_ch_enable && OPENTHERM_READ_t_set > 0.0;
    bool const dhw_enabled = this->dhw_enable && OPENTHERM_READ_dhw_enable;
    bool const cooling_enabled =
        this->cooling_enable && OPENTHERM_READ_cooling_enable && OPENTHERM_READ_cooling_control > 0.0;
    bool const otc_enabled = this->otc_active && OPENTHERM_READ_otc_active;
    bool const ch2_enabled = this->ch2_active && OPENTHERM_READ_ch2_active && OPENTHERM_READ_t_set_ch2 > 0.0;
    bool const summer_mode_is_active = this->summer_mode_active && OPENTHERM_READ_summer_mode_active;
    bool const dhw_blocked = this->dhw_block && OPENTHERM_READ_dhw_block;
    // NOLINTEND

    data.type = MessageType::READ_DATA;
    data.id = MessageId::STATUS;
    data.valueHB = ch_enabled | (dhw_enabled << 1) | (cooling_enabled << 2) | (otc_enabled << 3) | (ch2_enabled << 4) |
                   (summer_mode_is_active << 5) | (dhw_blocked << 6);

    return data;
  }

  // Another special case is OpenTherm version number which is configured at hub level as a constant
  if (request_id == MessageId::OT_VERSION_CONTROLLER) {
    data.type = MessageType::WRITE_DATA;
    data.id = MessageId::OT_VERSION_CONTROLLER;
    data.f88(this->opentherm_version_);

    return data;
  }

// Disable incomplete switch statement warnings, because the cases in each
// switch are generated based on the configured sensors and inputs.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

  // Next, we start with the write requests from switches and other inputs,
  // because we would want to write that data if it is available, rather than
  // request a read for that type (in the case that both read and write are
  // supported).
  switch (request_id) {
    OPENTHERM_SWITCH_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_WRITE_MESSAGE, OPENTHERM_MESSAGE_WRITE_ENTITY, ,
                                      OPENTHERM_MESSAGE_WRITE_POSTSCRIPT, )
    OPENTHERM_NUMBER_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_WRITE_MESSAGE, OPENTHERM_MESSAGE_WRITE_ENTITY, ,
                                      OPENTHERM_MESSAGE_WRITE_POSTSCRIPT, )
    OPENTHERM_OUTPUT_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_WRITE_MESSAGE, OPENTHERM_MESSAGE_WRITE_ENTITY, ,
                                      OPENTHERM_MESSAGE_WRITE_POSTSCRIPT, )
    OPENTHERM_INPUT_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_WRITE_MESSAGE, OPENTHERM_MESSAGE_WRITE_ENTITY, ,
                                            OPENTHERM_MESSAGE_WRITE_POSTSCRIPT, )
  }

  // Finally, handle the simple read requests, which only change with the message id.
  switch (request_id) { OPENTHERM_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_READ_MESSAGE, OPENTHERM_IGNORE, , , ) }
  switch (request_id) {
    OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_READ_MESSAGE, OPENTHERM_IGNORE, , , )
  }
#pragma GCC diagnostic pop

  // And if we get here, a message was requested which somehow wasn't handled.
  // This shouldn't happen due to the way the defines are configured, so we
  // log an error and just return a 0 message.
  ESP_LOGE(TAG, "Tried to create a request with unknown id %d. This should never happen, so please open an issue.",
           request_id);
  return {};
}

OpenthermHub::OpenthermHub() : Component(), in_pin_{}, out_pin_{} {}

void OpenthermHub::process_response(OpenthermData &data) {
  ESP_LOGD(TAG, "Received OpenTherm response with id %d (%s)", data.id,
           this->opentherm_->message_id_to_str((MessageId) data.id));
  ESP_LOGD(TAG, "%s", this->opentherm_->debug_data(data).c_str());

  switch (data.id) {
    OPENTHERM_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_RESPONSE_MESSAGE, OPENTHERM_MESSAGE_RESPONSE_ENTITY, ,
                                      OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT, )
  }
  switch (data.id) {
    OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_RESPONSE_MESSAGE, OPENTHERM_MESSAGE_RESPONSE_ENTITY, ,
                                             OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT, )
  }
}

void OpenthermHub::setup() {
  ESP_LOGD(TAG, "Setting up OpenTherm component");
  this->opentherm_ = make_unique<OpenTherm>(this->in_pin_, this->out_pin_);
  if (!this->opentherm_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize OpenTherm protocol. See previous log messages for details.");
    this->mark_failed();
    return;
  }

  // Ensure that there is at least one request, as we are required to
  // communicate at least once every second. Sending the status request is
  // good practice anyway.
  this->add_repeating_message(MessageId::STATUS);

  // Also ensure that we start communication with the STATUS message
  this->initial_messages_.insert(this->initial_messages_.begin(), MessageId::STATUS);

  if (this->opentherm_version_ > 0.0f) {
    this->initial_messages_.insert(this->initial_messages_.begin(), MessageId::OT_VERSION_CONTROLLER);
  }

  this->current_message_iterator_ = this->initial_messages_.begin();
}

void OpenthermHub::on_shutdown() { this->opentherm_->stop(); }

void OpenthermHub::loop() {
  if (this->sync_mode_) {
    this->sync_loop_();
    return;
  }

  auto cur_time = millis();
  auto const cur_mode = this->opentherm_->get_mode();
  switch (cur_mode) {
    case OperationMode::WRITE:
    case OperationMode::READ:
    case OperationMode::LISTEN:
      if (!this->check_timings_(cur_time)) {
        break;
      }
      this->last_mode_ = cur_mode;
      break;
    case OperationMode::ERROR_PROTOCOL:
      if (this->last_mode_ == OperationMode::WRITE) {
        this->handle_protocol_write_error_();
      } else if (this->last_mode_ == OperationMode::READ) {
        this->handle_protocol_read_error_();
      }

      this->stop_opentherm_();
      break;
    case OperationMode::ERROR_TIMEOUT:
      this->handle_timeout_error_();
      this->stop_opentherm_();
      break;
    case OperationMode::IDLE:
      if (this->should_skip_loop_(cur_time)) {
        break;
      }
      this->start_conversation_();
      break;
    case OperationMode::SENT:
      // Message sent, now listen for the response.
      this->opentherm_->listen();
      break;
    case OperationMode::RECEIVED:
      this->read_response_();
      break;
  }
}

void OpenthermHub::sync_loop_() {
  if (!this->opentherm_->is_idle()) {
    ESP_LOGE(TAG, "OpenTherm is not idle at the start of the loop");
    return;
  }

  auto cur_time = millis();

  this->check_timings_(cur_time);

  if (this->should_skip_loop_(cur_time)) {
    return;
  }

  this->start_conversation_();

  if (!this->spin_wait_(1150, [&] { return this->opentherm_->is_active(); })) {
    ESP_LOGE(TAG, "Hub timeout triggered during send");
    this->stop_opentherm_();
    return;
  }

  if (this->opentherm_->is_error()) {
    this->handle_protocol_write_error_();
    this->stop_opentherm_();
    return;
  } else if (!this->opentherm_->is_sent()) {
    ESP_LOGW(TAG, "Unexpected state after sending request: %s",
             this->opentherm_->operation_mode_to_str(this->opentherm_->get_mode()));
    this->stop_opentherm_();
    return;
  }

  // Listen for the response
  this->opentherm_->listen();
  if (!this->spin_wait_(1150, [&] { return this->opentherm_->is_active(); })) {
    ESP_LOGE(TAG, "Hub timeout triggered during receive");
    this->stop_opentherm_();
    return;
  }

  if (this->opentherm_->is_timeout()) {
    this->handle_timeout_error_();
    this->stop_opentherm_();
    return;
  } else if (this->opentherm_->is_protocol_error()) {
    this->handle_protocol_read_error_();
    this->stop_opentherm_();
    return;
  } else if (!this->opentherm_->has_message()) {
    ESP_LOGW(TAG, "Unexpected state after receiving response: %s",
             this->opentherm_->operation_mode_to_str(this->opentherm_->get_mode()));
    this->stop_opentherm_();
    return;
  }

  this->read_response_();
}

bool OpenthermHub::check_timings_(uint32_t cur_time) {
  if (this->last_conversation_start_ > 0 && (cur_time - this->last_conversation_start_) > 1150) {
    ESP_LOGW(TAG,
             "%d ms elapsed since the start of the last convo, but 1150 ms are allowed at maximum. Look at other "
             "components that might slow the loop down.",
             (int) (cur_time - this->last_conversation_start_));
    this->stop_opentherm_();
    return false;
  }

  return true;
}

bool OpenthermHub::should_skip_loop_(uint32_t cur_time) const {
  if (this->last_conversation_end_ > 0 && (cur_time - this->last_conversation_end_) < 100) {
    ESP_LOGV(TAG, "Less than 100 ms elapsed since last convo, skipping this iteration");
    return true;
  }

  return false;
}

void OpenthermHub::start_conversation_() {
  if (this->sending_initial_ && this->current_message_iterator_ == this->initial_messages_.end()) {
    this->sending_initial_ = false;
    this->current_message_iterator_ = this->repeating_messages_.begin();
  } else if (this->current_message_iterator_ == this->repeating_messages_.end()) {
    this->current_message_iterator_ = this->repeating_messages_.begin();
  }

  auto request = this->build_request_(*this->current_message_iterator_);

  ESP_LOGD(TAG, "Sending request with id %d (%s)", request.id,
           this->opentherm_->message_id_to_str((MessageId) request.id));
  ESP_LOGD(TAG, "%s", this->opentherm_->debug_data(request).c_str());
  // Send the request
  this->last_conversation_start_ = millis();
  this->opentherm_->send(request);
}

void OpenthermHub::read_response_() {
  OpenthermData response;
  if (!this->opentherm_->get_message(response)) {
    ESP_LOGW(TAG, "Couldn't get the response, but flags indicated success. This is a bug.");
    this->stop_opentherm_();
    return;
  }

  this->stop_opentherm_();

  this->process_response(response);

  this->current_message_iterator_++;
}

void OpenthermHub::stop_opentherm_() {
  this->opentherm_->stop();
  this->last_conversation_end_ = millis();
}

void OpenthermHub::handle_protocol_write_error_() {
  ESP_LOGW(TAG, "Error while sending request: %s",
           this->opentherm_->operation_mode_to_str(this->opentherm_->get_mode()));
  ESP_LOGW(TAG, "%s", this->opentherm_->debug_data(this->last_request_).c_str());
}

void OpenthermHub::handle_protocol_read_error_() {
  OpenThermError error;
  this->opentherm_->get_protocol_error(error);
  ESP_LOGW(TAG, "Protocol error occured while receiving response: %s", this->opentherm_->debug_error(error).c_str());
}

void OpenthermHub::handle_timeout_error_() {
  ESP_LOGW(TAG, "Receive response timed out at a protocol level");
  this->stop_opentherm_();
}

void OpenthermHub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm:");
  LOG_PIN("  In: ", this->in_pin_);
  LOG_PIN("  Out: ", this->out_pin_);
  ESP_LOGCONFIG(TAG, "  Sync mode: %d", this->sync_mode_);
  ESP_LOGCONFIG(TAG, "  Sensors: %s", SHOW(OPENTHERM_SENSOR_LIST(ID, )));
  ESP_LOGCONFIG(TAG, "  Binary sensors: %s", SHOW(OPENTHERM_BINARY_SENSOR_LIST(ID, )));
  ESP_LOGCONFIG(TAG, "  Switches: %s", SHOW(OPENTHERM_SWITCH_LIST(ID, )));
  ESP_LOGCONFIG(TAG, "  Input sensors: %s", SHOW(OPENTHERM_INPUT_SENSOR_LIST(ID, )));
  ESP_LOGCONFIG(TAG, "  Outputs: %s", SHOW(OPENTHERM_OUTPUT_LIST(ID, )));
  ESP_LOGCONFIG(TAG, "  Numbers: %s", SHOW(OPENTHERM_NUMBER_LIST(ID, )));
  ESP_LOGCONFIG(TAG, "  Initial requests:");
  for (auto type : this->initial_messages_) {
    ESP_LOGCONFIG(TAG, "  - %d (%s)", type, this->opentherm_->message_id_to_str((type)));
  }
  ESP_LOGCONFIG(TAG, "  Repeating requests:");
  for (auto type : this->repeating_messages_) {
    ESP_LOGCONFIG(TAG, "  - %d (%s)", type, this->opentherm_->message_id_to_str((type)));
  }
}

}  // namespace opentherm
}  // namespace esphome
