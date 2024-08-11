#include "hub.h"

#include <string>

// Disable incomplete switch statement warnings, because the cases in each
// switch are generated based on the configured sensors and inputs.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

namespace esphome {
namespace opentherm {

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
    // NOLINTBEGIN
    bool const ch_enabled = this->ch_enable &&
#ifdef OPENTHERM_READ_ch_enable
                            OPENTHERM_READ_ch_enable
#else
                            true
#endif
                            &&
#ifdef OPENTHERM_READ_t_set
                            OPENTHERM_READ_t_set > 0.0
#else
                            true
#endif
        ;

    bool dhw_enabled = this->dhw_enable &&
#ifdef OPENTHERM_READ_dhw_enable
                       OPENTHERM_READ_dhw_enable
#else
                       true
#endif
        ;
    bool cooling_enabled = this->cooling_enable &&
#ifdef OPENTHERM_READ_cooling_enable
                           OPENTHERM_READ_cooling_enable
#else
                           true
#endif
                           &&
#ifdef OPENTHERM_READ_cooling_control
                           OPENTHERM_READ_cooling_control > 0.0
#else
                           true
#endif
        ;
    bool otc_enabled = this->otc_active &&
#ifdef OPENTHERM_READ_otc_active
                       OPENTHERM_READ_otc_active
#else
                       true
#endif
        ;
    bool ch2_enabled = this->ch2_active &&
#ifdef OPENTHERM_READ_ch2_active
                       OPENTHERM_READ_ch2_active
#else
                       true
#endif
                       &&
#ifdef OPENTHERM_READ_t_set_ch2
                       OPENTHERM_READ_t_set_ch2 > 0.0
#else
                       true
#endif
        ;
    // NOLINTEND

    data.type = MessageType::READ_DATA;
    data.id = MessageId::STATUS;
    data.valueHB = ch_enabled | (dhw_enabled << 1) | (cooling_enabled << 2) | (otc_enabled << 3) | (ch2_enabled << 4);

    return data;
  }
  return OpenthermData();
}

OpenthermHub::OpenthermHub() : Component() {}

void OpenthermHub::process_response(OpenthermData &data) {
  ESP_LOGD(OT_TAG, "Received OpenTherm response with id %d (%s)", data.id,
           opentherm_->message_id_to_str((MessageId) data.id));
  ESP_LOGD(OT_TAG, "%s", opentherm_->debug_data(data).c_str());
}

void OpenthermHub::setup() {
  ESP_LOGD(OT_TAG, "Setting up OpenTherm component");
  this->opentherm_ = new OpenTherm(this->in_pin_, this->out_pin_);  // NOLINT because hub is never deleted
  if (!this->opentherm_->initialize()) {
    ESP_LOGE(OT_TAG, "Failed to initialize OpenTherm protocol. See previous log messages for details.");
    return;
  }

  // Ensure that there is at least one request, as we are required to
  // communicate at least once every second. Sending the status request is
  // good practice anyway.
  this->add_repeating_message(MessageId::STATUS);

  this->current_message_iterator_ = this->initial_messages_.begin();
  initialized_ = true;
}

void OpenthermHub::on_shutdown() { this->opentherm_->stop(); }

void OpenthermHub::loop() {
  if (!initialized_)
    return;

  if (sync_mode_) {
    sync_loop_();
    return;
  }

  auto cur_time = millis();
  auto const cur_mode = opentherm_->get_mode();
  switch (cur_mode) {
    case OperationMode::WRITE:
    case OperationMode::READ:
    case OperationMode::LISTEN:
      if (!check_timings_(cur_time)) {
        break;
      }
      last_mode_ = cur_mode;
      break;
    case OperationMode::ERROR_PROTOCOL:
      if (last_mode_ == OperationMode::WRITE) {
        handle_protocol_write_error_();
      } else if (last_mode_ == OperationMode::READ) {
        handle_protocol_read_error_();
      }

      stop_opentherm_();
      break;
    case OperationMode::ERROR_TIMEOUT:
      handle_timeout_error_();
      stop_opentherm_();
      break;
    case OperationMode::IDLE:
      if (should_skip_loop_(cur_time)) {
        break;
      }
      start_conversation_();
      break;
    case OperationMode::SENT:
      // Message sent, now listen for the response.
      opentherm_->listen();
      break;
    case OperationMode::RECEIVED:
      read_response_();
      break;
  }
}

