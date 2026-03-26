#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
#include "esphome/components/uart/uart.h"

#include "mbus.h"
#include "dlms.h"
#include "obis.h"

#include <array>
#include <vector>

namespace esphome::dlms_meter {

#ifndef DLMS_METER_SENSOR_LIST
#define DLMS_METER_SENSOR_LIST(F, SEP)
#endif

#ifndef DLMS_METER_TEXT_SENSOR_LIST
#define DLMS_METER_TEXT_SENSOR_LIST(F, SEP)
#endif

struct MeterData {
  float voltage_l1 = 0.0f;             // Voltage L1
  float voltage_l2 = 0.0f;             // Voltage L2
  float voltage_l3 = 0.0f;             // Voltage L3
  float current_l1 = 0.0f;             // Current L1
  float current_l2 = 0.0f;             // Current L2
  float current_l3 = 0.0f;             // Current L3
  float active_power_plus = 0.0f;      // Active power taken from grid
  float active_power_minus = 0.0f;     // Active power put into grid
  float active_energy_plus = 0.0f;     // Active energy taken from grid
  float active_energy_minus = 0.0f;    // Active energy put into grid
  float reactive_energy_plus = 0.0f;   // Reactive energy taken from grid
  float reactive_energy_minus = 0.0f;  // Reactive energy put into grid
  char timestamp[27]{};                // Text sensor for the timestamp value

  // Netz NOE
  float power_factor = 0.0f;  // Power Factor
  char meternumber[13]{};     // Text sensor for the meterNumber value
};

// Provider constants
enum Providers : uint32_t { PROVIDER_GENERIC = 0x00, PROVIDER_NETZNOE = 0x01 };

class DlmsMeterComponent : public Component, public uart::UARTDevice {
 public:
  DlmsMeterComponent() = default;

  void dump_config() override;
  void loop() override;

  void set_decryption_key(const std::array<uint8_t, 16> &key) { this->decryption_key_ = key; }
  void set_provider(uint32_t provider) { this->provider_ = provider; }

  void publish_sensors(MeterData &data) {
#define DLMS_METER_PUBLISH_SENSOR(s) \
  if (this->s##_sensor_ != nullptr) \
    s##_sensor_->publish_state(data.s);
    DLMS_METER_SENSOR_LIST(DLMS_METER_PUBLISH_SENSOR, )

#define DLMS_METER_PUBLISH_TEXT_SENSOR(s) \
  if (this->s##_text_sensor_ != nullptr) \
    s##_text_sensor_->publish_state(data.s);
    DLMS_METER_TEXT_SENSOR_LIST(DLMS_METER_PUBLISH_TEXT_SENSOR, )
  }

  DLMS_METER_SENSOR_LIST(SUB_SENSOR, )
  DLMS_METER_TEXT_SENSOR_LIST(SUB_TEXT_SENSOR, )

 protected:
  bool parse_mbus_(std::vector<uint8_t> &mbus_payload);
  bool parse_dlms_(const std::vector<uint8_t> &mbus_payload, uint16_t &message_length, uint8_t &systitle_length,
                   uint16_t &header_offset);
  bool decrypt_(std::vector<uint8_t> &mbus_payload, uint16_t message_length, uint8_t systitle_length,
                uint16_t header_offset);
  void decode_obis_(uint8_t *plaintext, uint16_t message_length);

  std::vector<uint8_t> receive_buffer_;  // Stores the packet currently being received
  std::vector<uint8_t> mbus_payload_;    // Parsed M-Bus payload, reused to avoid heap churn
  uint32_t last_read_ = 0;               // Timestamp when data was last read
  uint32_t read_timeout_ = 1000;         // Time to wait after last byte before considering data complete

  uint32_t provider_ = PROVIDER_GENERIC;  // Provider of the meter / your grid operator
  std::array<uint8_t, 16> decryption_key_;
};

}  // namespace esphome::dlms_meter
