#pragma once

#include "esphome/core/hal.h"

#ifdef ESP8266
#ifndef IRAM_ATTR
#define IRAM_ATTR ICACHE_RAM_ATTR
#endif
#endif

// The only thing we want from Arduino :)
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)

namespace esphome {
namespace opentherm {

enum OperationMode {
  IDLE = 0,  // no operation

  LISTEN = 1,    // waiting for transmission to start
  READ = 2,      // reading 32-bit data frame
  RECEIVED = 3,  // data frame received with valid start and stop bit

  WRITE = 4,  // writing data with timer_
  SENT = 5,   // all data written to output

  ERROR_MANCH = 8,   // manchester protocol data transfer error
  ERROR_TIMEOUT = 9  // read timeout
};

enum OpenThermMessageType {
  READ_DATA = 0,
  READ_ACK = 4,
  WRITE_DATA = 1,
  WRITE_ACK = 5,
  INVALID_DATA = 2,
  DATA_INVALID = 6,
  UNKNOWN_DATAID = 7
};

enum OpenThermMessageId {
  STATUS = 0,
  CH_SETPOINT = 1,
  MASTER_CONFIG = 2,
  SLAVE_CONFIG = 3,
  COMMAND_CODE = 4,
  FAULT_FLAGS = 5,
  REMOTE = 6,
  COOLING_CONTROL = 7,
  CH2_SETPOINT = 8,
  CH_SETPOINT_OVERRIDE = 9,
  TSP_COUNT = 10,
  TSP_COMMAND = 11,
  FHB_SIZE = 12,
  FHB_COMMAND = 13,
  MAX_MODULATION_LEVEL = 14,
  MAX_BOILER_CAPACITY = 15,
  ROOM_SETPOINT = 16,
  MODULATION_LEVEL = 17,
  CH_WATER_PRESSURE = 18,
  DHW_FLOW_RATE = 19,
  DAY_TIME = 20,
  DATE = 21,
  YEAR = 22,
  ROOM_SETPOINT_CH2 = 23,
  ROOM_TEMP = 24,
  FEED_TEMP = 25,
  DHW_TEMP = 26,
  OUTSIDE_TEMP = 27,
  RETURN_WATER_TEMP = 28,
  SOLAR_STORE_TEMP = 29,
  SOLAR_COLLECT_TEMP = 30,
  FEED_TEMP_CH2 = 31,
  DHW2_TEMP = 32,
  EXHAUST_TEMP = 33,
  DHW_BOUNDS = 48,
  CH_BOUNDS = 49,
  OTC_CURVE_BOUNDS = 50,
  DHW_SETPOINT = 56,
  MAX_CH_SETPOINT = 57,
  OTC_CURVE_RATIO = 58,

  // HVAC Specific Message IDs
  HVAC_STATUS = 70,
  REL_VENT_SETPOINT = 71,
  SLAVE_VENT = 74,
  REL_VENTILATION = 77,
  REL_HUMID_EXHAUST = 78,
  SUPPLY_INLET_TEMP = 80,
  SUPPLY_OUTLET_TEMP = 81,
  EXHAUST_INLET_TEMP = 82,
  EXHAUST_OUTLET_TEMP = 83,
  NOM_REL_VENTILATION = 87,

  OVERRIDE_FUNC = 100,
  OEM_DIAGNOSTIC = 115,
  BURNER_STARTS = 116,
  CH_PUMP_STARTS = 117,
  DHW_PUMP_STARTS = 118,
  DHW_BURNER_STARTS = 119,
  BURNER_HOURS = 120,
  CH_PUMP_HOURS = 121,
  DHW_PUMP_HOURS = 122,
  DHW_BURNER_HOURS = 123,
  OT_VERSION_MASTER = 124,
  OT_VERSION_SLAVE = 125,
  VERSION_MASTER = 126,
  VERSION_SLAVE = 127
};

enum BitPositions { STOP_BIT = 33 };

/**
 * Structure to hold Opentherm data packet content.
 * Use f88(), u16() or s16() functions to get appropriate value of data packet accoridng to id of message.
 */
struct OpenthermData {
  uint8_t type;
  uint8_t id;
  uint8_t valueHB;
  uint8_t valueLB;

