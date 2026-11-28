#include "opentherm.h"
#include "esphome/core/helpers.h"

#include <string>

namespace esphome::opentherm {

ESPHOME_LOG_TAG(TAG, "opentherm");
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
uint16_t parse_u16(OpenthermData &data) { return encode_uint16(data.valueHB, data.valueLB); }
uint16_t parse_u8_lb_60(OpenthermData &data) { return data.valueLB * 60; }
uint16_t parse_u8_hb_60(OpenthermData &data) { return data.valueHB * 60; }
int16_t parse_s16(OpenthermData &data) { return static_cast<int16_t>(encode_uint16(data.valueHB, data.valueLB)); }
float parse_f88(OpenthermData &data) { return data.get_f88(); }

void write_flag8_lb_0(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 0, value); }
void write_flag8_lb_1(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 1, value); }
void write_flag8_lb_2(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 2, value); }
void write_flag8_lb_3(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 3, value); }
void write_flag8_lb_4(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 4, value); }
void write_flag8_lb_5(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 5, value); }
void write_flag8_lb_6(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 6, value); }
void write_flag8_lb_7(const bool value, OpenthermData &data) { assign_bits(data.valueLB, 1 << 7, value); }
void write_flag8_hb_0(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 0, value); }
void write_flag8_hb_1(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 1, value); }
void write_flag8_hb_2(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 2, value); }
void write_flag8_hb_3(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 3, value); }
void write_flag8_hb_4(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 4, value); }
void write_flag8_hb_5(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 5, value); }
void write_flag8_hb_6(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 6, value); }
void write_flag8_hb_7(const bool value, OpenthermData &data) { assign_bits(data.valueHB, 1 << 7, value); }
void write_u8_lb(const uint8_t value, OpenthermData &data) { data.valueLB = value; }
void write_u8_hb(const uint8_t value, OpenthermData &data) { data.valueHB = value; }
void write_s8_lb(const int8_t value, OpenthermData &data) { data.valueLB = (uint8_t) value; }
void write_s8_hb(const int8_t value, OpenthermData &data) { data.valueHB = (uint8_t) value; }
void write_u16(const uint16_t value, OpenthermData &data) {
  const auto bytes = decode_value(value);
  data.valueHB = bytes[0];
  data.valueLB = bytes[1];
}
void write_s16(const int16_t value, OpenthermData &data) { write_u16(static_cast<uint16_t>(value), data); }
void write_f88(const float value, OpenthermData &data) { data.set_f88(value); }

}  // namespace message_data

OpenthermData OpenTherm::build_request_(MessageId request_id) const {
  OpenthermData data;
  data.type = 0;
  data.id = request_id;
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
    data.valueHB = ch_enabled | (dhw_enabled << 1) | (cooling_enabled << 2) | (otc_enabled << 3) | (ch2_enabled << 4) |
                   (summer_mode_is_active << 5) | (dhw_blocked << 6);

    return data;
  }

  // Next, we start with write requests from switches and other inputs,
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
    OPENTHERM_SETTING_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_WRITE_MESSAGE, OPENTHERM_MESSAGE_WRITE_SETTING, ,
                                       OPENTHERM_MESSAGE_WRITE_POSTSCRIPT, )
    default:
      break;
  }

  // Finally, handle the simple read requests, which only change with the message id.
  switch (request_id) {
    OPENTHERM_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_READ_MESSAGE, OPENTHERM_IGNORE, , , )
    default:
      break;
  }
  switch (request_id) {
    OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_READ_MESSAGE, OPENTHERM_IGNORE, , , )
    default:
      break;
  }

  // And if we get here, a message was requested which somehow wasn't handled.
  // This shouldn't happen due to the way the defines are configured, so we
  // log an error and just return a 0 message.
  ESP_LOGE(TAG, "Tried to create a request with unknown id %d. This should never happen, so please open an issue.",
           request_id);
  return {};
}

OpenTherm::OpenTherm() : Component(), in_pin_{}, out_pin_{} {}

void OpenTherm::process_response(OpenthermData &data) {
  ESP_LOGD(TAG, "Received OpenTherm response with id %d (%s)", data.id,
           this->opentherm_->message_id_to_str((MessageId) data.id));
  this->opentherm_->debug_data(data);

  switch (data.id) {
    OPENTHERM_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_RESPONSE_MESSAGE, OPENTHERM_MESSAGE_RESPONSE_ENTITY, ,
                                      OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT, )
  }
  switch (data.id) {
    OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_RESPONSE_MESSAGE, OPENTHERM_MESSAGE_RESPONSE_ENTITY, ,
                                             OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT, )
  }
}

