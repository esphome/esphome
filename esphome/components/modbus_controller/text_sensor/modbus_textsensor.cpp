
#include "modbus_textsensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.text_sensor";

void ModbusTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Modbus Controller Text Sensor", this); }

void ModbusTextSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  std::string output_str{};
  uint8_t items_left = this->response_bytes;
  uint8_t index = this->offset;
  while ((items_left > 0) && index < data.size()) {
    uint8_t b = data[index];
    switch (this->encode_) {
      case RawEncoding::HEXBYTES:
        output_str += str_snprintf("%02x", 2, b);
        break;
      case RawEncoding::COMMA:
        output_str += str_sprintf(index != this->offset ? ",%d" : "%d", b);
        break;
      case RawEncoding::ANSI:
        if (b < 0x20)
          break;
      // FALLTHROUGH
      // Anything else no encoding
      default:
        output_str += (char) b;
        break;
    }
    items_left--;
    index++;
  }

  // Is there a lambda registered
  // call it with the pre converted value and the raw data array
  if (this->transform_func_.has_value()) {
    // the lambda can parse the response itself
    auto val = (*this->transform_func_)(this, output_str, data);
    if (val.has_value()) {
      ESP_LOGV(TAG, "Value overwritten by lambda");
      output_str = val.value();
    }
  }
  this->publish_state(output_str);
}

}  // namespace modbus_controller
}  // namespace esphome