  /**
   * @return float representation of data packet value
   */
  float f88();

  /**
   * @param float number to set as value of this data packet
   */
  void f88(float value);

  /**
   * @return unsigned 16b integer representation of data packet value
   */
  uint16_t u16();

  /**
   * @param unsigned 16b integer number to set as value of this data packet
   */
  void u16(uint16_t value);

  /**
   * @return signed 16b integer representation of data packet value
   */
  int16_t s16();

  /**
   * @param signed 16b integer number to set as value of this data packet
   */
  void s16(int16_t value);
};

/**
 * Opentherm static class that supports either listening or sending Opentherm data packets in the same time
 */
class OpenTherm {
 public:
  OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t slave_timeout = 800);

  /**
   * Start listening for Opentherm data packet comming from line connected to given pin.
   * If data packet is received then has_message() function returns true and data packet can be retrieved by calling
   * get_message() function. If timeout > 0 then this function waits for incomming data package for timeout millis and
   * if no data packet is recevived, error state is indicated by is_error() function. If either data packet is received
   * or timeout is reached listening is stopped.
   */
  void listen();

  /**
   * Use this function to check whether listen() function already captured a valid data packet.
   *
   * @return true if data packet has been captured from line by listen() function.
   */
  bool has_message();

  /**
   * Use this to retrive data packed captured by listen() function. Data packet is ready when has_message() function
   * returns true. This function can be called multiple times until stop() is called.
   *
   * @param data reference to data structure to which fill the data packet data.
   * @return true if packet was ready and was filled into data structure passed, false otherwise.
   */
  bool get_message(OpenthermData &data);

  /**
   * Immediately send out Opentherm data packet to line connected on given pin.
   * Completed data transfer is indicated by is_sent() function.
   * Error state is indicated by is_error() function.
   *
   * @param data Opentherm data packet.
   */
  void send(OpenthermData &data);

  /**
   * Stops listening for data packet or sending out data packet and resets internal state of this class.
   * Stops all timers and unattaches all interrupts.
   */
  void stop();

  /**
   * Use this function to check whether send() function already finished sending data packed to line.
   *
   * @return true if data packet has been sent, false otherwise.
   */
  bool is_sent();

  /**
   * Indicates whether listinig or sending is not in progress.
   * That also means that no timers are running and no interrupts are attached.
   *
   * @return true if listening nor sending is in progress.
   */
  bool is_idle();

  /**
   * Indicates whether last listen() or send() operation ends up with an error.
   *
   * @return true if last listen() or send() operation ends up with an error.
   */
  bool is_error();

  static bool timer_isr(OpenTherm *arg);

 private:
  ISRInternalGPIOPin isr_in_pin_;
  ISRInternalGPIOPin isr_out_pin_;

  volatile OperationMode mode_;
  volatile uint32_t capture_;
  volatile uint8_t clock_;
  volatile uint32_t data_;
  volatile uint8_t bit_pos_;
  volatile bool active_;
  volatile int32_t timeout_counter_;  // <0 no timeout
  volatile bool timer_initialized_;

  int32_t slave_timeout_;

  void listen_();  // listen to incoming data packets
  void read_();    // data detected start reading
  void stop_();    // stop timers and interrupts
  void init_timer_();
  void start_timer_(uint64_t alarm_value);
  void start_read_timer_();   // reading timer_ to sample at 1/5 of manchester code bit length (at 5kHz)
  void start_write_timer_();  // writing timer_ to send manchester code (at 2kHz)
  void stop_timer_();
  bool check_parity_(uint32_t val);

  void bit_read_(uint8_t value);
  bool verify_stop_bit_(uint8_t value);
  void write_bit_(uint8_t high, uint8_t pos);
};

}  // namespace opentherm
}  // namespace esphome
