#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "sml_text_sensor.h"
#include "../sml_parser.h"
#include <cinttypes>

namespace esphome {
namespace sml {

static const char *const TAG = "sml_text_sensor";

SmlTextSensor::SmlTextSensor(std::string server_id, std::string obis_code, SmlType format)
    : SmlListener(std::move(server_id), std::move(obis_code)), format_(format) {}

void SmlTextSensor::publish_val(const ObisInfo &obis_info) {
  uint8_t value_type;
  if (this->format_ == SML_UNDEFINED) {
    value_type = obis_info.value_type;
  } else {
    value_type = this->format_;
  }

  switch (value_type) {
    case SML_HEX: {
      // Buffer for "0x" + up to 32 bytes as hex + null
      char buf[67];
      // Max 32 bytes of data fit in buffer ((67-3)/2)
      size_t hex_bytes = std::min(obis_info.value.size(), size_t(32));
      format_hex_prefixed_to(buf, obis_info.value.begin(), hex_bytes);
      publish_state(buf, 2 + hex_bytes * 2);
      break;
    }
    case SML_INT: {
      char buf[21];  // Enough for int64_t (-9223372036854775808)
      int len = snprintf(buf, sizeof(buf), "%" PRId64, bytes_to_int(obis_info.value));
      publish_state(buf, static_cast<size_t>(len));
      break;
    }
    case SML_BOOL:
      publish_state(bytes_to_uint(obis_info.value) ? "True" : "False");
      break;
    case SML_UINT: {
      char buf[21];  // Enough for uint64_t (18446744073709551615)
      int len = snprintf(buf, sizeof(buf), "%" PRIu64, bytes_to_uint(obis_info.value));
      publish_state(buf, static_cast<size_t>(len));
      break;
    }
    case SML_OCTET: {
      publish_state(reinterpret_cast<const char *>(obis_info.value.begin()), obis_info.value.size());
      break;
    }
  }
}

void SmlTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "SML", this);
  if (!this->server_id.empty()) {
    ESP_LOGCONFIG(TAG, "  Server ID: %s", this->server_id.c_str());
  }
  ESP_LOGCONFIG(TAG, "  OBIS Code: %s", this->obis_code.c_str());
}

}  // namespace sml
}  // namespace esphome
