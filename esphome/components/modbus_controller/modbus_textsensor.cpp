
#include <sstream>
#include <iomanip>
#include "modbus_textsensor.h"

namespace esphome {
namespace modbus_controller {

static const char *const TAG = "modbus_textsensor";

// ModbusTextSensor
void ModbusTextSensor::log() { LOG_TEXT_SENSOR(TAG, get_name().c_str(), this); }

void ModbusTextSensor::add_to_controller(ModbusController *master, ModbusFunctionCode register_type,
                                         uint16_t start_address, uint8_t offset, uint8_t register_count,
                                         uint16_t response_bytes, RawEncoding encode, uint8_t skip_updates) {
  this->register_type = register_type;
  this->start_address = start_address;
  this->offset = offset;
  this->bitmask = 0xFFFFFFFF;
  this->sensor_value_type = SensorValueType::RAW;
  this->response_bytes_ = response_bytes;
  this->register_count = register_count;
  this->encode = encode;
  this->skip_updates = skip_updates;
  this->parent_ = master;
  master->add_sensor_item(this);
}

float ModbusTextSensor::parse_and_publish(const std::vector<uint8_t> &data) {
  float result = static_cast<float>(this->response_bytes_);
  std::ostringstream output;
  uint8_t max_items = this->response_bytes_;
  char buffer[4];
  bool add_comma = false;
  for (auto b : data) {
    switch (this->encode) {
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
  return result;
}
// ModbusTextSensor End

}  // namespace modbus_controller
}  // namespace esphome
