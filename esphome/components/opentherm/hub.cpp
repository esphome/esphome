#include "hub.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "opentherm_base.h"

namespace esphome {
namespace opentherm {

static const char *const TAG = "opentherm";

enum {
  STATUS_HB_CH_ENABLE = 0,
  STATUS_HB_DHW_ENABLE = 1,
  STATUS_HB_COOLING_ENABLE = 2,
  STATUS_HB_OTC_ACTIVE = 3,
  STATUS_HB_CH2_ENABLE = 4,
  STATUS_HB_SUMMER_MODE_ACTIVE = 5,
  STATUS_HB_DHW_BLOCK = 6,
};

OpenthermData OpenthermHub::build_request_(MessageId request_id) const {
  OpenthermData data;

  // Start off with the status bits set in the hub; these can be overridden by switches defined by the user
  if (request_id == MessageId::STATUS) {
    OpenthermData temp;

    // Fill in the values from the hub
    data.valueHB = (this->ch_enable << STATUS_HB_CH_ENABLE) | (this->dhw_enable << STATUS_HB_DHW_ENABLE) |
                   (this->cooling_enable << STATUS_HB_COOLING_ENABLE) | (this->otc_active << STATUS_HB_OTC_ACTIVE) |
                   (this->ch2_active << STATUS_HB_CH2_ENABLE) |
                   (this->summer_mode_active << STATUS_HB_SUMMER_MODE_ACTIVE) |
                   (this->dhw_block << STATUS_HB_DHW_BLOCK);

    // Allow user-supplied switches to override them
    prepare_data_out_(request_id, data);

    // STATUS is _awlays_ a READ_DATA; prepare_data_out_() would have set this to WRITE_DATA
    data.type = MessageType::READ_DATA;

    // Clear CH_ENABLE if CH_SETPOINT is not set
    if (!prepare_data_out_(MessageId::CH_SETPOINT, temp) || !(temp.f88() > 0.0)) {
      clear_bit(data.valueHB, STATUS_HB_CH_ENABLE);
    }

    // Clear COOLING_ENABLE if COOLING_CONTROL is not set
    if (!prepare_data_out_(MessageId::COOLING_CONTROL, temp) || !(temp.f88() > 0.0)) {
      clear_bit(data.valueHB, STATUS_HB_COOLING_ENABLE);
    }

    // Clear CH2_ENABLE if CH2_SETPOINT is not set
    if (!prepare_data_out_(MessageId::CH2_SETPOINT, temp) || !(temp.f88() > 0.0)) {
      clear_bit(data.valueHB, STATUS_HB_CH2_ENABLE);
    }

    return data;
  }

  if (!prepare_data_out_(request_id, data)) {
    // If we get here, a message was requested which somehow wasn't handled.
    // This shouldn't happen due to the way things are configured, so we
    // log an error and just return a 0 message.
    ESP_LOGE(TAG,
             "Tried to create a request with unexpected id %d (%s). "
             "This should never happen, so please open an issue.",
             request_id, message_id_to_str(request_id));
    return {};
  }

  return data;
}

bool OpenthermHub::prepare_data_out_(MessageId request_id, OpenthermData &data) const {
  auto range = this->message_processors_.equal_range(request_id);
  if (range.first == range.second) {
    // No MessageProcessor for this MessageId
    return false;
  }

  data.id = request_id;

  for (auto it = range.first; it != range.second; ++it) {
    const auto *item = it->second;
    item->prepare_data_out(data);
  }

  return true;
}

OpenthermHub::OpenthermHub() : Component(), in_pin_{}, out_pin_{} {}

void OpenthermHub::process_response(OpenthermData &data) {
  ESP_LOGD(TAG, "Received OpenTherm response with id %d (%s)", data.id, message_id_to_str((MessageId) data.id));
  debug_data(data);

  auto range = this->message_processors_.equal_range((MessageId) data.id);
  for (auto it = range.first; it != range.second; ++it) {
    auto *item = it->second;
    item->parse_and_publish(data);
  }
}

void OpenthermHub::setup() {
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
  this->write_initial_messages_(this->messages_);
  this->message_iterator_ = this->messages_.begin();
}

void OpenthermHub::on_shutdown() { this->opentherm_->stop(); }

// Disabling clang-tidy for this particular line since it keeps removing the trailing underscore (bug?)
void OpenthermHub::write_initial_messages_(std::vector<MessageId> &target) {  // NOLINT
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
void OpenthermHub::write_repeating_messages_(std::vector<MessageId> &target) {  // NOLINT
  target.clear();
  for (auto const &pair : this->configured_messages_) {
    if (pair.second == REPEATING_MESSAGE_ORDER) {
      target.push_back(pair.first);
    }
  }
}

void OpenthermHub::loop() {
  if (this->sync_mode_) {
    this->sync_loop_();
    return;
  }

  auto cur_time = App.get_loop_component_start_time();
  auto const cur_mode = this->opentherm_->get_mode();

  if (this->handle_error_(cur_mode)) {
    return;
  }

  switch (cur_mode) {
    case OperationMode::WRITE:
    case OperationMode::READ:
      break;
    case OperationMode::LISTEN:
      if (this->last_conversation_start_ > 0 && (cur_time - this->last_conversation_start_) > 1150) {
        ESP_LOGE(TAG, "Hub timeout triggered during receive");
        this->stop_opentherm_();
      }
      break;
    case OperationMode::IDLE:
      this->check_timings_(cur_time);
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
    default:
      break;
  }
}

bool OpenthermHub::handle_error_(OperationMode mode) {
  switch (mode) {
    case OperationMode::ERROR_PROTOCOL:
      // Protocol error can happen only while reading boiler response.
      this->handle_protocol_error_();
      return true;
    case OperationMode::ERROR_TIMEOUT:
      // Timeout error might happen while we wait for device to respond.
      this->handle_timeout_error_();
      return true;
    case OperationMode::ERROR_RMT:
      // RMT error can occur when decoding boiler response from low-level RMT symbols.
      this->handle_rmt_error_();
      return true;
    default:
      return false;
  }
}

void OpenthermHub::sync_loop_() {
  if (!this->opentherm_->is_idle()) {
    ESP_LOGE(TAG, "OpenTherm is not idle at the start of the loop");
    return;
  }

  auto cur_time = App.get_loop_component_start_time();

  this->check_timings_(cur_time);

  if (this->should_skip_loop_(cur_time)) {
    return;
  }

  this->start_conversation_();

  // This is not neccessary now, since RMT transmission waits for completion in `opentherm.cpp`.
  // But in the future we will make it async again so we don't remove this block.
  // Spin while message is being sent to device
  if (!this->spin_wait_(1150, [&] { return this->opentherm_->is_active(); })) {
    ESP_LOGE(TAG, "Hub timeout triggered during send");
    this->stop_opentherm_();
    return;
  }

  // Check for errors and ensure we are in the right state (message sent successfully)
  if (this->handle_error_(this->opentherm_->get_mode())) {
    return;
  } else if (!this->opentherm_->is_sent()) {
    ESP_LOGW(TAG, "Unexpected state after sending request: %s", operation_mode_to_str(this->opentherm_->get_mode()));
    this->stop_opentherm_();
    return;
  }

  // Listen for the response
  this->opentherm_->listen();
  // There may be a timer error at this point
  if (this->handle_error_(this->opentherm_->get_mode())) {
    return;
  }

  // Spin while response is being received
  if (!this->spin_wait_(1150, [&] { return this->opentherm_->is_active(); })) {
    ESP_LOGE(TAG, "Hub timeout triggered during receive");
    this->stop_opentherm_();
    return;
  }

  // Check for errors and ensure we are in the right state (message received successfully)
  if (this->handle_error_(this->opentherm_->get_mode())) {
    return;
  } else if (!this->opentherm_->has_message()) {
    ESP_LOGW(TAG, "Unexpected state after receiving response: %s", operation_mode_to_str(this->opentherm_->get_mode()));
    this->stop_opentherm_();
    return;
  }

  this->read_response_();
}

void OpenthermHub::check_timings_(uint32_t cur_time) {
  if (this->last_conversation_start_ > 0 && (cur_time - this->last_conversation_start_) > 1150) {
    ESP_LOGW(TAG,
             "%d ms elapsed since the start of the last convo, but 1150 ms are allowed at maximum. Look at other "
             "components that might slow the loop down.",
             (int) (cur_time - this->last_conversation_start_));
  }
}

bool OpenthermHub::should_skip_loop_(uint32_t cur_time) const {
  if (this->last_conversation_end_ > 0 && (cur_time - this->last_conversation_end_) < 100) {
    ESP_LOGV(TAG, "Less than 100 ms elapsed since last convo, skipping this iteration");
    return true;
  }

  return false;
}

void OpenthermHub::start_conversation_() {
  if (this->message_iterator_ == this->messages_.end()) {
    if (this->sending_initial_) {
      this->sending_initial_ = false;
      this->write_repeating_messages_(this->messages_);
    }
    this->message_iterator_ = this->messages_.begin();
  }

  auto request = this->build_request_(*this->message_iterator_);

  this->before_send_callback_.call(request);

  this->last_conversation_start_ = App.get_loop_component_start_time();

  switch (request.type) {
    case MessageType::READ_DATA:
    case MessageType::WRITE_DATA:
      ESP_LOGD(TAG, "Sending request with id %d (%s)", request.id, message_id_to_str((MessageId) request.id));
      debug_data(request);
      // Send the request
      this->opentherm_->send(request);
      break;

    case MessageType::INVALID_DATA:
      ESP_LOGV(TAG, "Skipping sending request with id %d (%s): invalid data", request.id,
               message_id_to_str((MessageId) request.id));
      this->last_conversation_end_ = App.get_loop_component_start_time();
      break;

    default:
      ESP_LOGE(TAG, "Refusing to send bad message type %d with id %d (%s)", request.type, request.id,
               message_id_to_str((MessageId) request.id));
      this->last_conversation_end_ = App.get_loop_component_start_time();
  }
}

void OpenthermHub::read_response_() {
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

void OpenthermHub::stop_opentherm_() {
  this->opentherm_->stop();
  this->last_conversation_end_ = App.get_loop_component_start_time();
}

void OpenthermHub::handle_protocol_error_() {
  auto error_type = this->opentherm_->get_protocol_error_type();
  ESP_LOGW(TAG, "OpenTherm protocol error: %s", protocol_error_to_str(error_type));
  this->opentherm_->log_protocol_state();
  this->stop_opentherm_();
}

void OpenthermHub::handle_timeout_error_() {
  ESP_LOGW(TAG, "Timeout while waiting for response from device");
  this->stop_opentherm_();
}

void OpenthermHub::handle_rmt_error_() {
  ESP_LOGW(TAG, "OpenTherm RMT error");
  this->stop_opentherm_();
}

void OpenthermHub::dump_config() {
  std::vector<MessageId> initial_messages;
  std::vector<MessageId> repeating_messages;
  this->write_initial_messages_(initial_messages);
  this->write_repeating_messages_(repeating_messages);

  ESP_LOGCONFIG(TAG, "OpenTherm:");
  LOG_PIN("  In: ", this->in_pin_);
  LOG_PIN("  Out: ", this->out_pin_);
  ESP_LOGCONFIG(TAG, "  Sync mode: %s\n", YESNO(this->sync_mode_));
  ESP_LOGCONFIG(TAG, "  Child components (%d):", this->message_processors_.size());
  for (const auto &pair : this->message_processors_) {
    const auto type = pair.first;
    const auto *child = pair.second;
    ESP_LOGCONFIG(TAG, "  - %s: %s => %d (%s)", child->get_type_name(), child->get_id(), type, message_id_to_str(type));
  }
  ESP_LOGCONFIG(TAG, "  Initial requests:");
  for (auto type : initial_messages) {
    ESP_LOGCONFIG(TAG, "  - %d (%s)", type, message_id_to_str(type));
  }
  ESP_LOGCONFIG(TAG, "  Repeating requests:");
  for (auto type : repeating_messages) {
    ESP_LOGCONFIG(TAG, "  - %d (%s)", type, message_id_to_str(type));
  }
}

}  // namespace opentherm
}  // namespace esphome
