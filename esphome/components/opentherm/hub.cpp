#include "hub.h"
#include "esphome/core/helpers.h"

#include <string>

namespace esphome {
namespace opentherm {

static const char *const TAG = "opentherm";

OpenthermData OpenthermHub::build_request_(MessageId request_id) {
  OpenthermData data;
  data.type = 0;
  data.id = 0;
  data.valueHB = 0;
  data.valueLB = 0;

  // First, handle the status request. This requires special logic, because we
  // wouldn't want to inadvertently disable domestic hot water, for example.
  // It is also included in the macro-generated code below, but that will
  // never be executed, because we short-circuit it here.
  if (request_id == MessageId::STATUS) {
    bool const ch_enabled = this->ch_enable;
    bool dhw_enabled = this->dhw_enable;
    bool cooling_enabled = this->cooling_enable;
    bool otc_enabled = this->otc_active;
    bool ch2_enabled = this->ch2_active;

    data.type = MessageType::READ_DATA;
    data.id = MessageId::STATUS;
    data.valueHB = ch_enabled | (dhw_enabled << 1) | (cooling_enabled << 2) | (otc_enabled << 3) | (ch2_enabled << 4);

// Disable incomplete switch statement warnings, because the cases in each
// switch are generated based on the configured sensors and inputs.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

    // TODO: This is a placeholder for an auto-generated switch statement which builds request structure based on
    // which sensors are enabled in config.

#pragma GCC diagnostic pop

    return data;
  }
  return OpenthermData();
}

OpenthermHub::OpenthermHub() : Component() {}

void OpenthermHub::process_response(OpenthermData &data) {
  ESP_LOGD(TAG, "Received OpenTherm response with id %d (%s)", data.id,
           this->opentherm_->message_id_to_str((MessageId) data.id));
  ESP_LOGD(TAG, "%s", this->opentherm_->debug_data(data).c_str());
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

#define ID(x) x
#define SHOW2(x) #x
#define SHOW(x) SHOW2(x)

void OpenthermHub::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm:");
  LOG_PIN("  In: ", this->in_pin_);
  LOG_PIN("  Out: ", this->out_pin_);
  ESP_LOGCONFIG(TAG, "  Sync mode: %d", this->sync_mode_);
  ESP_LOGCONFIG(TAG, "  Initial requests:");
  for (auto type : this->initial_messages_) {
    ESP_LOGCONFIG(TAG, "  - %d", type);
  }
  ESP_LOGCONFIG(TAG, "  Repeating requests:");
  for (auto type : this->repeating_messages_) {
    ESP_LOGCONFIG(TAG, "  - %d", type);
  }
}

}  // namespace opentherm
}  // namespace esphome
