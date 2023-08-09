/// @file wk2132.h
/// @author @DrCoolZic
/// @brief  Interface of wk2132 class

#pragma once
#include <bitset>

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace wk2132 {

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
  // we store the base_address and we increment number of instances

  /// @brief WK2132Component ctor. We store the I²C base address of the
  /// component and we increment the number of instances of this class.
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

  /// @brief All write calls to I2C registers are performed through this method
  /// @param reg_address the register address
  /// @param channel the channel number. Only significant for UART registers
  /// @param buffer pointer to a buffer
  /// @param len length of the buffer
  /// @return the i2c error codes
  void write_wk2132_register_(uint8_t reg_number, uint8_t channel, const uint8_t *buffer, size_t len);

  /// @brief All read calls to I2C registers are performed through this method
  /// @param number the register number
  /// @param channel the channel number. Only significant for UART registers
  /// @param buffer the buffer pointer
  /// @param len length of the buffer
  /// @return the i2c error codes
  uint8_t read_wk2132_register_(uint8_t reg_number, uint8_t channel, uint8_t *buffer, size_t len);

  int get_num_() const { return int(this); }

  uint32_t crystal_{14745600L};              ///< crystal default value;
  uint8_t base_address_;                     ///< base address of I2C device
  int test_mode_{0};                         ///< debug flag
  uint8_t data_;                             ///< temporary buffer
  bool page1_{false};                        ///< set to true when in page1 mode
  bool initialized_{false};                  ///< true when initialization is finished
  std::vector<WK2132Channel *> children_{};  ///< @brief the list of WK2132Channel UART children
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Describes a UART channel of a WK2132 I²C component.
///
/// This class derives from the virtual @ref gen_uart::UARTComponent class.
///////////////////////////////////////////////////////////////////////////////
class WK2132Channel : public uart::UARTComponent {
 public:
  void set_parent(WK2132Component *parent) {
    this->parent_ = parent;
    this->parent_->children_.push_back(this);  // add ourself to the list (vector)
  }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void setup_channel();

  ///
  /// ** Important remark about the maximum buffer size **
  /// The maximum number of bytes you can read or write in one call
  /// is passed with the "len" parameter. This parameter should not
  /// be bigger than the fifo_size()
  ///

  /// @brief Write a specified number of bytes from a buffer to a serial port
  /// @param buffer pointer to the buffer
  /// @param len number of bytes to write
  void write_array(const uint8_t *buffer, size_t len) override;

  /// @brief Read a specified number of bytes from a serial port to a buffer
  /// @param buffer pointer to the buffer
  /// @param len number of bytes to read
  /// @return true if succeed, false otherwise
  bool read_array(uint8_t *buffer, size_t len) override;

  /// @brief Read next byte available from serial buffer without removing it
  /// from the FIFO
  /// @param buffer pointer to the byte
  /// @return true if succeed reading one byte, false if no character available
  bool peek_byte(uint8_t *buffer) override;

  /// @brief Return the number of bytes available for reading from the serial port.
  /// @return the number of bytes available in the receiver fifo
  int available() override { return this->rx_in_fifo_(); }

  /// @brief Flush the output fifo. This is the only way to wait until all the bytes
  /// in the transmit FIFO have been sent. The method timeout after 100 ms. Therefore
  /// at very low speed you can't be sure all characters are gone.
  void flush() override;

  //
  // overriden UARTComponent functions
  //

 protected:
  friend class WK2132Component;

  /// @brief cannot happen with external uart
  void check_logger_conflict() override {}

  void set_line_param_();
  void set_baudrate_();

  /// @brief Returns the number of bytes available in the receiver fifo
  /// @return the number of bytes we can read
  size_t rx_in_fifo_();

  /// @brief Returns the number of bytes available in the transmitter fifo
  /// @return the number of bytes we can write
  size_t tx_in_fifo_();

  /// @brief Reads data from the receiver fifo to a buffer
  /// @param buffer the buffer
  /// @param len the number of bytes we want to read
  /// @return true if succeed false otherwise
  bool read_data_(uint8_t *buffer, size_t len);

  /// @brief Writes data from a buffer to the transmitter fifo
  /// @param buffer the buffer
  /// @param len the number of bytes we want to write
  /// @return true if succeed false otherwise
  bool write_data_(const uint8_t *buffer, size_t len);

  /// @brief Return the size of the component's fifo
  /// @return the size
  size_t fifo_size_() { return 128; }

  bool safe_{true};  // false will speed up operation but is unsafe
  struct PeekBuffer {
    uint8_t data;
    bool empty{true};
  } peek_buffer_;  // temporary storage when you peek data

  void uart_send_test_(char *preamble);
  void uart_receive_test_(char *preamble, bool print_buf = true);

  WK2132Component *parent_;  ///< Our WK2132component parent
  uint8_t channel_;          ///< Our Channel number
  uint8_t data_;             ///< one byte buffer
};

}  // namespace wk2132
}  // namespace esphome
