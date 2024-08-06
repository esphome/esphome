#ifdef USE_ARDUINO

#include "esphome/core/log.h"
#include "optolink_text_sensor.h"
#include "../optolink.h"
#include "../datapoint_component.h"
#include "../helpers.h"

namespace esphome {
namespace optolink {

static const char *const TAG = "optolink.text_sensor";

void OptolinkTextSensor::setup() {
  switch (type_) {
    case TEXT_SENSOR_TYPE_MAP:
      break;
    case TEXT_SENSOR_TYPE_RAW:
      set_div_ratio(0);
      break;
    case TEXT_SENSOR_TYPE_DAY_SCHEDULE:
      set_div_ratio(0);
      set_bytes(8);
      set_address(get_address_() + 8 * dow_);
      break;
    case TEXT_SENSOR_TYPE_DEVICE_INFO:
      set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
      set_bytes(4);
      set_address(0x00f8);
      break;
    case TEXT_SENSOR_TYPE_STATE_INFO:
      set_entity_category(esphome::ENTITY_CATEGORY_DIAGNOSTIC);
      return;  // no datapoint setup!
  }
  setup_datapoint_();
};

void OptolinkTextSensor::update() {
  if (type_ == TEXT_SENSOR_TYPE_STATE_INFO) {
    publish_state(optolink_->get_state());
  } else {
    datapoint_read_request_();
  }
}

void OptolinkTextSensor::datapoint_value_changed(uint8_t *value, size_t length) {
  switch (type_) {
    case TEXT_SENSOR_TYPE_RAW:
      publish_state(std::string((const char *) value));
      break;
    case TEXT_SENSOR_TYPE_DAY_SCHEDULE:
      if (length == 8) {
        auto schedule = decode_day_schedule(value);
        rtrim(schedule);
        publish_state(schedule);
      } else {
        unfitting_value_type_();
      }
      break;
    case TEXT_SENSOR_TYPE_DEVICE_INFO:
    case TEXT_SENSOR_TYPE_STATE_INFO:
    case TEXT_SENSOR_TYPE_MAP:
      unfitting_value_type_();
      break;
  }
};

void OptolinkTextSensor::datapoint_value_changed(uint32_t value) {
  switch (type_) {
    case TEXT_SENSOR_TYPE_DEVICE_INFO: {
      uint8_t *bytes = (uint8_t *) &value;
      uint16_t tmp = esphome::byteswap(*((uint16_t *) bytes));
      std::string geraetekennung = esphome::format_hex_pretty(&tmp, 1);
      std::string hardware_revision = esphome::format_hex_pretty((uint8_t *) bytes + 2, 1);
      std::string software_index = esphome::format_hex_pretty((uint8_t *) bytes + 3, 1);
      publish_state("Device ID: " + geraetekennung + "|Hardware Revision: " + hardware_revision +
                    "|Software Index: " + software_index);
    } break;
    default:
      publish_state(std::to_string(value));
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
