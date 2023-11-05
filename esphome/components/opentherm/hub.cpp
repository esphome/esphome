#include "hub.h"

#include <string>

// Disable incomplete switch statement warnings, because the cases in each
// switch are generated based on the configured sensors and inputs.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

namespace esphome {
namespace opentherm {

namespace message_data {
bool parse_flag8_lb_0(OpenthermData &data) { return bitRead(data.valueLB, 0); }
bool parse_flag8_lb_1(OpenthermData &data) { return bitRead(data.valueLB, 1); }
bool parse_flag8_lb_2(OpenthermData &data) { return bitRead(data.valueLB, 2); }
bool parse_flag8_lb_3(OpenthermData &data) { return bitRead(data.valueLB, 3); }
bool parse_flag8_lb_4(OpenthermData &data) { return bitRead(data.valueLB, 4); }
bool parse_flag8_lb_5(OpenthermData &data) { return bitRead(data.valueLB, 5); }
bool parse_flag8_lb_6(OpenthermData &data) { return bitRead(data.valueLB, 6); }
bool parse_flag8_lb_7(OpenthermData &data) { return bitRead(data.valueLB, 7); }
bool parse_flag8_hb_0(OpenthermData &data) { return bitRead(data.valueHB, 0); }
bool parse_flag8_hb_1(OpenthermData &data) { return bitRead(data.valueHB, 1); }
bool parse_flag8_hb_2(OpenthermData &data) { return bitRead(data.valueHB, 2); }
bool parse_flag8_hb_3(OpenthermData &data) { return bitRead(data.valueHB, 3); }
bool parse_flag8_hb_4(OpenthermData &data) { return bitRead(data.valueHB, 4); }
bool parse_flag8_hb_5(OpenthermData &data) { return bitRead(data.valueHB, 5); }
bool parse_flag8_hb_6(OpenthermData &data) { return bitRead(data.valueHB, 6); }
bool parse_flag8_hb_7(OpenthermData &data) { return bitRead(data.valueHB, 7); }
uint8_t parse_u8_lb(OpenthermData &data) { return data.valueLB; }
uint8_t parse_u8_hb(OpenthermData &data) { return data.valueHB; }
int8_t parse_s8_lb(OpenthermData &data) { return (int8_t) data.valueLB; }
int8_t parse_s8_hb(OpenthermData &data) { return (int8_t) data.valueHB; }
uint16_t parse_u16(OpenthermData &data) { return data.u16(); }
int16_t parse_s16(OpenthermData &data) { return data.s16(); }
float parse_f88(OpenthermData &data) { return data.f88(); }

void write_flag8_lb_0(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 0, value); }
void write_flag8_lb_1(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 1, value); }
void write_flag8_lb_2(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 2, value); }
void write_flag8_lb_3(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 3, value); }
void write_flag8_lb_4(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 4, value); }
void write_flag8_lb_5(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 5, value); }
void write_flag8_lb_6(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 6, value); }
void write_flag8_lb_7(const bool value, OpenthermData &data) { bitWrite(data.valueLB, 7, value); }
void write_flag8_hb_0(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 0, value); }
void write_flag8_hb_1(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 1, value); }
void write_flag8_hb_2(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 2, value); }
void write_flag8_hb_3(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 3, value); }
void write_flag8_hb_4(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 4, value); }
void write_flag8_hb_5(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 5, value); }
void write_flag8_hb_6(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 6, value); }
void write_flag8_hb_7(const bool value, OpenthermData &data) { bitWrite(data.valueHB, 7, value); }
void write_u8_lb(const uint8_t value, OpenthermData &data) { data.valueLB = value; }
void write_u8_hb(const uint8_t value, OpenthermData &data) { data.valueHB = value; }
void write_s8_lb(const int8_t value, OpenthermData &data) { data.valueLB = (uint8_t) value; }
void write_s8_hb(const int8_t value, OpenthermData &data) { data.valueHB = (uint8_t) value; }
void write_u16(const uint16_t value, OpenthermData &data) { data.u16(value); }
void write_s16(const int16_t value, OpenthermData &data) { data.s16(value); }
void write_f88(const float value, OpenthermData &data) { data.f88(value); }

}  // namespace message_data

#define OPENTHERM_IGNORE_1(x)
#define OPENTHERM_IGNORE_2(x, y)

