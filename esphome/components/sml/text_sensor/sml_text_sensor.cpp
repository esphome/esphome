#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sml_text_sensor.h"
#include "../sml_parser.h"

namespace esphome {
namespace sml {

static const char *const TAG = "sml_text_sensor";

SmlTextSensor::SmlTextSensor(std::string server_id, std::string obis, std::string format)
    : SmlListener(std::move(server_id), std::move(obis)), format_(std::move(format)) {}

void SmlTextSensor::publish_val(const ObisInfo &obis_info) {
  char value_type;
  if (this->format_ == "hex")
    value_type = SML_HEX;
  else if (this->format_ == "text")
    value_type = SML_OCTET;
  else if (this->format_ == "bool")
    value_type = SML_BOOL;
  else if (this->format_ == "uint")
    value_type = SML_UINT;
  else if (this->format_ == "int")
    value_type = SML_INT;
  else
    value_type = obis_info.value_type;

  switch (value_type) {
    case SML_HEX: {
      publish_state("0x" + bytes_repr(obis_info.value));
      break;
    }
    case SML_INT: {
      publish_state(to_string(bytes_to_int(obis_info.value)));
      break;
    }
    case SML_BOOL:
      publish_state(bytes_to_uint(obis_info.value) ? "True" : "False");
      break;
    case SML_UINT: {
      publish_state(to_string(bytes_to_uint(obis_info.value)));
      break;
    }
    case SML_OCTET: {
      publish_state(std::string(obis_info.value.begin(), obis_info.value.end()));
      break;
    }
  }
}

void SmlTextSensor::dump_config() { LOG_TEXT_SENSOR("  ", "SML", this); }

}  // namespace sml
}  // namespace esphome
