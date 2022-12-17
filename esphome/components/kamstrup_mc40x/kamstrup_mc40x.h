#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace kamstrup_mc40x {

/*
    ===========================================================================
    ===                        KAMSTRUP MULTICAL 40x                        ===
    ===========================================================================

    The Kamstrup Multical is a measuring device registering consumed heat from
    a district heating system. It does this by measuring the incoming and
    outgoing water temperature and by measuring the water flow. The temperature
    difference (delta T) together with the water flow results in consumed
    energy, typically in giga joule (GJ).

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
      - Water Flow                [l/min]
      - Volume                    [m3]

    Note:
    The optical interface is enabled as soon as a button on the meter is pushed.
    The interface stays active for a few minutes. To keep the interface 'alive'
    magnets must be placed around the optical sensor.

    Acknowledgement:
    This interface was inspired by:
      - https://atomstar.tweakblogs.net/blog/19110/reading-out-kamstrup-multical-402-403-with-home-built-optical-head
      - https://wiki.hal9k.dk/projects/kamstrup
*/

// MC40x Commands
static const uint16_t CMD_HEAT_ENERGY = 0x003C;
static const uint16_t CMD_POWER = 0x0050;
static const uint16_t CMD_TEMP1 = 0x0056;
static const uint16_t CMD_TEMP2 = 0x0057;
static const uint16_t CMD_TEMP_DIFF = 0x0059;
static const uint16_t CMD_FLOW = 0x004A;
static const uint16_t CMD_VOLUME = 0x0044;

// MC40x units
static const char *UNITS[] = {
    "",      "Wh",   "kWh",  "MWh",   "GWh",     "J",       "kJ",       "MJ",       "GJ",       "Cal",
    "kCal",  "Mcal", "Gcal", "varh",  "kvarh",   "Mvarh",   "Gvarh",    "VAh",      "kVAh",     "MVAh",
    "GVAh",  "kW",   "kW",   "MW",    "GW",      "kvar",    "kvar",     "Mvar",     "Gvar",     "VA",
    "kVA",   "MVA",  "GVA",  "V",     "A",       "kV",      "kA",       "C",        "K",        "l",
    "m3",    "l/h",  "m3/h", "m3xC",  "ton",     "ton/h",   "h",        "hh:mm:ss", "yy:mm:dd", "yyyy:mm:dd",
    "mm:dd", "",     "bar",  "RTC",   "ASCII",   "m3 x 10", "ton x 10", "GJ x 10",  "minutes",  "Bitfield",
    "s",     "ms",   "days", "RTC-Q", "Datetime"};

class KamstrupMC40xComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void set_heat_energy_sensor(sensor::Sensor *heat_energy_sensor) { heat_energy_sensor_ = heat_energy_sensor; }
  void set_power_sensor(sensor::Sensor *power_sensor) { power_sensor_ = power_sensor; }
  void set_temp1_sensor(sensor::Sensor *temp1_sensor) { temp1_sensor_ = temp1_sensor; }
  void set_temp2_sensor(sensor::Sensor *temp2_sensor) { temp2_sensor_ = temp2_sensor; }
  void set_temp_diff_sensor(sensor::Sensor *temp_diff_sensor) { temp_diff_sensor_ = temp_diff_sensor; }
  void set_flow_sensor(sensor::Sensor *flow_sensor) { flow_sensor_ = flow_sensor; }
  void set_volume_sensor(sensor::Sensor *volume_sensor) { volume_sensor_ = volume_sensor; }
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  // Sensors
  sensor::Sensor *heat_energy_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *temp1_sensor_{nullptr};
  sensor::Sensor *temp2_sensor_{nullptr};
  sensor::Sensor *temp_diff_sensor_{nullptr};
  sensor::Sensor *flow_sensor_{nullptr};
  sensor::Sensor *volume_sensor_{nullptr};

  // Methods

  // Sends a command to the meter and receives its response
  void send_command(uint16_t command);
  // Sends a message to the meter. A prefix/suffix and CRC are added
  void send_message(const uint8_t *msg, int msg_len);
  // Clears and data that might be in the UART Rx buffer
  void clear_uart_rx_buffer();
  // Reads and validates the response to a send command
  void read_command(uint16_t command);
  // Parses a received message
  void parse_command_message(uint16_t command, const uint8_t *msg, int msg_len);
  // Sets the received value to the correct sensor
  void set_sensor_value(uint16_t command, float value, uint8_t unit_idx);
};

// "true" CCITT CRC-16
uint16_t crc16_ccitt(const uint8_t *buffer, int len);

}  // namespace kamstrup_mc40x
}  // namespace esphome