OpenthermData OpenthermHub::build_request_(OpenThermMessageId request_id) {
  // First, handle the status request. This requires special logic, because we
  // wouldn't want to inadvertently disable domestic hot water, for example.
  // It is also included in the macro-generated code below, but that will
  // never be executed, because we short-circuit it here.
  if (request_id == OpenThermMessageId::STATUS) {
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

    OpenthermData data;
    data.type = OpenThermMessageType::READ_DATA;
    data.id = OpenThermMessageId::STATUS;
    data.valueHB = ch_enabled | (dhw_enabled << 1) | (cooling_enabled << 2) | (otc_enabled << 3) | (ch2_enabled << 4);
    data.valueLB = 0;

    return data;
  }

// Next, we start with the write requests from switches and other inputs,
// because we would want to write that data if it is available, rather than
// request a read for that type (in the case that both read and write are
// supported).
#define OPENTHERM_MESSAGE_WRITE_MESSAGE(msg) \
  case OpenThermMessageId::msg: { \
    ESP_LOGD(OT_TAG, "Building %s write request", #msg); \
    OpenthermData data; \
    data.type = OpenThermMessageType::WRITE_DATA; \
    data.id = request_id; \
    data.valueHB = 0; \
    data.valueLB = 0;
#define OPENTHERM_MESSAGE_WRITE_ENTITY(key, msg_data) message_data::write_##msg_data(this->key->state, data);
#define OPENTHERM_MESSAGE_WRITE_POSTSCRIPT \
  return data; \
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
  case OpenThermMessageId::msg: \
    ESP_LOGD(OT_TAG, "Building %s read request", #msg); \
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
  return OpenthermData();
}

OpenthermHub::OpenthermHub() : Component() {}

void OpenthermHub::process_response(OpenthermData &data) {
  // TODO: Debug print the response
  // ESP_LOGD(OT_TAG, "Received OpenTherm response with id %d: %s", id, int_to_hex(response).c_str());

// Define the handler helpers to publish the results to all sensors
#define OPENTHERM_MESSAGE_RESPONSE_MESSAGE(msg) \
  case OpenThermMessageId::msg: \
    ESP_LOGD(OT_TAG, "Received %s response", #msg);
#define OPENTHERM_MESSAGE_RESPONSE_ENTITY(key, msg_data) \
  this->key->publish_state(message_data::parse_##msg_data(response));
#define OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT break;

  // Then use those to create a switch statement for each thing we would want
  // to report. We use a separate switch statement for each type, because some
  // messages include results for multiple types, like flags and a number.
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
  ESP_LOGD(OT_TAG, "Setting up OpenTherm component");
  this->opentherm_ = new OpenTherm(this->in_pin_, this->out_pin_);  // NOLINT because hub is never deleted
  this->opentherm_->begin();

  // Ensure that there is at least one request, as we are required to
  // communicate at least once every second. Sending the status request is
  // good practice anyway.
  this->add_repeating_message(OpenThermMessageId::STATUS);

  this->current_message_iterator_ = this->initial_messages_.begin();
}

void OpenthermHub::on_shutdown() { this->opentherm_->stop(); }

void OpenthermHub::loop() {
  if (!this->opentherm_->is_idle()) {
    ESP_LOGE(OT_TAG, "OpenTherm is not idle at the start of the loop");
    return;
  }

  if (this->initializing_ && this->current_message_iterator_ == this->initial_messages_.end()) {
    this->initializing_ = false;
    this->current_message_iterator_ = this->repeating_messages_.begin();
  } else if (this->current_message_iterator_ == this->repeating_messages_.end()) {
    this->current_message_iterator_ = this->repeating_messages_.begin();
  }

  auto request = this->build_request_(*this->current_message_iterator_);
  // TODO: Send request logic
  this->current_message_iterator_++;
}

#define ID(x) x
#define SHOW2(x) #x
#define SHOW(x) SHOW2(x)

void OpenthermHub::dump_config() {
  ESP_LOGCONFIG(OT_TAG, "OpenTherm:");
  ESP_LOGCONFIG(OT_TAG, "  In: GPIO%d", this->in_pin_->get_pin());
  ESP_LOGCONFIG(OT_TAG, "  Out: GPIO%d", this->out_pin_->get_pin());
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
