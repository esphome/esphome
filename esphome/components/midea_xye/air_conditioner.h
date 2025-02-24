#pragma once

#ifdef USE_ARDUINO


#include "esphome/core/defines.h"

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"
#include "ir_transmitter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/climate/climate_traits.h"

//CLIENT command structure
#define PREAMBLE      0XAA
#define PROLOGUE      0X55

#define CLIENT_COMMAND_QUERY  0xC0
#define CLIENT_COMMAND_SET    0xC3
#define CLIENT_COMMAND_LOCK   0xCC
#define CLIENT_COMMAND_UNLOCK 0xCD
#define CLIENT_COMMAND_CELCIUS 0xC4

#define FROM_CLIENT           0x80

#define OP_MODE_OFF          0x00
#define OP_MODE_AUTO         0x80
#define OP_MODE_FAN          0x81
#define OP_MODE_DRY          0x82
#define OP_MODE_HEAT         0x84
#define OP_MODE_COOL         0x88

#define FAN_MODE_AUTO        0x80
#define FAN_MODE_OFF         0x00
#define FAN_MODE_HIGH        0x01
#define FAN_MODE_MEDIUM      0x02
#define FAN_MODE_LOW         0x04

#define TEMP_SET_FAN_MODE    0xFF

#define MODE_FLAG_AUX_HEAT   0x02
#define MODE_FLAG_NORM       0x00
#define MODE_FLAG_ECO        0x01
#define MODE_FLAG_SWING      0x04
#define MODE_FLAG_VENT       0x88

#define TIMER_15MIN          0x01
#define TIMER_30MIN          0x02
#define TIMER_1HOUR          0x04
#define TIMER_2HOUR          0x08
#define TIMER_4HOUR          0x10
#define TIMER_8HOUR          0x20
#define TIMER_16HOUR         0x40
#define TIMER_INVALID        0x80

#define COMMAND_UNKNOWN      0x00  

//SERVER Response

#define SERVER_COMMAND_QUERY  0xC0
#define SERVER_COMMAND_SET    0xC3
#define SERVER_COMMAND_LOCK   0xCC
#define SERVER_COMMAND_UNLOCK 0xCD

#define TO_CLIENT            0x80

#define RESPONSE_UNKNOWN     0x30  

#define CAPABILITIES_EXT_TEMP 0x80  
#define CAPABILITIES_SWING    0x10

#define RESPONSE_UNKNOWN1    0xFF  
#define RESPONSE_UNKNOWN2    0x01  

#define OP_FLAG_WATER_PUMP   0x04  
#define OP_FLAG_WATER_LOCK   0x80  

#define RESPONSE_UNKNOWN3    0x00  

#define TX_LEN 16

#define RX_BYTE_PREAMBLE          0
#define RX_BYTE_COMMAND_TYPE      1
#define RX_BYTE_TO_CLIENT         2
#define RX_BYTE_DESTINATION1      3
#define RX_BYTE_SOURCE            4
#define RX_BYTE_DESTINATION2      5
#define RX_BYTE_UNKNOWN1          6
#define RX_BYTE_CAPABILITIES      7
#define RX_BYTE_OP_MODE           8
#define RX_BYTE_FAN_MODE          9
#define RX_BYTE_SET_TEMP         10
#define RX_BYTE_T1_TEMP          11
#define RX_BYTE_T2A_TEMP         12
#define RX_BYTE_T2B_TEMP         13
#define RX_BYTE_T3_TEMP          14
#define RX_BYTE_CURRENT          15
#define RX_BYTE_UNKNOWN2         16
#define RX_BYTE_TIMER_START      17
#define RX_BYTE_TIMER_STOP       18
#define RX_BYTE_UNKNOWN3         19
#define RX_BYTE_MODE_FLAGS       20
#define RX_BYTE_OP_FLAGS         21
#define RX_BYTE_ERROR_FLAGS1     22
#define RX_BYTE_ERROR_FLAGS2     23
#define RX_BYTE_PROTECT_FLAGS1   24
#define RX_BYTE_PROTECT_FLAGS2   25
#define RX_BYTE_CCM_COM_ERROR_FLAGS 26
#define RX_BYTE_UNKNOWN4         27
#define RX_BYTE_UNKNOWN5         28
#define RX_BYTE_UNKNOWN6         29
#define RX_BYTE_CRC              30
#define RX_BYTE_PROLOGUE         31
#define RX_LEN                   32

//TODO: Don't hardcode this
#define SERVER_ID 0
#define CLIENT_ID 0

