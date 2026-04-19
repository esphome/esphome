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
      set_div_ratio(DIV_RATIO_RAW);
      break;
    case TEXT_SENSOR_TYPE_DAY_SCHEDULE:
      set_div_ratio(DIV_RATIO_BINARY);
      set_bytes(8);
      set_address(get_address_() + 8 * dow_);
      break;
    case TEXT_SENSOR_TYPE_DATETIME:
      set_writeable(true);
      set_bytes(8);
      set_div_ratio(DIV_RATIO_BINARY);
      break;
    case TEXT_SENSOR_TYPE_DEVICE_INFO:
      set_bytes(4);
      set_address(0x00f8);
      break;
    case TEXT_SENSOR_TYPE_STATE_INFO:
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

void OptolinkTextSensor::datapoint_value_changed(float value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%g", value);
  publish_state(buf);
}

void OptolinkTextSensor::datapoint_value_changed(uint8_t value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u", value);
  publish_state(buf);
}

void OptolinkTextSensor::datapoint_value_changed(uint16_t value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%u", value);
  publish_state(buf);
}

void OptolinkTextSensor::datapoint_value_changed(const std::string &value) {
  switch (type_) {
    case TEXT_SENSOR_TYPE_RAW:
      publish_state(value);
      break;
    default:
      unfitting_value_type_();
      break;
  }
}

void OptolinkTextSensor::datapoint_value_changed(uint8_t *value, size_t length) {
  switch (type_) {
    case TEXT_SENSOR_TYPE_DAY_SCHEDULE:
      if (length == 8) {
        auto schedule = decode_day_schedule(value);
        rtrim(schedule);
        publish_state(schedule);
      } else {
        unfitting_value_type_();
      }
      break;
    case TEXT_SENSOR_TYPE_DATETIME:
      if (length == 8) {
        auto datetime = decode_datetime(value, length);
        publish_state(datetime);
      } else {
        unfitting_value_type_();
      }
      break;
    case TEXT_SENSOR_TYPE_RAW:
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
      std::string result = "Device ID: ";
      uint8_t *bytes = (uint8_t *) &value;
      uint16_t tmp = esphome::byteswap(*((uint16_t *) bytes));
      char deviceId[format_hex_pretty_uint16_size(1)];
      format_hex_pretty_to(deviceId, &tmp, 1);
      result.append(deviceId);
      result.append("|Hardware Revision: ");
      char hardware_revision[format_hex_pretty_size(1)];
      format_hex_pretty_to(hardware_revision, (uint8_t *) bytes + 2, 1);
      result.append(hardware_revision);
      result.append("|Software Index: ");
      char software_index[format_hex_pretty_size(1)];
      format_hex_pretty_to(software_index, (uint8_t *) bytes + 3, 1);
      result.append(software_index);
      publish_state(result);
    } break;
    default:
      char buf[16];
      snprintf(buf, sizeof(buf), "%u", value);
      publish_state(buf);
  }
};

}  // namespace optolink
}  // namespace esphome

#endif
