#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#include <cstdint>
#include <unordered_map>
#include <string>
#include <memory>
#include "iec62056sensor.h"
#include "iec62056uart.h"

namespace esphome {
namespace iec62056 {

using SENSOR_MAP = std::unordered_multimap<std::string, IEC62056SensorBase *>;

/// @brief States for component state machine.
enum CommState {
  BEGIN,
  WAIT,
  SEND_REQUEST,
  GET_IDENTIFICATION,
  PREPARE_ACK,
  WAIT_FOR_STX,
  READOUT,
  SET_BAUD_RATE,
  UPDATE_STATES,
  INFINITE_WAIT,
  BATTERY_WAKEUP,
  MODE_D_WAIT,
  MODE_D_READOUT,
};

/// @brief Protocol types
enum ProtocolMode { PROTOCOL_MODE_A = 'A', PROTOCOL_MODE_B = 'B', PROTOCOL_MODE_C = 'C', PROTOCOL_MODE_D = 'D' };

/// @brief Implements support for IEC 62056-21 meters.
class IEC62056Component : public Component, public uart::UARTDevice {
 public:
  IEC62056Component();

  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;
  void set_update_interval(uint32_t val) { update_interval_ms_ = val; }
  uint32_t get_update_interval() { return update_interval_ms_; }
  void set_config_baud_rate_max(uint32_t val) { config_baud_rate_max_bps_ = val; }
  void set_connection_timeout_ms(uint32_t val) { connection_timeout_ms_ = val; }
  void set_max_retry_counter(int val) { max_retries_ = val; }
  void set_retry_delay(int val) { retry_delay_ = val; }
  void register_sensor(IEC62056SensorBase *sensor);
#ifdef USE_BINARY_SENSOR
  void register_sensor(binary_sensor::BinarySensor *sensor) { readout_status_sensor_ = sensor; }
#endif
  /// Set flag indicating battery operated meter.
  /// @param flag @c true for battery operated meter, otherwise @c false.
  void set_battery_meter(bool flag) { battery_meter_ = flag; }
  /// @brief Called when switch state changed. Begins readout.
  void trigger_readout();
  void set_mode_d(bool flag) { force_mode_d_ = flag; }

 protected:
  bool parse_line_(const char *line, std::string &out_obis, std::string &out_value1, std::string &out_value2);
  /// Reset values for all sensors.
  void reset_all_sensors_();
  /// Sets sensor value. Detects sensor type. It does not publish the value.
  /// \retval true the value was changed
  /// \retval false the value was not changed. The value is not a number.
  bool set_sensor_value_(SENSOR_MAP::iterator &i, const char *value1, const char *value2);
  void verify_all_sensors_got_value_();
  void connection_status_(bool connected);
  /// Returns a pointer to null terminated string (without starting '/')
  /// or nullptr if no packet in the buffer
  /// @remarks
  /// The function can ignore garbage at the beginning of the packet before '/' character
  char *get_id_(size_t frame_size);
  void update_last_transmission_from_meter_timestamp_() { last_transmission_from_meter_timestamp_ = millis(); }
  void parse_id_(const char *packet);
  /// @brief Sets protocol mode based on baud rate char
  /// @param z baud rate char from identification package
  void set_protocol_(char z);
  /// Dynamically sets UART baud rate
  void update_baudrate_(uint32_t baudrate);
  /// Sends data stored in @a out_buf_
  void send_frame_();
  /// Reads data from serial port until the end of line \r\n or STX/ETX
  ///
  /// @return 0 if no frame received or length of the frame when received
  size_t receive_frame_();
  /// Returns baud rate identification.
  /// It uses @ref mode_ to determine protocol.
  /// @retval '\0' if no match
  /// @retval 'A'-'F' or '1'-'6'
  char baud_rate_to_identification_(uint32_t baud_rate);
  /// @retval 0 unknown identification or protocol A
  uint32_t identification_to_baud_rate_(char z);
  void retry_counter_reset_() { retry_counter_ = 0; }
  void retry_counter_inc_() { retry_counter_++; }
  /// @brief Check if retry or wait for the next scheduled readout.
  void retry_or_sleep_();
  void update_lrc_(const uint8_t *data, size_t size);
  void reset_lrc_() { lrc_ = 0; }
  void wait_(uint32_t ms, CommState state);
  void clear_uart_input_buffer_();
  void send_battery_wakeup_sequence_();
  /// Checks wait timeout
  /// @retval true time has passed
  /// @retval false time has not passed
  bool check_wait_period_() { return millis() - wait_start_timestamp_ >= wait_period_ms_; }
  /// @brief Diagnostic function that reports the state.
  void report_state_();
  void set_next_state_(CommState new_state) { state_ = new_state; }
  /// @brief Not very strict format checker.
  /// To detect transmission errors (LRC is not the best checksum).
  bool validate_obis_(const std::string &obis);
  /// @brief Converts state enum to string.
  const char *state2txt_(CommState state);
  /// @brief Waits computed time to get precise updates as configured.
  /// @remark
  /// Function calls @ref wait() with computed timeout.
  void wait_next_readout_();
  void update_connection_start_timestamp_();
  /// @brief Check if periodic readout is enabled
  /// @retval true yes, read data from meter time to time
  /// @retval false only switch can trigger readout
  bool is_periodic_readout_enabled_() { return UINT32_MAX != update_interval_ms_; }
  /// @brief Check if state machine in in one of wait states
  /// @retval true in wait state
  /// @retval false not in wait state
  bool is_wait_state_() { return state_ == WAIT || state_ == INFINITE_WAIT || state_ == MODE_D_WAIT; }

