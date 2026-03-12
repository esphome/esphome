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
  float voltage_l1 = 0.0f;              // Voltage L1
  float voltage_l2 = 0.0f;              // Voltage L2
  float voltage_l3 = 0.0f;              // Voltage L3
  float current_l1 = 0.0f;              // Current L1
  float current_l2 = 0.0f;              // Current L2
  float current_l3 = 0.0f;              // Current L3
  float active_power_plus = 0.0f;       // Active power taken from grid
  float active_power_minus = 0.0f;      // Active power put into grid
  float active_power_plus_l1 = 0.0f;    // Active power taken from grid on L1
  float active_power_minus_l1 = 0.0f;   // Active power put into grid on L1
  float active_power_plus_l2 = 0.0f;    // Active power taken from grid on L2
  float active_power_minus_l2 = 0.0f;   // Active power put into grid on L2
  float active_power_plus_l3 = 0.0f;    // Active power taken from grid on L3
  float active_power_minus_l3 = 0.0f;   // Active power put into grid on L3
  float reactive_power_plus = 0.0f;     // Reactive power taken from grid
  float reactive_power_minus = 0.0f;    // Reactive power put into grid
  float active_energy_plus = 0.0f;      // Active energy taken from grid
  float active_energy_minus = 0.0f;     // Active energy put into grid
  float active_energy_plus_l1 = 0.0f;   // Active energy taken from grid on L1
  float active_energy_minus_l1 = 0.0f;  // Active energy put into grid on L1
  float active_energy_plus_l2 = 0.0f;   // Active energy taken from grid on L2
  float active_energy_minus_l2 = 0.0f;  // Active energy put into grid on L2
  float active_energy_plus_l3 = 0.0f;   // Active energy taken from grid on L3
  float active_energy_minus_l3 = 0.0f;  // Active energy put into grid on L3
  float reactive_energy_plus = 0.0f;    // Reactive energy taken from grid
  float reactive_energy_minus = 0.0f;   // Reactive energy put into grid
  char timestamp[27]{};                 // Text sensor for the timestamp value

  // Netz NOE
  float power_factor = 0.0f;  // Power Factor
  float power_factor_l1 = 0.0f;
  float power_factor_l2 = 0.0f;
  float power_factor_l3 = 0.0f;
  float power_factor_total = 0.0f;
  char meternumber[13]{};   // Text sensor for the meterNumber value
  char meter_number[17]{};  // Kamstrup meter number
  char obis_list_version[17]{};
};

// Provider constants
enum Providers : uint32_t { PROVIDER_GENERIC = 0x00, PROVIDER_NETZNOE = 0x01, PROVIDER_KAMSTRUP_OMNIPOWER = 0x02 };
enum class TransportType : uint8_t { UNKNOWN = 0x00, MBUS = 0x01, HDLC = 0x02 };

class DlmsMeterComponent : public Component, public uart::UARTDevice {
 public:
  DlmsMeterComponent() = default;

  void dump_config() override;
  void loop() override;

  void set_decryption_key(const std::array<uint8_t, 16> &key) { this->decryption_key_ = key; }
  void set_authentication_key(const std::array<uint8_t, 16> &key) {
    this->authentication_key_ = key;
    this->has_authentication_key_ = true;
  }
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
  bool parse_hdlc_(std::vector<uint8_t> &dlms_payload);
  bool parse_dlms_(const std::vector<uint8_t> &dlms_payload, uint16_t &message_length, uint8_t &systitle_length,
                   uint16_t &header_offset);
  bool decrypt_(std::vector<uint8_t> &dlms_payload, uint16_t message_length, uint8_t systitle_length,
                uint16_t header_offset);
  MeterData parse_kamstrup_omnipower_data_(uint8_t *plaintext, uint16_t message_length);
  void decode_kamstrup_omnipower_obis_(uint8_t *plaintext, uint16_t message_length);
  void decode_obis_(uint8_t *plaintext, uint16_t message_length);
  TransportType detect_transport_() const;
  bool uses_hdlc_transport_() const;
  size_t max_receive_length_() const;
  void publish_parsed_data_(MeterData &data);

  std::vector<uint8_t> receive_buffer_;  // Stores the packet currently being received
  std::vector<uint8_t> dlms_payload_;    // Parsed DLMS payload, reused to avoid heap churn
  uint32_t last_read_ = 0;               // Timestamp when data was last read
  uint32_t read_timeout_ = 1000;         // Time to wait after last byte before considering data complete

  uint32_t provider_ = PROVIDER_GENERIC;  // Provider of the meter / your grid operator
  std::array<uint8_t, 16> decryption_key_;
  std::array<uint8_t, 16> authentication_key_{};
  bool has_authentication_key_{false};
};

}  // namespace esphome::dlms_meter
