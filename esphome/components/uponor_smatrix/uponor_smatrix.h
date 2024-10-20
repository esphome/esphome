#pragma once

#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/core/defines.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

#include <queue>
#include <set>
#include <vector>

namespace esphome {
namespace uponor_smatrix {

/// Date/Time Part 1 (year, month, day of week)
static const uint8_t UPONOR_ID_DATETIME1 = 0x08;
/// Date/Time Part 2 (day of month, hour, minute)
static const uint8_t UPONOR_ID_DATETIME2 = 0x09;
/// Date/Time Part 3 (seconds)
static const uint8_t UPONOR_ID_DATETIME3 = 0x0A;
/// Unknown (observed values: 0x0342, 0x0024)
static const uint8_t UPONOR_ID_UNKNOWN1 = 0x0C;
/// Outdoor Temperature? (sent by controller)
static const uint8_t UPONOR_ID_OUTDOOR_TEMP = 0x2D;
/// Unknown (observed values: 0x8000)
static const uint8_t UPONOR_ID_UNKNOWN2 = 0x35;
/// Room Temperature Setpoint Minimum
static const uint8_t UPONOR_ID_TARGET_TEMP_MIN = 0x37;
/// Room Temperature Setpoint Maximum
static const uint8_t UPONOR_ID_TARGET_TEMP_MAX = 0x38;
/// Room Temperature Setpoint
static const uint8_t UPONOR_ID_TARGET_TEMP = 0x3B;
/// Room Temperature Setpoint Setback for ECO Mode
static const uint8_t UPONOR_ID_ECO_SETBACK = 0x3C;
/// Heating/Cooling Demand
static const uint8_t UPONOR_ID_DEMAND = 0x3D;
/// Thermostat Operating Mode 1 (ECO state, program schedule state)
static const uint8_t UPONOR_ID_MODE1 = 0x3E;
/// Thermostat Operating Mode 2 (sensor configuration, heating/cooling allowed)
static const uint8_t UPONOR_ID_MODE2 = 0x3F;
/// Current Room Temperature
static const uint8_t UPONOR_ID_ROOM_TEMP = 0x40;
/// Current External (Floor/Outdoor) Sensor Temperature
static const uint8_t UPONOR_ID_EXTERNAL_TEMP = 0x41;
/// Current Room Humidity
static const uint8_t UPONOR_ID_HUMIDITY = 0x42;
/// Data Request (sent by controller)
static const uint8_t UPONOR_ID_REQUEST = 0xFF;

/// Indicating an invalid/missing value
static const uint16_t UPONOR_INVALID_VALUE = 0x7FFF;

struct UponorSmatrixData {
  uint8_t id;
  uint16_t value;
};

class UponorSmatrixDevice;

class UponorSmatrixComponent : public uart::UARTDevice, public Component {
 public:
  UponorSmatrixComponent() = default;

  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_system_address(uint16_t address) { this->address_ = address; }
  void register_device(UponorSmatrixDevice *device) { this->devices_.push_back(device); }

  bool send(uint16_t device_address, const UponorSmatrixData *data, size_t data_len);

#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
  void set_time_device_address(uint16_t address) { this->time_device_address_ = address; }
  void send_time() { this->send_time_requested_ = true; }
#endif

 protected:
  bool parse_byte_(uint8_t byte);

  uint16_t address_;
  std::vector<UponorSmatrixDevice *> devices_;
  std::set<uint16_t> unknown_devices_;

  std::vector<uint8_t> rx_buffer_;
  std::queue<std::vector<uint8_t>> tx_queue_;
  uint32_t last_rx_;
  uint32_t last_tx_;

#ifdef USE_TIME
  time::RealTimeClock *time_id_{nullptr};
  uint16_t time_device_address_;
  bool send_time_requested_;
  bool do_send_time_();
#endif
};

class UponorSmatrixDevice : public Parented<UponorSmatrixComponent> {
 public:
  void set_device_address(uint16_t address) { this->address_ = address; }

  virtual void on_device_data(const UponorSmatrixData *data, size_t data_len) = 0;
  bool send(const UponorSmatrixData *data, size_t data_len) {
    return this->parent_->send(this->address_, data, data_len);
  }

 protected:
  friend UponorSmatrixComponent;
  uint16_t address_;
};

inline float raw_to_celsius(uint16_t raw) {
  return (raw == UPONOR_INVALID_VALUE) ? NAN : fahrenheit_to_celsius(raw / 10.0f);
}

inline uint16_t celsius_to_raw(float celsius) {
  return std::isnan(celsius) ? UPONOR_INVALID_VALUE
                             : static_cast<uint16_t>(lroundf(celsius_to_fahrenheit(celsius) * 10.0f));
}

}  // namespace uponor_smatrix
}  // namespace esphome