  static const char PROTO_B_RANGE_BEGIN = 'A';
  static const char PROTO_B_RANGE_END = 'F';
  static const char PROTO_C_RANGE_BEGIN = '0';
  static const char PROTO_C_RANGE_END = '6';
  static const size_t MAX_IN_BUF_SIZE = 128;
  static const size_t MAX_OUT_BUF_SIZE = 84;

  /// @brief A list of sensors.
  SENSOR_MAP sensors_;
#ifdef USE_BINARY_SENSOR
  /// @brief Transmission indicator.
  binary_sensor::BinarySensor *readout_status_sensor_{nullptr};
#endif

  /// @brief Configured update interval
  uint32_t update_interval_ms_;
  /// @brief Maximum baud rate from the config or 0 if not set
  uint32_t config_baud_rate_max_bps_;
  /// @brief Configured connection timeout.
  uint32_t connection_timeout_ms_;
  /// @brief Counts number of retries.
  int retry_counter_{0};
  /// @brief Maximum number of retires. Set from configuration file.
  int max_retries_;
  /// @brief Delay between retries.
  uint32_t retry_delay_{10000};
  /// @brief Flag indicating battery meter.
  bool battery_meter_;
  /// @brief The current state of the state machine.
  CommState state_;
  /// @brief Timestamp, the last transmission from the meter.
  uint32_t last_transmission_from_meter_timestamp_;
  /// @brief I/O input buffer
  /// @sa data_in_size_
  uint8_t in_buf_[MAX_IN_BUF_SIZE];
  /// The size of data in I/O input buffer
  size_t data_in_size_;
  /// Meter identification.
  /// @remark For future use to support not fully compliant meters
  std::string meter_identification_;
  /// Protocol mode: A, B, C
  ProtocolMode mode_;
  /// Baud rate as read from identification packet or 0 (not provided)
  char baud_rate_identification_;
  /// @brief I/O output buffer
  uint8_t out_buf_[MAX_OUT_BUF_SIZE];
  /// The size of data in I/O output buffer
  size_t data_out_size_;
  /// @brief Computed LRC/BCC.
  uint8_t lrc_;
  /// @brief BCC received from the meter.
  uint8_t readout_lrc_;
  /// @brief When WAIT state began.
  uint32_t wait_start_timestamp_;
  /// @brief Time period in WAIT state.
  uint32_t wait_period_ms_;
  /// @brief What is the next state after WAIT
  CommState wait_next_state_;
  /// @brief Helper for diagnostic function that reports current state.
  CommState reported_state_{INFINITE_WAIT};
  /// @brief Updated when every connection starts including retries.
  uint32_t retry_connection_start_timestamp_;
  /// @brief Start of the scheduled readout; not updated for retries.
  uint32_t scheduled_connection_start_timestamp_;
  /// @brief Flag indicating whether @ref scheduled_connection_start_timestamp_ is set or not.
  /// @retval false @ref scheduled_connection_start_timestamp_ is not valid.
  /// @retval true  @ref scheduled_connection_start_timestamp_ was set.
  bool scheduled_timestamp_set_{false};
  /// @brief Check if string is valid float value
  bool validate_float_(const char *value);
  /// @brief Iterator used for publishing sensor values.
  SENSOR_MAP::iterator sensors_iterator_;
  /// @brief Custom extended serial port object.
  std::unique_ptr<IEC62056UART> iuart_;
  /// @brief Indicates unidirectional communication, mode D
  bool force_mode_d_;
};

}  // namespace iec62056
}  // namespace esphome
