#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#include "mbus.h"
#include "dlms.h"
#include "obis.h"
#include "dlms_parser.h"

#include <array>
#include <vector>
#include <string>

namespace esphome::dlms_meter {

// Provider constants
enum Providers : uint32_t { PROVIDER_GENERIC = 0x00, PROVIDER_NETZNOE = 0x01 };
// Transport constants
enum Transports : uint32_t { TRANSPORT_MBUS = 0x00, TRANSPORT_RAW = 0x01 };

class DlmsMeterComponent : public Component, public uart::UARTDevice {
 public:
  DlmsMeterComponent() = default;

  void dump_config() override;
  void loop() override;

  void set_decryption_key(const std::array<uint8_t, 16> &key) {
    this->decryption_key_ = key;
    this->has_decryption_key_ = true;
  }
  void set_provider(uint32_t provider) { this->provider_ = provider; }
  void set_transport(uint32_t transport) { this->transport_ = transport; }

#ifdef USE_SENSOR
  void register_sensor(const std::string &obis_code, sensor::Sensor *sensor) {
    this->sensors_.push_back({obis_code, sensor});
  }
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(const std::string &obis_code, text_sensor::TextSensor *sensor) {
    this->text_sensors_.push_back({obis_code, sensor});
  }
#endif

 protected:
  bool parse_mbus_(std::vector<uint8_t> &mbus_payload);
  bool parse_dlms_(const std::vector<uint8_t> &mbus_payload, uint16_t &message_length, uint8_t &systitle_length,
                   uint16_t &header_offset);
  bool decrypt_(std::vector<uint8_t> &mbus_payload, uint16_t message_length, uint8_t systitle_length,
                uint16_t header_offset);

  std::vector<uint8_t> receive_buffer_;  // Stores the packet currently being received
  std::vector<uint8_t> payload_;         // Parsed payload, reused to avoid heap churn
  uint32_t last_read_ = 0;               // Timestamp when data was last read
  uint32_t read_timeout_ = 1000;         // Time to wait after last byte before considering data complete

  uint32_t provider_ = PROVIDER_GENERIC;  // Provider of the meter / your grid operator
  uint32_t transport_ = TRANSPORT_MBUS;   // Transport protocol used

  bool has_decryption_key_{false};
  std::array<uint8_t, 16> decryption_key_;

#ifdef USE_SENSOR
  struct NumericSensorEntry {
    std::string obis;
    sensor::Sensor *sensor;
  };
  std::vector<NumericSensorEntry> sensors_;
#endif
#ifdef USE_TEXT_SENSOR
  struct TextSensorEntry {
    std::string obis;
    text_sensor::TextSensor *sensor;
  };
  std::vector<TextSensorEntry> text_sensors_;
#endif
};

}  // namespace esphome::dlms_meter
