/*
 * OpenTherm protocol implementation. Originally taken from https://github.com/jpraus/arduino-opentherm, but
 * heavily modified to comply with ESPHome coding standards and provide better logging.
 * Original code is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International
 * Public License, which is compatible with GPLv3 license, which covers C++ part of ESPHome project.
 */

#pragma once

#include <string>
#include "opentherm_base.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <esp_err.h>
#include <driver/gpio.h>

namespace esphome {
namespace opentherm {

/**
 * OpenTherm class that supports either listening or sending OpenTherm data packets at the same time
 */
class OpenTherm {
 public:
  OpenTherm(InternalGPIOPin *in_pin, InternalGPIOPin *out_pin, int32_t device_timeout = 800);

  /**
   * Setup pins.
   */
  bool initialize();

  /**
   * Start listening for OpenTherm data packet coming from line connected to given pin.
   * If data packet is received then has_message() function returns true and data packet can be retrieved by calling
   * get_message() function. If timeout > 0 then this function waits for incoming data package for timeout millis and
   * if no data packet is received, error state is indicated by is_error() function. If either data packet is received
   * or timeout is reached listening is stopped.
   */
  void listen();

  /**
   * Use this function to check whether listen() function already captured a valid data packet.
   *
   * @return true if data packet has been captured from line by listen() function.
   */
  bool has_message() { return mode_ == OperationMode::RECEIVED; }

  /**
   * Use this to retrieve data packed captured by listen() function. Data packet is ready when has_message() function
   * returns true. This function can be called multiple times until stop() is called.
   *
   * @param data reference to data structure to which fill the data packet data.
   * @return true if packet was ready and was filled into data structure passed, false otherwise.
   */
  bool get_message(OpenthermData &data);

  /**
   * Immediately send out OpenTherm data packet to line connected on given pin.
   * Completed data transfer is indicated by is_sent() function.
   * Error state is indicated by is_error() function.
   *
   * @param data OpenTherm data packet.
   */
  void send(OpenthermData &data);

  /**
   * Stops listening for data packet or sending out data packet and resets internal state of this class.
   * Stops all timers and unattaches all interrupts.
   */
  void stop();

  /**
   * Get protocol error details in case a protocol error occurred.
   * @param error reference to data structure to which fill the error details
   * @return true if protocol error occurred during last conversation, false otherwise.
   */
  const OpenThermProtocolError &get_protocol_error() const;

  /**
   * Use this function to check whether send() function already finished sending data packed to line.
   *
   * @return true if data packet has been sent, false otherwise.
   */
  bool is_sent() { return mode_ == OperationMode::SENT; }

  /**
   * Indicates whether listing or sending is not in progress.
   * That also means that no timers are running and no interrupts are attached.
   *
   * @return true if listening nor sending is in progress.
   */
  bool is_idle() { return mode_ == OperationMode::IDLE; }

  /**
   * Indicates whether last listen() or send() operation ends up with an error. Includes both timeout and
   * protocol errors.
   *
   * @return true if last listen() or send() operation ends up with an error.
   */
  bool is_error() {
    return mode_ == OperationMode::ERROR_TIMEOUT || mode_ == OperationMode::ERROR_PROTOCOL || mode_ == ERROR_RMT;
  }

  /**
   * Indicates whether last listen() or send() operation ends up with a *timeout* error
   * @return true if last listen() or send() operation ends up with a *timeout* error.
   */
  bool is_timeout() { return mode_ == OperationMode::ERROR_TIMEOUT; }

  /**
   * Indicates whether last listen() or send() operation ends up with a *protocol* error
   * @return true if last listen() or send() operation ends up with a *protocol* error.
   */
  bool is_protocol_error() { return mode_ == OperationMode::ERROR_PROTOCOL; }

  /**
   * Indicates whether start_esp32_timer_() or stop_timer_() had an error. Only relevant when used on ESP32.
   * @return true if there was an error.
   */
  bool is_rmt_error() { return mode_ == OperationMode::ERROR_RMT; }

  bool is_active() { return mode_ == LISTEN || mode_ == READ || mode_ == WRITE; }

  OperationMode get_mode() { return mode_; }

  void debug_rmt() const;

 private:
  InternalGPIOPin *in_pin_{};
  InternalGPIOPin *out_pin_{};
  ISRInternalGPIOPin isr_in_pin_{};
  ISRInternalGPIOPin isr_out_pin_{};

  // RMT resources
  rmt_channel_handle_t rx_channel_{};
  rmt_channel_handle_t tx_channel_{};
  rmt_receive_config_t rx_config_{};
  rmt_encoder_handle_t tx_encoder_{};
  // RX buffer for one OpenTherm frame
  // One OpenTherm frame contains 34 Manchester symbols (start + 32 data + stop) and we
  // allow a little slack for diagnostic captures.
  static constexpr size_t RMT_SYMBOL_CAPACITY = 40;
  rmt_symbol_word_t rmt_buffer_[RMT_SYMBOL_CAPACITY]{};
  size_t rmt_buffer_symbol_count_{};
  // RMT clock resolution in Hz (1 MHz => 1 tick == 1 us)
  static constexpr uint32_t RMT_RESOLUTION_HZ = 1000000u;

  OperationMode mode_{OperationMode::IDLE};
  OpenThermProtocolError error_{};
  uint32_t data_{};

  bool rmt_init_();
  void rmt_read_();
  void rmt_write_();
  static bool rmt_read_callback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *evt, void *arg);

  void set_protocol_error(ProtocolErrorType error_type, size_t bit_index);

  bool decode_rmt_symbols_(size_t num_symbols);

  bool check_parity_(uint32_t val);
};

}  // namespace opentherm
}  // namespace esphome
