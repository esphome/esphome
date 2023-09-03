/// @file wk2132.h
/// @author DrCoolZic
/// @brief  wk2132 classes interface

#pragma once
#include <bitset>
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/uart/uart.h"
#include "gen_uart.h"

namespace esphome {
namespace wk2132 {

// here we indicate if we want to include the code for auto tests (recommended)
#define AUTOTEST_COMPONENT

/// @brief Global register
constexpr uint8_t REG_WK2132_GENA = 0x00;  ///< Global control register
constexpr uint8_t REG_WK2132_GRST = 0x01;  ///< Global UART channel reset register
constexpr uint8_t REG_WK2132_GMUT = 0x02;  ///< Global main UART control register

/// @brief UART Channel register when PAGE = 0
constexpr uint8_t REG_WK2132_SPAGE = 0x03;  ///< UART page control register
constexpr uint8_t REG_WK2132_SCR = 0x04;    ///< serial control register
constexpr uint8_t REG_WK2132_LCR = 0x05;    ///< line control register
constexpr uint8_t REG_WK2132_FCR = 0x06;    ///< FIFO control register
constexpr uint8_t REG_WK2132_SIER = 0x07;   ///< interrupt enable register
constexpr uint8_t REG_WK2132_SIFR = 0x08;   ///< interrupt flag register
constexpr uint8_t REG_WK2132_TFCNT = 0x09;  ///< transmit FIFO value register
constexpr uint8_t REG_WK2132_RFCNT = 0x0A;  ///< receive FIFO value register
constexpr uint8_t REG_WK2132_FSR = 0x0B;    ///< FIFO status register
constexpr uint8_t REG_WK2132_LSR = 0x0C;    ///< receive status register
constexpr uint8_t REG_WK2132_FDA = 0x0D;    ///< FIFO data register (r/w)

///@brief UART Channel register PAGE = 1
constexpr uint8_t REG_WK2132_BRH = 0x04;  ///< Channel baud rate configuration register high byte
constexpr uint8_t REG_WK2132_BRL = 0x05;  ///< Channel baud rate configuration register low byte
constexpr uint8_t REG_WK2132_BRD = 0x06;  ///< Channel baud rate configuration register decimal part
constexpr uint8_t REG_WK2132_RFI = 0x07;  ///< Channel receive FIFO interrupt trigger configuration register
constexpr uint8_t REG_WK2132_TFI = 0x08;  ///< Channel transmit FIFO interrupt trigger configuration register

/// @brief the max size we allow for transmissions
constexpr size_t FIFO_SIZE = 128;

class WK2132Channel;  // forward declaration
///////////////////////////////////////////////////////////////////////////////
/// @brief This class describes a WK2132 I²C component.
///
/// This class derives from two @ref esphome classes:
/// - The @ref Virtual Component class: we redefine the @ref Component::setup(),
///   @ref Component::dump_config() and @ref Component::get_setup_priority() methods
/// - The @ref i2c::I2CDevice class. From which we use some methods
///
/// We have one related class :
/// - The @ref WK2132Channel class that takes cares of the UART related methods
///////////////////////////////////////////////////////////////////////////////
class WK2132Component : public Component, public i2c::I2CDevice {
 public:
  /// @brief WK2132Component ctor. We store the I²C base address of the component
  WK2132Component() : base_address_{this->address_} {}

  void set_crystal(uint32_t crystal) { this->crystal_ = crystal; }
  void set_test_mode(int test_mode) { this->test_mode_ = test_mode; }

  //
  //  override Component methods
  //

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void loop() override;

 protected:
  friend class WK2132Channel;
  const char *reg_to_str_(int val);  // for debug

  /// @brief All write calls to I²C registers are performed through this method
  /// @param reg_address the register address
  /// @param channel the channel number (0-1). Only significant for UART registers
  /// @param buffer pointer to a buffer
  /// @param len length of the buffer
  /// @return the I²C error codes
  void write_wk2132_register_(uint8_t reg_number, uint8_t channel, const uint8_t *buffer, size_t len);