void OpenthermHub::sync_loop_() {
  if (!this->opentherm_->is_idle()) {
    ESP_LOGE(OT_TAG, "OpenTherm is not idle at the start of the loop");
    return;
  }

  auto cur_time = millis();

  check_timings_(cur_time);

  if (should_skip_loop_(cur_time)) {
    return;
  }

  start_conversation_();

  if (!spin_wait_(1150, [&] { return opentherm_->is_active(); })) {
    ESP_LOGE(OT_TAG, "Hub timeout triggered during send");
    stop_opentherm_();
    return;
  }

  if (opentherm_->is_error()) {
    handle_protocol_write_error_();
    stop_opentherm_();
    return;
  } else if (!opentherm_->is_sent()) {
    ESP_LOGW(OT_TAG, "Unexpected state after sending request: %s",
             opentherm_->operation_mode_to_str(opentherm_->get_mode()));
    stop_opentherm_();
    return;
  }

  // Listen for the response
  opentherm_->listen();
  if (!spin_wait_(1150, [&] { return opentherm_->is_active(); })) {
    ESP_LOGE(OT_TAG, "Hub timeout triggered during receive");
    opentherm_->stop();
    last_conversation_end_ = millis();
    return;
  }

  if (opentherm_->is_timeout()) {
    handle_timeout_error_();
    stop_opentherm_();
    return;
  } else if (opentherm_->is_protocol_error()) {
    handle_protocol_read_error_();
    stop_opentherm_();
    return;
  } else if (!opentherm_->has_message()) {
    ESP_LOGW(OT_TAG, "Unexpected state after receiving response: %s",
             opentherm_->operation_mode_to_str(opentherm_->get_mode()));
    stop_opentherm_();
    return;
  }

  read_response_();
}

bool OpenthermHub::check_timings_(uint32_t cur_time) {
  if (last_conversation_start_ > 0 && (cur_time - last_conversation_start_) > 1150) {
    ESP_LOGW(OT_TAG,
             "%d ms elapsed since the start of the last convo, but 1150 ms are allowed at maximum. Look at other "
             "components that might slow the loop down.",
             (int) (cur_time - last_conversation_start_));
    stop_opentherm_();
    return false;
  }

  return true;
}

bool OpenthermHub::should_skip_loop_(uint32_t cur_time) const {
  if (last_conversation_end_ > 0 && (cur_time - last_conversation_end_) < 100) {
    ESP_LOGV(OT_TAG, "Less than 100 ms elapsed since last convo, skipping this iteration");
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

  ESP_LOGD(OT_TAG, "Sending request with id %d (%s)", request.id,
           opentherm_->message_id_to_str((MessageId) request.id));
  ESP_LOGD(OT_TAG, "%s", opentherm_->debug_data(request).c_str());
  // Send the request
  last_conversation_start_ = millis();
  opentherm_->send(request);
}

void OpenthermHub::read_response_() {
  OpenthermData response;
  if (!opentherm_->get_message(response)) {
    ESP_LOGW(OT_TAG, "Couldn't get the response, but flags indicated success. This is a bug.");
    stop_opentherm_();
    return;
  }

  stop_opentherm_();

  process_response(response);

  this->current_message_iterator_++;
}

void OpenthermHub::stop_opentherm_() {
  opentherm_->stop();
  last_conversation_end_ = millis();
}

void OpenthermHub::handle_protocol_write_error_() {
  ESP_LOGW(OT_TAG, "Error while sending request: %s", opentherm_->operation_mode_to_str(opentherm_->get_mode()));
  ESP_LOGW(OT_TAG, "%s", opentherm_->debug_data(last_request_).c_str());
}

void OpenthermHub::handle_protocol_read_error_() {
  OpenThermError error;
  opentherm_->get_protocol_error(error);
  ESP_LOGW(OT_TAG, "Protocol error occured while receiving response: %s", opentherm_->debug_error(error).c_str());
}

void OpenthermHub::handle_timeout_error_() {
  ESP_LOGW(OT_TAG, "Receive response timed out at a protocol level");
  stop_opentherm_();
}

#define ID(x) x
#define SHOW2(x) #x
#define SHOW(x) SHOW2(x)

void OpenthermHub::dump_config() {
  ESP_LOGCONFIG(OT_TAG, "OpenTherm:");
  ESP_LOGCONFIG(OT_TAG, "  In: GPIO%d", this->in_pin_->get_pin());
  ESP_LOGCONFIG(OT_TAG, "  Out: GPIO%d", this->out_pin_->get_pin());
  ESP_LOGCONFIG(OT_TAG, "  Sync mode: %d", this->sync_mode_);
  ESP_LOGCONFIG(OT_TAG, "  Sensors: %s", SHOW(OPENTHERM_SENSOR_LIST(ID, )));
  ESP_LOGCONFIG(OT_TAG, "  Binary sensors: %s", SHOW(OPENTHERM_BINARY_SENSOR_LIST(ID, )));
  ESP_LOGCONFIG(OT_TAG, "  Switches: %s", SHOW(OPENTHERM_SWITCH_LIST(ID, )));
  ESP_LOGCONFIG(OT_TAG, "  Input sensors: %s", SHOW(OPENTHERM_INPUT_SENSOR_LIST(ID, )));
  ESP_LOGCONFIG(OT_TAG, "  Outputs: %s", SHOW(OPENTHERM_OUTPUT_LIST(ID, )));
  ESP_LOGCONFIG(OT_TAG, "  Numbers: %s", SHOW(OPENTHERM_NUMBER_LIST(ID, )));
  ESP_LOGCONFIG(OT_TAG, "  Initial requests:");
  for (auto type : this->initial_messages_) {
    ESP_LOGCONFIG(OT_TAG, "  - %d", type);
  }
  ESP_LOGCONFIG(OT_TAG, "  Repeating requests:");
  for (auto type : this->repeating_messages_) {
    ESP_LOGCONFIG(OT_TAG, "  - %d", type);
  }
}

}  // namespace opentherm
}  // namespace esphome

#pragma GCC diagnostic pop