void OpenTherm::setup() {
  // One transport per build: RMT where the chip has it, timer sampling everywhere else.
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  this->opentherm_ = make_unique<OpenThermRMT>(this->in_pin_, this->out_pin_);
  this->opentherm_->set_rmt_symbols(this->rmt_symbols_);
  this->opentherm_->set_buffer(this->buffer_);
#else
  this->opentherm_ = make_unique<OpenThermPolling>(this->in_pin_, this->out_pin_);
#endif
  if (!this->opentherm_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize OpenTherm protocol. See previous log messages for details.");
    this->mark_failed();
    return;
  }

  // Ensure that there is at least one request, as we are required to
  // communicate at least once every second. Sending the status request is
  // good practice anyway.
  this->add_repeating_message(MessageId::STATUS);
  this->write_initial_messages_(this->messages_);
  this->message_iterator_ = this->messages_.begin();
}

void OpenTherm::on_shutdown() { this->opentherm_->stop(); }

// Disabling clang-tidy for this particular line since it keeps removing the trailing underscore (bug?)
void OpenTherm::write_initial_messages_(std::vector<MessageId> &target) {  // NOLINT
  std::vector<std::pair<MessageId, uint8_t>> sorted;
  std::copy_if(this->configured_messages_.begin(), this->configured_messages_.end(), std::back_inserter(sorted),
               [](const std::pair<MessageId, uint8_t> &pair) { return pair.second < REPEATING_MESSAGE_ORDER; });
  std::sort(sorted.begin(), sorted.end(),
            [](const std::pair<MessageId, uint8_t> &a, const std::pair<MessageId, uint8_t> &b) {
              return a.second < b.second;
            });

  target.clear();
  std::transform(sorted.begin(), sorted.end(), std::back_inserter(target),
                 [](const std::pair<MessageId, uint8_t> &pair) { return pair.first; });
}

// Disabling clang-tidy for this particular line since it keeps removing the trailing underscore (bug?)
void OpenTherm::write_repeating_messages_(std::vector<MessageId> &target) {  // NOLINT
  target.clear();
  for (auto const &pair : this->configured_messages_) {
    if (pair.second == REPEATING_MESSAGE_ORDER) {
      target.push_back(pair.first);
    }
  }
}

void OpenTherm::loop() {
  // Decode an RMT capture first, so the state machine is updated with the latest state
  this->opentherm_->loop();

  // One pass per call, except in sync mode.
  do {
    this->run_state_machine_();
  } while (this->sync_step_());
}

void OpenTherm::run_state_machine_() {
  auto cur_time = millis();
  const OperationMode cur_mode = this->opentherm_->get_mode();

  if (this->handle_error_(cur_mode)) {
    return;
  }

  switch (cur_mode) {
    case OperationMode::WRITE:
      break;
    case OperationMode::LISTEN:
      // The transport arms its receiver on the first pass after the transmission completed. The answer cannot come
      // sooner than 20 ms, so the loop is in time and the interrupt path stays free of it.
      this->opentherm_->listen();
      [[fallthrough]];
    case OperationMode::READ:
      // The response clock lives here, not in the transport: the deadline runs from the start of the request.
      if (cur_time - this->last_conversation_start_ >= this->opentherm_->response_deadline_ms()) {
        this->opentherm_->timeout();
      }
      break;
    case OperationMode::IDLE:
      this->check_timings_(cur_time);
      if (this->should_skip_loop_(cur_time)) {
        break;
      }
      this->start_conversation_();
      break;
    case OperationMode::RECEIVED:
      this->read_response_();
      break;
    default:
      break;
  }
}

bool OpenTherm::sync_step_() {
#if defined(USE_ESP32) && SOC_RMT_SUPPORTED
  return false;  // Sync mode does not exist on the RMT transport.
#else
  if (!this->sync_mode_ || this->opentherm_->is_idle()) {
    return false;
  }
  if (!this->opentherm_->is_active()) {
    return true;  // The stage already advanced; run the machine again.
  }
  // The transport advances in its interrupts, so wait for the stage to finish, within the response deadline.
  const uint32_t deadline = this->opentherm_->response_deadline_ms();
  const uint32_t elapsed = millis() - this->last_conversation_start_;
  if (!this->opentherm_->wait_until_done(elapsed < deadline ? deadline - elapsed : 0)) {
    this->opentherm_->timeout();
  }
  return true;
#endif
}

bool OpenTherm::handle_error_(OperationMode mode) {
  switch (mode) {
    case OperationMode::ERROR_PROTOCOL:
      // Protocol error can happen only while reading boiler response.
      this->handle_protocol_error_();
      return true;
    case OperationMode::ERROR_TIMEOUT:
      // Timeout error might happen while we wait for device to respond.
      this->handle_timeout_error_();
      return true;
    case OperationMode::ERROR_TIMER:
      // Transport failure: the timer on ESP32 and ESP8266, or the RMT driver when a transmit cannot be queued.
      this->handle_timer_error_();
      return true;
    case OperationMode::ERROR_TRANSPORT:
      // The driver rejected a transmit or a capture. Both are retried on the next cycle, so this is not fatal.
      this->handle_transport_error_();
      return true;
    default:
      return false;
  }
}

