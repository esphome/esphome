#pragma once

#include <cstddef>
#include <cstdint>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::opentherm {

template<class T> constexpr T read_bit(T value, uint8_t bit) { return (value >> bit) & 0x01; }

enum OperationMode {
  IDLE = 0,  // no operation

  LISTEN = 1,    // waiting for transmission to start
  READ = 2,      // reading 32-bit data frame
  RECEIVED = 3,  // data frame received with valid start and stop bit

  WRITE = 4,  // writing data to output

  ERROR_PROTOCOL = 8,  // protocol error, can happen only during READ
  ERROR_TIMEOUT = 9,   // timeout while waiting for response from device, only during LISTEN
  ERROR_TIMER = 10     // error operating the transport: the ESP32 timer, or the RMT driver on a failed transmit
};

enum ProtocolErrorType {
  NO_ERROR = 0,            // No error
  NO_TRANSITION = 1,       // No transition in the middle of the bit
  INVALID_STOP_BIT = 2,    // Stop bit wasn't present when expected
  PARITY_ERROR = 3,        // Parity check didn't pass
  NO_CHANGE_TOO_LONG = 4,  // No level change for too much timer ticks
  INVALID_START_BIT = 5,   // Start bit wasn't present when expected
};

enum TimerErrorType {
  NO_TIMER_ERROR = 0,           // No error
  SET_ALARM_VALUE_ERROR = 1,    // gptimer_set_alarm_action failed
  TIMER_START_ERROR = 2,        // gptimer_start failed
  TIMER_PAUSE_ERROR = 3,        // gptimer_stop failed
  SET_COUNTER_VALUE_ERROR = 4,  // gptimer_set_raw_count failed
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

enum BitPositions { STOP_BIT = 33 };

/**
 * Structure to hold Opentherm data packet content.
 * Use get_f88(), get_u16() or get_s16() functions to get appropriate value of data packet according to id of message.
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
  float get_f88();

  /**
   * @param float number to set as value of this data packet
   */
  void set_f88(float value);

  /**
   * @return unsigned 16b integer representation of data packet value
   */
  uint16_t get_u16();

  /**
   * @param unsigned 16b integer number to set as value of this data packet
   */
  void set_u16(uint16_t value);

  /**
   * @return signed 16b integer representation of data packet value
   */
  int16_t get_s16();

  /**
   * @param signed 16b integer number to set as value of this data packet
   */
  void set_s16(int16_t value);
};

struct OpenThermError {
  ProtocolErrorType error_type;
  uint32_t capture;
  uint8_t clock;
  uint32_t data;
  uint8_t bit_pos;
};

/**
 * Everything the two transports share: frame state machine, message packing and the diagnostic helpers.
 * The transport itself is supplied by a subclass, which is either timer sampling or RMT.
 */
class OpenThermBase {
 public:
  OpenThermBase(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t device_timeout = 800);
  virtual ~OpenThermBase() = default;

  /// Configures the pins and brings up the transport.
  bool initialize();

  /// Sends a request. The transport moves the conversation to listening when the last bit has left the line; call
  /// listen() from then on, until an answer or an error arrives.
  void send(OpenthermData &data);

  /// Called by the transport the moment the last bit has left the line: the conversation goes straight from writing
  /// to listening, with nothing of the protocol left to do in between.
  /// @return true when a request had finished, false when the completion was not the end of one
  bool transmitted();

  /// Arms the receiver for the answer. The state machine calls it from the listening state; whether that is a
  /// repeat is the transport's business, since it knows whether it is already armed.
  void listen() { this->listen_impl_(); }

  /// Aborts whatever is in progress and drives the line to its idle level.
  void stop();

  /// Services the transport. Call it from the component loop; a timer-based transport needs nothing.
  void loop() { this->loop_impl_(); }

  /**
   * @return true when a valid data packet has been received and can be retrieved with get_message()
   */
  bool has_message() { return this->mode_ == OperationMode::RECEIVED; }

  bool get_message(OpenthermData &data);
  bool get_protocol_error(OpenThermError &error);

