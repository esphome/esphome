#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace kamstrup_kmp {

/*
    ===========================================================================
    ===                            KAMSTRUP KMP                             ===
    ===========================================================================

    Kamstrup Meter Protocol (KMP) is a protocol used with Kamstrup district
    heating meters, e.g. Kamstrup MULTICAL 403.
    These devices register consumed heat from a district heating system.
    It does this by measuring the incoming and outgoing water temperature
    and by measuring the water flow. The temperature difference (delta T)
    together with the water flow results in consumed energy, typically
    in giga joule (GJ).

    The Kamstrup Multical has an optical interface just above the display.
    This interface is essentially an RS-232 interface using a proprietary
    protocol (Kamstrup Meter Protocol [KMP]).

    The integration uses this optical interface to periodically read the
    configured values (sensors) from the meter. Supported sensors are:
      - Heat Energy               [GJ]
      - Current Power Consumption [kW]
      - Temperature 1             [°C]
      - Temperature 2             [°C]
      - Temperature Difference    [°K]
      - Water Flow                [l/h]
      - Volume                    [m3]

    Apart from these supported 'fixed' sensors, the user can configure up to
    five custom sensors. The KMP command (16 bit unsigned int) has to be
    provided in that case.

    Note:
    The optical interface is enabled as soon as a button on the meter is pushed.
    The interface stays active for a few minutes. To keep the interface 'alive'
    magnets must be placed around the optical sensor.

    Units:
    Units are set using the regular Sensor config in the user yaml. However,
    KMP does also send the correct unit with every value. When DEBUG logging
    is enabled, the received value with the received unit are logged.

    Acknowledgement:
    This interface was inspired by:
      - https://atomstar.tweakblogs.net/blog/19110/reading-out-kamstrup-multical-402-403-with-home-built-optical-head
      - https://wiki.hal9k.dk/projects/kamstrup
*/

// KMP Commands
static const uint16_t CMD_HEAT_ENERGY = 0x003C;
static const uint16_t CMD_POWER = 0x0050;
static const uint16_t CMD_TEMP1 = 0x0056;
static const uint16_t CMD_TEMP2 = 0x0057;
static const uint16_t CMD_TEMP_DIFF = 0x0059;
static const uint16_t CMD_FLOW = 0x004A;
static const uint16_t CMD_VOLUME = 0x0044;

// KMP units
static const char *const UNITS[] = {
    "",      "Wh",   "kWh",  "MWh",   "GWh",     "J",       "kJ",       "MJ",       "GJ",       "Cal",
    "kCal",  "Mcal", "Gcal", "varh",  "kvarh",   "Mvarh",   "Gvarh",    "VAh",      "kVAh",     "MVAh",
    "GVAh",  "kW",   "kW",   "MW",    "GW",      "kvar",    "kvar",     "Mvar",     "Gvar",     "VA",
    "kVA",   "MVA",  "GVA",  "V",     "A",       "kV",      "kA",       "C",        "K",        "l",
    "m3",    "l/h",  "m3/h", "m3xC",  "ton",     "ton/h",   "h",        "hh:mm:ss", "yy:mm:dd", "yyyy:mm:dd",
    "mm:dd", "",     "bar",  "RTC",   "ASCII",   "m3 x 10", "ton x 10", "GJ x 10",  "minutes",  "Bitfield",
    "s",     "ms",   "days", "RTC-Q", "Datetime"};

class KamstrupKMPComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void set_heat_energy_sensor(sensor::Sensor *sensor) { this->heat_energy_sensor_ = sensor; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sensor_ = sensor; }
  void set_temp1_sensor(sensor::Sensor *sensor) { this->temp1_sensor_ = sensor; }
  void set_temp2_sensor(sensor::Sensor *sensor) { this->temp2_sensor_ = sensor; }
  void set_temp_diff_sensor(sensor::Sensor *sensor) { this->temp_diff_sensor_ = sensor; }
  void set_flow_sensor(sensor::Sensor *sensor) { this->flow_sensor_ = sensor; }
  void set_volume_sensor(sensor::Sensor *sensor) { this->volume_sensor_ = sensor; }
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void loop() override;
  void add_custom_sensor(sensor::Sensor *sensor, uint16_t command) {
    this->custom_sensors_.push_back(sensor);
    this->custom_commands_.push_back(command);
  }

 protected:
  // Sensors
  sensor::Sensor *heat_energy_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *temp1_sensor_{nullptr};
  sensor::Sensor *temp2_sensor_{nullptr};
  sensor::Sensor *temp_diff_sensor_{nullptr};
  sensor::Sensor *flow_sensor_{nullptr};
  sensor::Sensor *volume_sensor_{nullptr};

  // Custom sensors and commands
  std::vector<sensor::Sensor *> custom_sensors_;
  std::vector<uint16_t> custom_commands_;

  // Command queue
  std::queue<uint16_t> command_queue_;

  // Methods

  // Sends a command to the meter and receives its response
  void send_command_(uint16_t command);
  // Sends a message to the meter. A prefix/suffix and CRC are added
  void send_message_(const uint8_t *msg, int msg_len);
  // Clears and data that might be in the UART Rx buffer
  void clear_uart_rx_buffer_();
  // Reads and validates the response to a send command
  void read_command_(uint16_t command);
  // Parses a received message
  void parse_command_message_(uint16_t command, const uint8_t *msg, int msg_len);
  // Sets the received value to the correct sensor
  void set_sensor_value_(uint16_t command, float value, uint8_t unit_idx);
};

// "true" CCITT CRC-16
uint16_t crc16_ccitt(const uint8_t *buffer, int len);

}  // namespace kamstrup_kmp
}  // namespace esphome
