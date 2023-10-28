#include "hub.h"

// Disable incomplete switch statement warnings, because the cases in each
// switch are generated based on the configured sensors and inputs.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

namespace esphome {
namespace opentherm {

namespace message_data {
bool parse_flag8_lb_0(const uint32_t response) { return response & 0b0000000000000001; }
bool parse_flag8_lb_1(const uint32_t response) { return response & 0b0000000000000010; }
bool parse_flag8_lb_2(const uint32_t response) { return response & 0b0000000000000100; }
bool parse_flag8_lb_3(const uint32_t response) { return response & 0b0000000000001000; }
bool parse_flag8_lb_4(const uint32_t response) { return response & 0b0000000000010000; }
bool parse_flag8_lb_5(const uint32_t response) { return response & 0b0000000000100000; }
bool parse_flag8_lb_6(const uint32_t response) { return response & 0b0000000001000000; }
bool parse_flag8_lb_7(const uint32_t response) { return response & 0b0000000010000000; }
bool parse_flag8_hb_0(const uint32_t response) { return response & 0b0000000100000000; }
bool parse_flag8_hb_1(const uint32_t response) { return response & 0b0000001000000000; }
bool parse_flag8_hb_2(const uint32_t response) { return response & 0b0000010000000000; }
bool parse_flag8_hb_3(const uint32_t response) { return response & 0b0000100000000000; }
bool parse_flag8_hb_4(const uint32_t response) { return response & 0b0001000000000000; }
bool parse_flag8_hb_5(const uint32_t response) { return response & 0b0010000000000000; }
bool parse_flag8_hb_6(const uint32_t response) { return response & 0b0100000000000000; }
bool parse_flag8_hb_7(const uint32_t response) { return response & 0b1000000000000000; }
uint8_t parse_u8_lb(const uint32_t response) { return (uint8_t) (response & 0xff); }
uint8_t parse_u8_hb(const uint32_t response) { return (uint8_t) ((response >> 8) & 0xff); }
int8_t parse_s8_lb(const uint32_t response) { return (int8_t) (response & 0xff); }
int8_t parse_s8_hb(const uint32_t response) { return (int8_t) ((response >> 8) & 0xff); }
uint16_t parse_u16(const uint32_t response) { return (uint16_t) (response & 0xffff); }
int16_t parse_s16(const uint32_t response) { return (int16_t) (response & 0xffff); }
float parse_f88(const uint32_t response) {
  int16_t const data = response & 0xffff;
  return (data / 256.0f);
}

unsigned int write_flag8_lb_0(const bool value, const unsigned int data) {
  return value ? data | 0b0000000000000001 : data & 0b1111111111111110;
}
unsigned int write_flag8_lb_1(const bool value, const unsigned int data) {
  return value ? data | 0b0000000000000010 : data & 0b1111111111111101;
}
unsigned int write_flag8_lb_2(const bool value, const unsigned int data) {
  return value ? data | 0b0000000000000100 : data & 0b1111111111111011;
}
unsigned int write_flag8_lb_3(const bool value, const unsigned int data) {
  return value ? data | 0b0000000000001000 : data & 0b1111111111110111;
}
unsigned int write_flag8_lb_4(const bool value, const unsigned int data) {
  return value ? data | 0b0000000000010000 : data & 0b1111111111101111;
}
unsigned int write_flag8_lb_5(const bool value, const unsigned int data) {
  return value ? data | 0b0000000000100000 : data & 0b1111111111011111;
}
unsigned int write_flag8_lb_6(const bool value, const unsigned int data) {
  return value ? data | 0b0000000001000000 : data & 0b1111111110111111;
}
unsigned int write_flag8_lb_7(const bool value, const unsigned int data) {
  return value ? data | 0b0000000010000000 : data & 0b1111111101111111;
}
unsigned int write_flag8_hb_0(const bool value, const unsigned int data) {
  return value ? data | 0b0000000100000000 : data & 0b1111111011111111;
}
unsigned int write_flag8_hb_1(const bool value, const unsigned int data) {
  return value ? data | 0b0000001000000000 : data & 0b1111110111111111;
}
unsigned int write_flag8_hb_2(const bool value, const unsigned int data) {
  return value ? data | 0b0000010000000000 : data & 0b1111101111111111;
}
unsigned int write_flag8_hb_3(const bool value, const unsigned int data) {
  return value ? data | 0b0000100000000000 : data & 0b1111011111111111;
}
unsigned int write_flag8_hb_4(const bool value, const unsigned int data) {
  return value ? data | 0b0001000000000000 : data & 0b1110111111111111;
}
unsigned int write_flag8_hb_5(const bool value, const unsigned int data) {
  return value ? data | 0b0010000000000000 : data & 0b1101111111111111;
}
unsigned int write_flag8_hb_6(const bool value, const unsigned int data) {
  return value ? data | 0b0100000000000000 : data & 0b1011111111111111;
}
unsigned int write_flag8_hb_7(const bool value, const unsigned int data) {
  return value ? data | 0b1000000000000000 : data & 0b0111111111111111;
}
unsigned int write_u8_lb(const uint8_t value, const unsigned int data) { return (data & 0xff00) | value; }
unsigned int write_u8_hb(const uint8_t value, const unsigned int data) { return (data & 0x00ff) | (value << 8); }
unsigned int write_s8_lb(const int8_t value, const unsigned int data) { return (data & 0xff00) | value; }
unsigned int write_s8_hb(const int8_t value, const unsigned int data) { return (data & 0x00ff) | (value << 8); }
unsigned int write_u16(const uint16_t value, const unsigned int data) { return value; }
unsigned int write_s16(const int16_t value, const unsigned int data) { return value; }
unsigned int write_f88(const float value, const unsigned int data) { return (unsigned int) (value * 256.0f); }
}  // namespace message_data

#define OPENTHERM_IGNORE_1(x)
#define OPENTHERM_IGNORE_2(x, y)

unsigned int OpenthermHub::build_request_(OpenThermMessageID request_id) {
  // First, handle the status request. This requires special logic, because we
  // wouldn't want to inadvertently disable domestic hot water, for example.
  // It is also included in the macro-generated code below, but that will
  // never be executed, because we short-circuit it here.
  if (request_id == OpenThermMessageID::Status) {
    ESP_LOGD(OT_TAG, "Building Status request");
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
    bool otc_active_ = this->otc_active &&
#ifdef OPENTHERM_READ_otc_active
                       OPENTHERM_READ_otc_active
#else
                       true
#endif
        ;
    bool ch2_active_ = this->ch2_active &&
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
    return opentherm_->build_set_boiler_status_request(ch_enabled, dhw_enabled, cooling_enabled, otc_active_,
                                                       ch2_active_);
  }

// Next, we start with the write requests from switches and other inputs,
// because we would want to write that data if it is available, rather than
// request a read for that type (in the case that both read and write are
// supported).
#define OPENTHERM_MESSAGE_WRITE_MESSAGE(msg) \
  case OpenThermMessageID::msg: { \
    ESP_LOGD(TAG, "Building %s write request", #msg); \
    unsigned int data = 0;
#define OPENTHERM_MESSAGE_WRITE_ENTITY(key, msg_data) data = message_data::write_##msg_data(this->key->state, data);
#define OPENTHERM_MESSAGE_WRITE_POSTSCRIPT \
  return opentherm_->build_request(OpenThermMessageType::WRITE_DATA, request_id, data); \
  }
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
#define OPENTHERM_MESSAGE_READ_MESSAGE(msg) \
  case OpenThermMessageID::msg: \
    ESP_LOGD(TAG, "Building %s read request", #msg); \
    return opentherm_->build_request(OpenThermMessageType::READ_DATA, request_id, 0);
  switch (request_id) { OPENTHERM_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_READ_MESSAGE, OPENTHERM_IGNORE_2, , , ) }
  switch (request_id) {
    OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_READ_MESSAGE, OPENTHERM_IGNORE_2, , , )
  }

  // And if we get here, a message was requested which somehow wasn't handled.
  // This shouldn't happen due to the way the defines are configured, so we
  // log an error and just return a 0 message.
  ESP_LOGE(OT_TAG, "Tried to create a request with unknown id %d. This should never happen, so please open an issue.",
           request_id);
  return 0;
}

OpenthermHub::OpenthermHub(void (*process_response_callback)(uint32_t, OpenThermResponseStatus))
    : Component(), process_response_callback_(process_response_callback) {}

void OpenthermHub::process_response(uint32_t response, OpenThermResponseStatus status) {
  // Read the second byte of the response, which is the message id.
  uint8_t const id = (response >> 16 & 0xFF);
  // First check if the response is valid and short-circuit execution if it isn't.
  if (!opentherm_->is_valid_response(response)) {
    ESP_LOGW(OT_TAG, "Received invalid OpenTherm response (id: %u): %08x, status=%s", id, response,
             opentherm_->status_to_string(opentherm_->get_last_response_status()));
    return;
  }

  ESP_LOGD(OT_TAG, "Received OpenTherm response with id %d: %s", id, String(response, HEX).c_str());

// Define the handler helpers to publish the results to all sensors
#define OPENTHERM_MESSAGE_RESPONSE_MESSAGE(msg) \
  case OpenThermMessageID::msg: \
    ESP_LOGD(TAG, "Received %s response", #msg);
#define OPENTHERM_MESSAGE_RESPONSE_ENTITY(key, msg_data) \
  this->key->publish_state(message_data::parse_##msg_data(response));
#define OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT break;

  // Then use those to create a switch statement for each thing we would want
  // to report. We use a separate switch statement for each type, because some
  // messages include results for multiple types, like flags and a number.
  switch (id) {
    OPENTHERM_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_RESPONSE_MESSAGE, OPENTHERM_MESSAGE_RESPONSE_ENTITY, ,
                                      OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT, )
  }
  switch (id) {
    OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS(OPENTHERM_MESSAGE_RESPONSE_MESSAGE, OPENTHERM_MESSAGE_RESPONSE_ENTITY, ,
                                             OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT, )
  }
}

void OpenthermHub::setup() {
  ESP_LOGD(OT_TAG, "Setting up OpenTherm component");
  this->opentherm_ = new OpenTherm(this->in_pin_, this->out_pin_, false);  // NOLINT because hub is never deleted
  this->opentherm_->begin(this->process_response_callback_);

  // Ensure that there is at least one request, as we are required to
  // communicate at least once every second. Sending the status request is
  // good practice anyway.
  this->add_repeating_message(OpenThermMessageID::Status);

  this->current_message_iterator_ = this->initial_messages_.begin();
}

void OpenthermHub::on_shutdown() { this->opentherm_->end(); }

void OpenthermHub::loop() {
  if (this->opentherm_->is_ready()) {
    if (this->initializing_ && this->current_message_iterator_ == this->initial_messages_.end()) {
      this->initializing_ = false;
      this->current_message_iterator_ = this->repeating_messages_.begin();
    } else if (this->current_message_iterator_ == this->repeating_messages_.end()) {
      this->current_message_iterator_ = this->repeating_messages_.begin();
    }

    uint16_t const request = this->build_request_(*this->current_message_iterator_);
    if (this->sync_mode) {
      ESP_LOGD(OT_TAG, "Sending SYNC OpenTherm request: %s", String(request, HEX).c_str());
      this->opentherm_->send_request(request);
    } else {
      this->opentherm_->send_request_aync(request);
      ESP_LOGD(OT_TAG, "Sent OpenTherm request: %s", String(request, HEX).c_str());
    }
    this->current_message_iterator_++;
  }

  if (!this->sync_mode)
    this->opentherm_->process();
}

#define ID(x) x
#define SHOW2(x) #x
#define SHOW(x) SHOW2(x)

void OpenthermHub::dump_config() {
  ESP_LOGCONFIG(OT_TAG, "OpenTherm:");
  ESP_LOGCONFIG(OT_TAG, "  In: GPIO%d", this->in_pin_->get_pin());
  ESP_LOGCONFIG(OT_TAG, "  Out: GPIO%d", this->out_pin_->get_pin());
  ESP_LOGCONFIG(OT_TAG, "  Sync mode: %d", this->sync_mode);
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