  /// @brief All read calls to I²C registers are performed through this method
  /// @param number the register number
  /// @param channel the channel number. Only significant for UART registers
  /// @param buffer the buffer pointer
  /// @param len length of the buffer
  /// @return the I²C error codes
  uint8_t read_wk2132_register_(uint8_t reg_number, uint8_t channel, uint8_t *buffer, size_t len);

  /// @brief for debug: returns the address of the device on the I²C bus
  /// @return the address
  int get_num_() const { return base_address_; }

  uint32_t crystal_;                         ///< crystal default value;
  uint8_t base_address_;                     ///< base address of I2C device
  std::bitset<8> test_mode_;                 ///< test mode 0 -> no tests
  uint8_t data_;                             ///< temporary buffer
  bool page1_{false};                        ///< set to true when in page1 mode
  std::vector<WK2132Channel *> children_{};  ///< the list of WK2132Channel UART children
  bool initialized_{false};                  ///< set to true when the wk2132 is initialized
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Describes a UART channel of a WK2132 I²C component.
///
/// This class derives from the virtual @ref gen_uart::GenericUART class.
///
/// Unfortunately I have not found **any documentation** about the
/// uart::UARTDevice and uart::UARTComponent classes of @ref ESPHome.\n
/// However it seems that both of them are based on Arduino library.\n
///
/// Most of the interfaces provided by the Arduino Serial library are **poorly
/// defined** and it seems that the API has even \b changed over time!\n
/// The esphome::uart::UARTDevice class directly relates to the **Serial Class**
/// in Arduino and that derives from **Stream class**.\n
/// For compatibility reason (?) many helper methods are made available in
/// ESPHome to read and write. Unfortunately in many cases these helpers
/// are missing the critical status information and therefore are even
/// more unsafe to use...\n
///////////////////////////////////////////////////////////////////////////////
class WK2132Channel : public wk2132::GenericUART {
 public:
  void set_parent(WK2132Component *parent) {
    this->parent_ = parent;
    this->parent_->children_.push_back(this);  // add ourself to the list (vector)
  }
  void set_channel(uint8_t channel) { this->channel_ = channel; }

 protected:
  friend class WK2132Component;

  void set_line_param_();
  void set_baudrate_();
  void setup_channel_();

  /// @brief Flush the output fifo.
  ///
  /// If we refer to Serial.flush() in Arduino it says: ** Waits for the transmission
  /// of outgoing serial data to complete. (Prior to Arduino 1.0, this the method was
  /// removing any buffered incoming serial data.). **
  /// The method tries to wait until all characters inside the fifo have been sent.
  /// It timeout after 100 ms and therefore at very low speed you can't be sure that
  /// all the characters have correctly been sent
  void flush() override;

  /// @brief Returns the number of bytes in the receive fifo
  /// @return the number of bytes in the fifo
  size_t rx_in_fifo_();

  /// @brief Returns the number of bytes in the transmit fifo
  /// @return the number of bytes in the fifo
  size_t tx_in_fifo_();

  /// @brief Reads data from the receive fifo to a buffer
  /// @param buffer the buffer
  /// @param len the number of bytes we want to read
  /// @return true if succeed false otherwise
  bool read_data_(uint8_t *buffer, size_t len);

  /// @brief Writes data from a buffer to the transmit fifo
  /// @param buffer the buffer
  /// @param len the number of bytes we want to write
  /// @return true if succeed false otherwise
  bool write_data_(const uint8_t *buffer, size_t len);

  /// @brief we transfer the bytes in the rx_fifo to the ring buffer
  void rx_fifo_to_ring_();
  /// @brief we transfer the bytes in the rx_fifo to the ring buffer
  void ring_to_tx_fifo_();

  size_t fifo_size() override { return FIFO_SIZE; }
  WK2132Component *parent_;  ///< Our WK2132component parent
  uint8_t channel_;          ///< Our Channel number
  uint8_t data_;             ///< one byte buffer
};

}  // namespace wk2132
}  // namespace esphome
