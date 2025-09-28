#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace opentherm {

template<class T> constexpr T read_bit(T value, uint8_t bit) { return (value >> bit) & 0x01; }

template<class T> constexpr T set_bit(T value, uint8_t bit) { return value |= (1UL << bit); }

template<class T> constexpr T clear_bit(T value, uint8_t bit) { return value &= ~(1UL << bit); }

template<class T> constexpr T write_bit(T value, uint8_t bit, uint8_t bit_value) {
  return bit_value ? set_bit(value, bit) : clear_bit(value, bit);
}

// Normal state flow for RMT: IDLE → WRITE → SENT → LISTEN → RECEIVED → IDLE
// Normal state flow for timers: IDLE → WRITE → SENT → LISTEN → READ → RECEIVED → IDLE
enum OperationMode {
  IDLE = 0,  // no operation

  LISTEN = 1,    // waiting for transmission to start
  READ = 2,      // reading 32-bit data frame
  RECEIVED = 3,  // data frame received with valid start and stop bit

  WRITE = 4,  // writing data to output
  SENT = 5,   // all data written to output

  ERROR_PROTOCOL = 8,  // protocol error, can happen only during READ
  ERROR_TIMEOUT = 9,   // timeout while waiting for response from device, only during LISTEN
  ERROR_RMT = 10       // error with RMT machinery
};

enum ProtocolErrorType {
  NO_ERROR = 0,                // No error
  NO_TRANSITION = 1,           // No transition in the middle of the bit
  INVALID_START_STOP_BIT = 2,  // Start or stop bit wasn't present when expected
  PARITY_ERROR = 3,            // Parity check didn't pass
  NO_CHANGE_TOO_LONG = 4,      // No level change for too much timer ticks
  INVALID_DURATION = 5,        // Interval had an invalid duration
  INSUFFICIENT_DATA = 6,       // Not enough data in the OpenTherm frame
};

// Deprecated timer error types (legacy). Kept for compatibility with hub code.
enum TimerErrorType {
  NO_TIMER_ERROR = 0,
  SET_ALARM_VALUE_ERROR = 1,
  TIMER_START_ERROR = 2,
  TIMER_PAUSE_ERROR = 3,
  SET_COUNTER_VALUE_ERROR = 4,
};

enum MessageType {
  READ_DATA = 0,
  READ_ACK = 4,
  WRITE_DATA = 1,
  WRITE_ACK = 5,
  INVALID_DATA = 2,
  DATA_INVALID = 6,
  UNKNOWN_DATAID = 7
};

enum MessageId {
  STATUS = 0,
  CH_SETPOINT = 1,
  CONTROLLER_CONFIG = 2,
  DEVICE_CONFIG = 3,
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
  MAX_BOILER_CAPACITY = 15,  // u8_hb - u8_lb gives min modulation level
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
  FAN_SPEED = 35,
  FLAME_CURRENT = 36,
  ROOM_TEMP_CH2 = 37,
  REL_HUMIDITY = 38,
  DHW_BOUNDS = 48,
  CH_BOUNDS = 49,
  OTC_CURVE_BOUNDS = 50,
  DHW_SETPOINT = 56,
  MAX_CH_SETPOINT = 57,
  OTC_CURVE_RATIO = 58,

  // HVAC Specific Message IDs
  HVAC_STATUS = 70,
  REL_VENT_SETPOINT = 71,
  DEVICE_VENT = 74,
  HVAC_VER_ID = 75,
  REL_VENTILATION = 77,
  REL_HUMID_EXHAUST = 78,
  EXHAUST_CO2 = 79,
  SUPPLY_INLET_TEMP = 80,
  SUPPLY_OUTLET_TEMP = 81,
  EXHAUST_INLET_TEMP = 82,
  EXHAUST_OUTLET_TEMP = 83,
  EXHAUST_FAN_SPEED = 84,
  SUPPLY_FAN_SPEED = 85,
  REMOTE_VENTILATION_PARAM = 86,
  NOM_REL_VENTILATION = 87,
  HVAC_NUM_TSP = 88,
  HVAC_IDX_TSP = 89,
  HVAC_FHB_SIZE = 90,
  HVAC_FHB_IDX = 91,