void OpenTherm::check_timings_(uint32_t cur_time) {
  if (this->last_conversation_start_ > 0 && (cur_time - this->last_conversation_start_) > 1150) {
    ESP_LOGW(TAG,
             "%d ms elapsed since the start of the last convo, but 1150 ms are allowed at maximum. Look at other "
             "components that might slow the loop down.",
             (int) (cur_time - this->last_conversation_start_));
  }
}

bool OpenTherm::should_skip_loop_(uint32_t cur_time) const {
  if (this->last_conversation_end_ > 0 && (cur_time - this->last_conversation_end_) < 100) {
    ESP_LOGV(TAG, "Less than 100 ms elapsed since last convo, skipping this iteration");
    return true;
  }

  return false;
}

void OpenTherm::start_conversation_() {
  if (this->message_iterator_ == this->messages_.end()) {
    if (this->sending_initial_) {
      this->sending_initial_ = false;
      this->write_repeating_messages_(this->messages_);
    }
    this->message_iterator_ = this->messages_.begin();
  }

  auto request = this->build_request_(*this->message_iterator_);

  this->before_send_callback_.call(request);

  ESP_LOGD(TAG, "Sending request with id %d (%s)", request.id,
           this->opentherm_->message_id_to_str((MessageId) request.id));
  this->opentherm_->debug_data(request);
  // Send the request
  this->last_conversation_start_ = millis();
  this->opentherm_->send(request);
}

void OpenTherm::read_response_() {
  OpenthermData response;
  if (!this->opentherm_->get_message(response)) {
    ESP_LOGW(TAG, "Couldn't get the response, but flags indicated success. This is a bug.");
    this->stop_opentherm_();
    return;
  }

  this->stop_opentherm_();

  this->before_process_response_callback_.call(response);
  this->process_response(response);

  this->message_iterator_++;
}

void OpenTherm::stop_opentherm_() {
  this->opentherm_->stop();
  this->last_conversation_end_ = millis();
}

void OpenTherm::handle_protocol_error_() {
  OpenThermError error;
  this->opentherm_->get_protocol_error(error);
  ESP_LOGW(TAG, "Protocol error occurred while receiving response: %s",
           this->opentherm_->protocol_error_to_str(error.error_type));
  this->opentherm_->debug_error(error);
  this->stop_opentherm_();
}

void OpenTherm::handle_timeout_error_() {
  ESP_LOGW(TAG, "Timeout while waiting for response from device");
  this->stop_opentherm_();
}

void OpenTherm::handle_timer_error_() {
  this->opentherm_->report_and_reset_timer_error();
  this->stop_opentherm_();
  // Timer error is critical, there is no point in retrying.
  this->mark_failed();
}

void OpenTherm::handle_transport_error_() {
  // The device is still there, the driver is not; the next conversation retries the same request.
  ESP_LOGW(TAG, "Transport error, retrying on the next cycle");
  this->stop_opentherm_();
}

void OpenTherm::dump_config() {
  std::vector<MessageId> initial_messages;
  std::vector<MessageId> repeating_messages;
  this->write_initial_messages_(initial_messages);
  this->write_repeating_messages_(repeating_messages);

  ESP_LOGCONFIG(TAG, "OpenTherm:");
#if !(defined(USE_ESP32) && SOC_RMT_SUPPORTED)
  ESP_LOGCONFIG(TAG, "  Sync mode: %s", YESNO(this->sync_mode_));
#endif
  ESP_LOGCONFIG(TAG,
                "  Sensors: %s\n"
                "  Binary sensors: %s\n"
                "  Switches: %s\n"
                "  Input sensors: %s\n"
                "  Outputs: %s\n"
                "  Numbers: %s",
                SHOW(OPENTHERM_SENSOR_LIST(ID, )), SHOW(OPENTHERM_BINARY_SENSOR_LIST(ID, )),
                SHOW(OPENTHERM_SWITCH_LIST(ID, )), SHOW(OPENTHERM_INPUT_SENSOR_LIST(ID, )),
                SHOW(OPENTHERM_OUTPUT_LIST(ID, )), SHOW(OPENTHERM_NUMBER_LIST(ID, )));
  LOG_PIN("  In: ", this->in_pin_);
  LOG_PIN("  Out: ", this->out_pin_);
  ESP_LOGCONFIG(TAG, "  Initial requests:");
  for (auto type : initial_messages) {
    ESP_LOGCONFIG(TAG, "  - %d (%s)", type, this->opentherm_->message_id_to_str(type));
  }
  ESP_LOGCONFIG(TAG, "  Repeating requests:");
  for (auto type : repeating_messages) {
    ESP_LOGCONFIG(TAG, "  - %d (%s)", type, this->opentherm_->message_id_to_str(type));
  }
}

}  // namespace esphome::opentherm
