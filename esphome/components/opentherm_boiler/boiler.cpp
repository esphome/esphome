#include "boiler.h"
#include "esphome/components/opentherm/opentherm_base.h"
#include "esphome/core/application.h"

namespace esphome {
namespace opentherm_boiler {

using opentherm::OperationMode;
using opentherm::MessageType;

static const char *const TAG = "opentherm_boiler";

void Boiler::setup() {
  this->opentherm_ = make_unique<OpenTherm>(this->in_pin_, this->out_pin_);
  if (!this->opentherm_->initialize()) {
    ESP_LOGE(TAG, "Failed to initialize OpenTherm protocol. See previous log messages for details.");
    this->mark_failed();
    return;
  }
}

void Boiler::loop() {
  auto cur_time = App.get_loop_component_start_time();
  auto const mode = this->opentherm_->get_mode();

  if (mode != this->last_mode_) {
    this->last_mode_ = mode;
    this->last_mode_change_ = cur_time;
  } else if (mode == OperationMode::LISTEN) {
    // don't time out of LISTEN mode, we'll handle that sort of thing below
  } else if (this->last_mode_change_ > 0 && (cur_time - this->last_mode_change_) > 5000) {
    ESP_LOGE(TAG, "Stuck in mode %d", mode);
    this->opentherm_->stop();
    this->status_set_warning("Stuck state machine");
    return;
  }

  switch (mode) {
    case OperationMode::IDLE:
    case OperationMode::SENT:
      if (this->response_enqueued_) {
        // Send the response, unless it's too early
        this->transmit_response_();
      } else {
        // Listen for incoming messages from the thermostat
        this->opentherm_->listen();
      }
      break;

    case OperationMode::LISTEN:
      // TODO: communications fault
      // TODO: short-circuit feature
      break;

    case OperationMode::RECEIVED:
      this->status_clear_warning();
      this->read_request_();
      break;

    case OperationMode::ERROR_PROTOCOL: {
      auto error_type = this->opentherm_->get_protocol_error_type();
      ESP_LOGW(TAG, "OpenTherm protocol error: %s", protocol_error_to_str(error_type));
      this->opentherm_->log_protocol_state();
      this->opentherm_->stop();
      this->status_set_warning("Protocol error");
      break;
    }

    case OperationMode::ERROR_TIMEOUT:
      this->opentherm_->stop();
      this->status_set_warning("Timeout error");
      break;

    case OperationMode::ERROR_RMT:
      ESP_LOGW(TAG, "OpenTherm RMT error");
      this->opentherm_->stop();
      this->status_set_warning("RMT error");
      break;

    default:
      break;
  }
}

void Boiler::dump_config() {
  ESP_LOGCONFIG(TAG, "OpenTherm Boiler:");
  LOG_PIN("  In: ", this->in_pin_);
  LOG_PIN("  Out: ", this->out_pin_);
  ESP_LOGCONFIG(TAG, "  Child components (%d):", this->request_processors_.size());
  for (const auto &pair : this->request_processors_) {
    const auto type = pair.first;
    const auto *child = pair.second;
    ESP_LOGCONFIG(TAG, "  - %s: %s => %d (%s)", child->get_type_name(), child->get_id(), type, message_id_to_str(type));
  }
}

void Boiler::on_shutdown() { this->opentherm_->stop(); }

void Boiler::read_request_() {
  OpenthermData data;

  if (!this->opentherm_->get_message(data)) {
    ESP_LOGW(TAG, "Couldn't get the request, but flags indicated success. This is a bug.");
    this->opentherm_->stop();
    this->status_set_error("BUG");
    return;
  }

  this->opentherm_->stop();
  this->last_rx_ = App.get_loop_component_start_time();

  this->process_request_(data);

  this->response_ = data;
  this->response_enqueued_ = true;
}

void Boiler::process_request_(OpenthermData &data) {
  bool handled = false;

  ESP_LOGD(TAG, "Received OpenTherm request with id %d (%s)", data.id, message_id_to_str((MessageId) data.id));
  debug_data(data);

  this->on_receive_trigger_.call(data);

  auto range = this->request_processors_.equal_range((MessageId) data.id);
  if (range.first == range.second) {
    // No RequestProcessor for this MessageId
    data.type = MessageType::UNKNOWN_DATAID;
    return;
  }

  for (auto it = range.first; it != range.second; ++it) {
    auto *item = it->second;
    handled |= item->handle_request(data);
    yield();
  }

  if (handled) {
    switch (data.type) {
      case MessageType::READ_DATA:
        data.type = MessageType::READ_ACK;
        return;
        break;
      case MessageType::WRITE_DATA:
        data.type = MessageType::WRITE_ACK;
        return;
        break;
      default:
        break;
    }
  }

  data.type = MessageType::DATA_INVALID;
}

void Boiler::transmit_response_() {
  if (!this->response_enqueued_) {
    return;
  }

  // We need to wait a minimum of 20ms between receiving the request and transmitting our
  // response.
  auto now = App.get_loop_component_start_time();
  if (now < this->last_rx_ + 20) {
    return;
  }

  this->before_transmit_trigger_.call(this->response_);

  ESP_LOGD(TAG, "Sending OpenTherm response with id %d (%s)", this->response_.id,
           message_id_to_str((MessageId) this->response_.id));
  debug_data(this->response_);

  this->response_enqueued_ = false;
  this->opentherm_->send(this->response_);
}

}  // namespace opentherm_boiler
}  // namespace esphome
