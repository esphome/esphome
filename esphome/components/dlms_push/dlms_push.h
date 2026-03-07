#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#include <vector>
#include <string>
#include <array>
#include <memory>

namespace esphome {
namespace dlms_push {

class DlmsParser;

class DlmsPushComponent : public Component, public uart::UARTDevice {
 public:
  DlmsPushComponent();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_receive_timeout(uint32_t timeout_ms) { this->receive_timeout_ms_ = timeout_ms; }
  void set_show_log(bool show_log) { this->show_log_ = show_log; }
  void set_custom_pattern(const std::string &pattern) { this->custom_pattern_ = pattern; }

#ifdef USE_SENSOR
  void register_sensor(const std::string &obis_code, sensor::Sensor *sensor);
#endif
#ifdef USE_TEXT_SENSOR
  void register_text_sensor(const std::string &obis_code, text_sensor::TextSensor *sensor);
#endif
#ifdef USE_BINARY_SENSOR
  void register_binary_sensor(const std::string &obis_code, binary_sensor::BinarySensor *sensor);
#endif

 protected:
  void read_rx_buffer_();
  void process_frame_();

  void on_data_parsed_(const char *obis_code, float float_val, const char *str_val, bool is_numeric);

  uint32_t receive_timeout_ms_{50};
  bool show_log_{false};
  std::string custom_pattern_{""};

  static const size_t MAX_RX_BUFFER_SIZE = 2048;
  std::unique_ptr<uint8_t[]> rx_buffer_;
  size_t rx_buffer_len_{0};
  uint32_t last_rx_char_time_{0};
  bool receiving_{false};

  std::unique_ptr<DlmsParser> parser_;

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
#ifdef USE_BINARY_SENSOR
  struct BinarySensorEntry {
    std::string obis;
    binary_sensor::BinarySensor *sensor;
  };
  std::vector<BinarySensorEntry> binary_sensors_;
#endif
};

}  // namespace dlms_push
}  // namespace esphome
