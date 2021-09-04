
#include "modbus_textsensor.h"
#include "esphome/core/log.h"
#include <iomanip>
#include <sstream>

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_controller.text_sensor";

void ModbusTextSensor::dump_config() { LOG_TEXT_SENSOR("", "Modbus Controller Text Sensor", this); }

void ModbusTextSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  std::ostringstream output;
  uint8_t max_items = this->response_bytes_;
  char buffer[4];
  bool add_comma = false;
  for (auto b : data) {
    switch (this->encode_) {
      case RawEncoding::HEXBYTES:
        sprintf(buffer, "%02x", b);
        output << buffer;
        break;
      case RawEncoding::COMMA:
        sprintf(buffer, add_comma ? ",%d" : "%d", b);
        output << buffer;
        add_comma = true;
        break;
      // Anything else no encoding
      case RawEncoding::NONE:
      default:
        output << (char) b;
        break;
    }
    if (--max_items == 0) {
      break;
    }
  }
  this->publish_state(output.str());
}

}  // namespace modbus_controller
}  // namespace esphome