  bool is_error() {
    return this->mode_ == OperationMode::ERROR_TIMEOUT || this->mode_ == OperationMode::ERROR_PROTOCOL ||
           this->mode_ == OperationMode::ERROR_TIMER;
  }
  bool is_timeout() { return this->mode_ == OperationMode::ERROR_TIMEOUT; }
  bool is_protocol_error() { return this->mode_ == OperationMode::ERROR_PROTOCOL; }
  bool is_timer_error() { return this->mode_ == OperationMode::ERROR_TIMER; }
  bool is_idle() { return this->mode_ == OperationMode::IDLE; }
  bool is_active() {
    return this->mode_ == OperationMode::LISTEN || this->mode_ == OperationMode::READ ||
           this->mode_ == OperationMode::WRITE;
  }
  OperationMode get_mode() { return this->mode_; }

  // A bit is 1 ms. Manchester puts the value in the right half and its inverse in the left, so a frame is 34 bits
  // of two halves each.
  static constexpr uint32_t HALF_BIT_US = 500;
  static constexpr size_t FRAME_BITS = 34;
  static constexpr uint32_t FRAME_US = FRAME_BITS * 2 * HALF_BIT_US;
  static constexpr uint32_t REQUEST_MS = FRAME_US / 1000;
  // The longest pause allowed inside a frame. A capture also closes this long after the last edge.
  static constexpr uint32_t FRAME_IDLE_US = 10000;

  /// Milliseconds from the start of a request until the response is overdue. The response time in the protocol
  /// covers the start of the reply alone, so the reply frame and the gap that closes it still have to fit after it.
  uint32_t response_deadline_ms() const {
    return REQUEST_MS + static_cast<uint32_t>(this->device_timeout_) + (FRAME_US + FRAME_IDLE_US) / 1000u;
  }

  /// Marks the wait for a response as timed out. The component owns the clock and calls this when the deadline passes.
  void timeout() { this->mode_ = OperationMode::ERROR_TIMEOUT; }

  void debug_data(OpenthermData &data);
  void debug_error(OpenThermError &error) const;

  /// Reports and clears a transport error. Only the timer transport can raise one.
  virtual void report_and_reset_timer_error() {}

  const char *protocol_error_to_str(ProtocolErrorType error_type);
  const char *timer_error_to_str(TimerErrorType error_type);
  const char *message_type_to_str(MessageType message_type);
  const char *operation_mode_to_str(OperationMode mode);
  const char *message_id_to_str(MessageId id);

 protected:
  virtual bool initialize_impl_() = 0;
  virtual void listen_impl_() = 0;
  virtual void send_impl_() = 0;
  virtual void stop_impl_() = 0;
  virtual void loop_impl_() = 0;

  static bool check_parity_(uint32_t val);

  // Both edges of a pulse shorter than this are noise; the shortest real half bit is 500 us.
  static constexpr uint32_t GLITCH_MAX_US = 50;
  // Consecutive bit transitions are one bit apart, so anything closer is the boundary transition and is skipped.
  static constexpr uint32_t BIT_GAP_MIN_US = 750;

  /// Level of one half of a bit. Bit 0 is the start bit and bit 33 the stop bit; half 0 is the left half, which
  /// carries the inverse of the value.
  static bool half_bit_level_(uint32_t data, uint8_t bit, uint8_t half);

  /// Reads a frame from its edges: drops noise, then takes 34 bits and validates the frame.
  static ProtocolErrorType decode_edges_(uint32_t *edges_us, uint8_t *levels, size_t *count, uint32_t *data,
                                         uint8_t *bit_pos);

  /// What a complete frame has to satisfy: it opens and closes with a 1 and carries even parity.
  static ProtocolErrorType check_frame_(uint32_t data, bool start_bit, bool stop_bit);

  InternalGPIOPin *in_pin_;
  InternalGPIOPin *out_pin_;
  ISRInternalGPIOPin isr_in_pin_;
  ISRInternalGPIOPin isr_out_pin_;

  OperationMode mode_;
  ProtocolErrorType error_type_;
  uint32_t capture_;
  uint8_t clock_;
  uint32_t data_;
  uint8_t bit_pos_;
  int32_t device_timeout_;
};

}  // namespace esphome::opentherm
