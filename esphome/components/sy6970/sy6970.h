#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"

namespace esphome::sy6970 {

// SY6970 Register addresses with descriptive names
static const uint8_t SY6970_REG_INPUT_CURRENT_LIMIT = 0x00;     // Input current limit control
static const uint8_t SY6970_REG_VINDPM = 0x01;                  // Input voltage limit
static const uint8_t SY6970_REG_ADC_CONTROL = 0x02;             // ADC control and function disable
static const uint8_t SY6970_REG_SYS_CONTROL = 0x03;             // Charge enable and system config
static const uint8_t SY6970_REG_CHARGE_CURRENT = 0x04;          // Fast charge current limit
static const uint8_t SY6970_REG_PRECHARGE_CURRENT = 0x05;       // Pre-charge/termination current
static const uint8_t SY6970_REG_CHARGE_VOLTAGE = 0x06;          // Charge voltage limit
static const uint8_t SY6970_REG_TIMER_CONTROL = 0x07;           // Charge timer and status LED control
static const uint8_t SY6970_REG_IR_COMP = 0x08;                 // IR compensation
static const uint8_t SY6970_REG_FORCE_DPDM = 0x09;              // Force DPDM detection
static const uint8_t SY6970_REG_BOOST_CONTROL = 0x0A;           // Boost mode voltage/current
static const uint8_t SY6970_REG_STATUS = 0x0B;                  // System status (bus, charge status)
static const uint8_t SY6970_REG_FAULT = 0x0C;                   // Fault status (NTC)
static const uint8_t SY6970_REG_VINDPM_STATUS = 0x0D;           // Input voltage limit status (also sys voltage)
static const uint8_t SY6970_REG_BATV = 0x0E;                    // Battery voltage
static const uint8_t SY6970_REG_VBUS_VOLTAGE = 0x11;            // VBUS voltage
static const uint8_t SY6970_REG_CHARGE_CURRENT_MONITOR = 0x12;  // Charge current
static const uint8_t SY6970_REG_INPUT_VOLTAGE_LIMIT = 0x13;     // Input voltage limit
static const uint8_t SY6970_REG_DEVICE_ID = 0x14;               // Part information

// Bus Status values (REG_0B[7:5])
enum BusStatus {
  BUS_STATUS_NO_INPUT = 0,
  BUS_STATUS_USB_SDP = 1,
  BUS_STATUS_USB_CDP = 2,
  BUS_STATUS_USB_DCP = 3,
  BUS_STATUS_HVDCP = 4,
  BUS_STATUS_ADAPTER = 5,
  BUS_STATUS_NO_STD_ADAPTER = 6,
  BUS_STATUS_OTG = 7,
};

// Charge Status values (REG_0B[4:3])
enum ChargeStatus {
  CHARGE_STATUS_NOT_CHARGING = 0,
  CHARGE_STATUS_PRE_CHARGE = 1,
  CHARGE_STATUS_FAST_CHARGE = 2,
  CHARGE_STATUS_CHARGE_DONE = 3,
};

// Structure to hold all register data read in one transaction
struct SY6970Data {
  uint8_t registers[21];  // Registers 0x00-0x14 (includes unused 0x0F, 0x10, 0x13)
};

class SY6970Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Sensor setters
  void set_vbus_voltage_sensor(sensor::Sensor *sensor) { this->vbus_voltage_sensor_ = sensor; }
  void set_battery_voltage_sensor(sensor::Sensor *sensor) { this->battery_voltage_sensor_ = sensor; }
  void set_system_voltage_sensor(sensor::Sensor *sensor) { this->system_voltage_sensor_ = sensor; }
  void set_charge_current_sensor(sensor::Sensor *sensor) { this->charge_current_sensor_ = sensor; }
  void set_precharge_current_sensor(sensor::Sensor *sensor) { this->precharge_current_sensor_ = sensor; }

  // Binary sensor setters
  void set_vbus_connected_binary_sensor(binary_sensor::BinarySensor *sensor) {
    this->vbus_connected_binary_sensor_ = sensor;
  }
  void set_charging_binary_sensor(binary_sensor::BinarySensor *sensor) { this->charging_binary_sensor_ = sensor; }
  void set_charge_done_binary_sensor(binary_sensor::BinarySensor *sensor) { this->charge_done_binary_sensor_ = sensor; }

  // Text sensor setters
  void set_bus_status_text_sensor(text_sensor::TextSensor *sensor) { this->bus_status_text_sensor_ = sensor; }
  void set_charge_status_text_sensor(text_sensor::TextSensor *sensor) { this->charge_status_text_sensor_ = sensor; }
  void set_ntc_status_text_sensor(text_sensor::TextSensor *sensor) { this->ntc_status_text_sensor_ = sensor; }

  // Configuration methods
  void set_input_current_limit(uint16_t milliamps);
  void set_charge_target_voltage(uint16_t millivolts);
  void set_precharge_current(uint16_t milliamps);
  void set_charge_current(uint16_t milliamps);
  void set_charge_enabled(bool enabled);
  void set_led_enabled(bool enabled);
  void enable_adc_measure();

 protected:
  bool read_all_registers_();
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);

  void publish_sensors_(const SY6970Data &data);
  void publish_binary_sensors_(const SY6970Data &data);
  void publish_text_sensors_(const SY6970Data &data);

  // Helper methods to extract data from register block
  uint16_t get_vbus_voltage_(const SY6970Data &data);
  uint16_t get_battery_voltage_(const SY6970Data &data);
  uint16_t get_system_voltage_(const SY6970Data &data);
  uint16_t get_charge_current_(const SY6970Data &data);
  uint16_t get_precharge_current_(const SY6970Data &data);
  bool is_vbus_connected_(const SY6970Data &data);
  bool is_charging_(const SY6970Data &data);
  bool is_charge_done_(const SY6970Data &data);
  uint8_t get_bus_status_(const SY6970Data &data);
  uint8_t get_charge_status_(const SY6970Data &data);
  uint8_t get_ntc_status_(const SY6970Data &data);
  const char *get_bus_status_string_(uint8_t status);
  const char *get_charge_status_string_(uint8_t status);
  const char *get_ntc_status_string_(uint8_t status);

  bool initialized_{false};
  SY6970Data data_{};

  // Sensor pointers
  sensor::Sensor *vbus_voltage_sensor_{nullptr};
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *system_voltage_sensor_{nullptr};
  sensor::Sensor *charge_current_sensor_{nullptr};
  sensor::Sensor *precharge_current_sensor_{nullptr};

  // Binary sensor pointers
  binary_sensor::BinarySensor *vbus_connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *charging_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *charge_done_binary_sensor_{nullptr};

  // Text sensor pointers
  text_sensor::TextSensor *bus_status_text_sensor_{nullptr};
  text_sensor::TextSensor *charge_status_text_sensor_{nullptr};
  text_sensor::TextSensor *ntc_status_text_sensor_{nullptr};
};

}  // namespace esphome::sy6970