namespace esphome {
namespace midea {
namespace ac {

using sensor::Sensor;
using climate::ClimateCall;
using climate::ClimatePreset;
using climate::ClimateTraits;
using climate::ClimateMode;
using climate::ClimateSwingMode;
using climate::ClimateFanMode;

class Constants {
public:
  static const char *const TAG;
};



class AirConditioner : public PollingComponent, public climate::Climate {
public:
  AirConditioner() : PollingComponent(1000) { this->response_timeout = 100;}

#ifdef USE_REMOTE_TRANSMITTER
  void set_transmitter(RemoteTransmitterBase *transmitter) { this->transmitter_.set_transmitter(transmitter); }
#endif

  /* UART communication */

  void set_uart_parent(uart::UARTComponent *parent) { this->uart_ = parent; } 
  void set_period(uint32_t ms) {this->set_update_interval(ms); } 
  void set_response_timeout(uint32_t ms) {this->response_timeout = ms; } 

    /* Component methods */

  float get_setup_priority() const override { return setup_priority::BEFORE_CONNECTION; }


  void dump_config() override;
  void set_outdoor_temperature_sensor(Sensor *sensor) { this->outdoor_sensor_ = sensor; }
  void set_temperature_2a_sensor(Sensor *sensor) { this->temperature_2a_sensor_ = sensor; }
  void set_temperature_2b_sensor(Sensor *sensor) { this->temperature_2b_sensor_ = sensor; }
  void set_current_sensor(Sensor *sensor) { this->current_sensor_ = sensor; }
  void set_timer_start_sensor(Sensor *sensor) { this->timer_start_sensor_ = sensor; }
  void set_timer_stop_sensor(Sensor *sensor) { this->timer_stop_sensor_ = sensor; }
  void set_error_flags_sensor(Sensor *sensor) { this->error_flags_sensor_ = sensor; }
  void set_protect_flags_sensor(Sensor *sensor) { this->protect_flags_sensor_ = sensor; }
  void set_humidity_setpoint_sensor(Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_power_sensor(Sensor *sensor) { this->power_sensor_ = sensor; }
  void update() override;
  void setClientCommand(uint8_t command);
  void setup() override;
  void loop() override {}
  void setPowerState(bool state);

  /* ############### */
  /* ### ACTIONS ### */
  /* ############### */

  void do_follow_me(float temperature, bool beeper = false);
  void do_display_toggle();
  void do_swing_step();
  //TODO: Do we actually need these three?
  void do_power_on() { this->setPowerState(true); }
  void do_power_off() { this->setPowerState(false); }
  void do_power_toggle() { this->setPowerState(this->mode == ClimateMode::CLIMATE_MODE_OFF); }

  void set_supported_modes(const std::set<ClimateMode> &modes) { this->supported_modes_ = modes; }
  void set_supported_swing_modes(const std::set<ClimateSwingMode> &modes) { this->supported_swing_modes_ = modes; }
  void set_supported_presets(const std::set<ClimatePreset> &presets) { this->supported_presets_ = presets; }
  void set_custom_presets(const std::set<std::string> &presets) { this->supported_custom_presets_ = presets; }
  void set_custom_fan_modes(const std::set<std::string> &modes) { this->supported_custom_fan_modes_ = modes; }

  uint8_t TXData[TX_LEN];
  uint8_t RXData[RX_LEN];

private:
  uint8_t UpdateNextCycle;
  uint8_t ForceReadNextCycle;
  uint32_t response_timeout;

protected:
  uart::UARTComponent *uart_;
#ifdef USE_REMOTE_TRANSMITTER
  IrTransmitter transmitter_;
#endif
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  std::set<ClimateMode> supported_modes_{};
  std::set<ClimateSwingMode> supported_swing_modes_{};
  std::set<ClimatePreset> supported_presets_{};
  std::set<std::string> supported_custom_presets_{};
  std::set<std::string> supported_custom_fan_modes_{};
  Sensor *outdoor_sensor_{nullptr};
  Sensor *temperature_2a_sensor_{nullptr};
  Sensor *temperature_2b_sensor_{nullptr};
  Sensor *current_sensor_{nullptr};
  Sensor *timer_start_sensor_{nullptr};
  Sensor *timer_stop_sensor_{nullptr};
  Sensor *error_flags_sensor_{nullptr};
  Sensor *protect_flags_sensor_{nullptr};
  Sensor *humidity_sensor_{nullptr};
  Sensor *power_sensor_{nullptr};
  ClimateMode last_on_mode_;

  static uint8_t CalculateCRC(uint8_t* Data, uint8_t len);
  void ParseResponse();
  uint8_t CalculateSetTime(uint32_t time);
  uint32_t CalculateGetTime(uint8_t time);
  static float CalculateTemp(uint8_t byte);
};

}  // namespace ac
}  // namespace midea
}  // namespace esphome

#endif  // USE_ARDUINO