  RF_SIGNAL = 98,
  DHW_MODE = 99,
  OVERRIDE_FUNC = 100,

  // Solar Specific Message IDs
  SOLAR_MODE_FLAGS = 101,  // hb0-2 Controller storage mode
                           // lb0   Device fault
                           // lb1-3 Device mode status
                           // lb4-5 Device status
  SOLAR_ASF = 102,
  SOLAR_VERSION_ID = 103,
  SOLAR_PRODUCT_ID = 104,
  SOLAR_NUM_TSP = 105,
  SOLAR_IDX_TSP = 106,
  SOLAR_FHB_SIZE = 107,
  SOLAR_FHB_IDX = 108,
  SOLAR_STARTS = 109,
  SOLAR_HOURS = 110,
  SOLAR_ENERGY = 111,
  SOLAR_TOTAL_ENERGY = 112,

  FAILED_BURNER_STARTS = 113,
  BURNER_FLAME_LOW = 114,
  OEM_DIAGNOSTIC = 115,
  BURNER_STARTS = 116,
  CH_PUMP_STARTS = 117,
  DHW_PUMP_STARTS = 118,
  DHW_BURNER_STARTS = 119,
  BURNER_HOURS = 120,
  CH_PUMP_HOURS = 121,
  DHW_PUMP_HOURS = 122,
  DHW_BURNER_HOURS = 123,
  OT_VERSION_CONTROLLER = 124,
  OT_VERSION_DEVICE = 125,
  VERSION_CONTROLLER = 126,
  VERSION_DEVICE = 127
};

/**
 * Structure to hold OpenTherm data packet content.
 * Use f88(), u16() or s16() functions to get appropriate value of data packet according to id of message.
 */
struct OpenthermData {
  uint8_t type;
  uint8_t id;
  uint8_t valueHB;
  uint8_t valueLB;

  OpenthermData() : type(0), id(0), valueHB(0), valueLB(0) {}

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

const char *protocol_error_to_str(ProtocolErrorType error_type);
const char *timer_error_to_str(TimerErrorType error_type);
const char *message_type_to_str(MessageType message_type);
const char *operation_mode_to_str(OperationMode mode);
const char *message_id_to_str(MessageId id);

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
void debug_data(const OpenthermData &data);
#else
inline void debug_data(const OpenthermData &data) {};
#endif

bool check_parity(uint32_t val);

class OpenThermBase {
 public:
  OpenThermBase(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin);

  virtual bool initialize();

  virtual void listen();

  virtual void send(OpenthermData &data);

  virtual void stop();

  virtual void log_protocol_state() const {}

  bool has_message() const { return mode_ == OperationMode::RECEIVED; }

  bool get_message(OpenthermData &data);

  ProtocolErrorType get_protocol_error_type() const { return this->error_type_; }

  OperationMode get_mode() const { return mode_; }

  bool is_sent() const { return mode_ == OperationMode::SENT; }

  bool is_idle() const { return mode_ == OperationMode::IDLE; }

  bool is_error() const {
    return mode_ == OperationMode::ERROR_TIMEOUT || mode_ == OperationMode::ERROR_PROTOCOL || mode_ == ERROR_RMT;
  }

  bool is_timeout() const { return mode_ == OperationMode::ERROR_TIMEOUT; }

  bool is_protocol_error() const { return mode_ == OperationMode::ERROR_PROTOCOL; }

  bool is_rmt_error() const { return mode_ == OperationMode::ERROR_RMT; }

  bool is_active() const { return mode_ == LISTEN || mode_ == READ || mode_ == WRITE; }

 protected:
  InternalGPIOPin *in_pin_{};
  InternalGPIOPin *out_pin_{};

  OperationMode mode_{OperationMode::IDLE};
  ProtocolErrorType error_type_;
  uint32_t data_{};
};

}  // namespace opentherm
}  // namespace esphome
