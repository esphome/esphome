/// @file wk2132.h
/// @author DrCoolZic
/// @brief  wk2132 classes interface

#pragma once
#include <bitset>
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace wk2132 {

/// @brief the max size we allow for transmissions
constexpr size_t FIFO_SIZE = 128;

/// @brief size of the ring buffer
constexpr size_t RING_BUFFER_SIZE = 128;
///////////////////////////////////////////////////////////////////////////////
/// @brief This is an helper class that provides a simple ring buffers
/// that implements a FIFO function
///
/// This ring buffer is used to buffer the exchanges between the receiver
/// HW fifo and the client. Usually to read characters you first
/// check if bytes were received and if so you read them.
/// There is a problem if you try to the received bytes one by one.
/// If the registers are located on the chip everything is fine but with a
/// device like the wk2132 these registers are remote and therefore accessing
/// them requires several transactions on the I²C bus which is relatively slow.
/// One solution would be for the client to check the number of bytes available
/// and to read them all using the read_array() method. Unfortunately
/// most client I have reviewed are reading one character at a time in a
/// while loop which is the most inefficient way of doing things.
/// Therefore the solution I have chosen to implement is to
/// store received bytes locally in a buffer as soon as they arrive. With
/// this solution the bytes are stored locally and therefore accessible
/// very quickly when requested one by one.
/// @image html read_cycles.png
///////////////////////////////////////////////////////////////////////////////
template<typename T, size_t SIZE> class RingBuffer {
 public:
  /// @brief pushes an item at the tail of the fifo
  /// @param item item to push
  /// @return true if item has been pushed, false il item was not pushed (buffer full)
  bool push(const T item) {
    if (is_full())
      return false;
    rb_[head_] = item;
    head_ = (head_ + 1) % SIZE;
    count_++;
    return true;
  }

  /// @brief return and remove the item at head of the fifo
  /// @param item item read
  /// @return true if item has been retrieved, false il no item was found (buffer empty)
  bool pop(T &item) {
    if (is_empty())
      return false;
    item = rb_[tail_];
    tail_ = (tail_ + 1) % SIZE;
    count_--;
    return true;
  }

  /// @brief return the value of the item at fifo's head without removing it
  /// @param item pointer to item to return
  /// @return true if item has been retrieved, false il no item was found (buffer empty)
  bool peek(T &item) {
    if (is_empty())
      return false;
    item = rb_[tail_];
    return true;
  }

  /// @brief test is the Ring Buffer is empty ?
  /// @return true if empty
  bool is_empty() { return (count_ == 0); }

  /// @brief test is the ring buffer is full ?
  /// @return true if full
  bool is_full() { return (count_ == SIZE); }

  /// @brief return the number of item in the ring buffer
  /// @return the count
  size_t count() { return count_; }

  /// @brief returns the number of free positions in the buffer
  /// @return how many items can be added
  size_t free() { return SIZE - count_; }

  /// @brief clear the buffer content
  void clear() { head_ = tail_ = count_ = 0; }

 private:
  std::array<T, SIZE> rb_{0};
  int head_{0};      // points to the next element to write
  int tail_{0};      // points to the next element to read
  size_t count_{0};  // count number of element in the buffer
};

//
// brief WK2132 registers
//

/// GENA description of global control register:
/// @code
///  * -------------------------------------------------------------------------
///  * |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  * -------------------------------------------------------------------------
///  * |   M1   |   M0   |              RESERVED             |  UT2EN |  UT1EN |
///  * -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_GENA = 0x00;

///  GRST description of global reset register:
/// @code
///  * -------------------------------------------------------------------------
///  * |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  * -------------------------------------------------------------------------
///  * |       RSV       | UT2SLE | UT1SLE |       RSV       | UT2RST | UT1RST |
///  * -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_GRST = 0x01;

constexpr uint8_t REG_WK2132_GMUT = 0x02;  ///< Global main UART control register

/// @brief UART Channel register when PAGE = 0
constexpr uint8_t REG_WK2132_SPAGE = 0x03;  ///< UART page control register

/// SCR description of UART Serial control register:
/// @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  -------------------------------------------------------------------------
///  |                     RSV                    | SLEEPEN|  TXEN  |  RXEN  |
///  -------------------------------------------------------------------------
/// @encode
constexpr uint8_t REG_WK2132_SCR = 0x04;

/// LCR description of line configuration register:
/// @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  -------------------------------------------------------------------------
///  |        RSV      |  BREAK |  IREN  |  PAEN  |      PAM        |  STPL  |
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_LCR = 0x05;

/// FCR description of UART FIFO control register:
/// @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |      TFTRIG     |      RFTRIG     |  TFEN  |  RFEN  |  TFRST |  RFRST |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_FCR = 0x06;
constexpr uint8_t REG_WK2132_SIER = 0x07;   ///< interrupt enable register
constexpr uint8_t REG_WK2132_SIFR = 0x08;   ///< interrupt flag register
constexpr uint8_t REG_WK2132_TFCNT = 0x09;  ///< transmit FIFO value register
constexpr uint8_t REG_WK2132_RFCNT = 0x0A;  ///< receive FIFO value register

/// FSR FIFO status register:
/// @code
/// * -------------------------------------------------------------------------
/// * |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// * -------------------------------------------------------------------------
/// * |  RFOE  |  RFBI  |  RFFE  |  RFPE  |  RDAT  |  TDAT  |  TFULL |  TBUSY |
/// * -------------------------------------------------------------------------
/// @endcode
/// WARNING:\n
/// The received buffer can hold 256 bytes. However, as the RFCNT reg is 8 bits,
/// in this case the value 256 is reported as 0 ! Therefore the RFCNT count can be
/// zero when there is 0 byte or 256 bytes in the buffer. So if we have RXDAT = 1
/// and RFCNT = 0 it should be interpreted as there is 256 bytes in the FIFO.
/// Note that in case of overflow the RFOE goes to one **but** as soon as you read
/// the FSR this bit is cleared. Therefore Overflow can be read only once even if
/// still in overflow.
///
/// The same remark is valid for the transmit buffer but here we have to check the
/// TFULL flag. So if TFULL is set and TFCNT is 0 this should be interpreted as 256
constexpr uint8_t REG_WK2132_FSR = 0x0B;

constexpr uint8_t REG_WK2132_LSR = 0x0C;  ///< receive status register
constexpr uint8_t REG_WK2132_FDA = 0x0D;  ///< FIFO data register (r/w)

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
///
/// The image below shows some relationship between UART related classes
///  @image html uart.png width=1024px
///////////////////////////////////////////////////////////////////////////////
class WK2132Component : public Component, public i2c::I2CDevice {
 public:
  /// @brief WK2132Component ctor. We store the I²C base address of the component
  WK2132Component() : base_address_{this->address_} {}

  void set_crystal(uint32_t crystal) { this->crystal_ = crystal; }
  void set_test_mode(int test_mode) { this->test_mode_ = test_mode; }
  void set_name(std::string name) { this->name_ = std::move(name); }
  const char *get_name() { return this->name_.c_str(); }

  //
  //  override virtual Component methods
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

  uint32_t crystal_;                         ///< crystal default value;
  uint8_t base_address_;                     ///< base address of I2C device
  int test_mode_;                            ///< test mode 0 -> no tests
  uint8_t data_;                             ///< temporary buffer
  bool page1_{false};                        ///< set to true when in page1 mode
  std::vector<WK2132Channel *> children_{};  ///< the list of WK2132Channel UART children
  bool initialized_{false};                  ///< set to true when the wk2132 is initialized
  std::string name_;                         ///< store name of entity
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
class WK2132Channel : public uart::UARTComponent {
 public:
  void set_parent(WK2132Component *parent) {
    this->parent_ = parent;
    this->parent_->children_.push_back(this);  // add ourself to the list (vector)
  }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_channel_name(std::string name) { this->name_ = std::move(name); }
  const char *get_channel_name() { return this->name_.c_str(); }

  //
  // we implement the virtual class from UARTComponent
  //

  /// @brief Writes a specified number of bytes toward a serial port
  /// @param buffer pointer to the buffer
  /// @param len number of bytes to write
  ///
  /// This method sends 'len' characters from the buffer to the serial line.
  /// Unfortunately (unlike the Arduino equivalent) this method
  /// does not return any flag and therefore it is not possible to know
  /// if any/all bytes have been transmitted correctly. Another problem
  /// is that it is not possible to know ahead of time how many bytes we
  /// can safely send as there is no tx_available() method provided!
  /// To avoid overrun when using the write method you can use the flush()
  /// method to wait until the transmit fifo is empty.
  ///
  /// Typical usage could be:
  /// @code
  ///   // ...
  ///   uint8_t buffer[128];
  ///   // ...
  ///   write_array(&buffer, len);
  ///   flush();
  ///   // ...
  /// @endcode
  void write_array(const uint8_t *buffer, size_t len) override;

  /// @brief Reads a specified number of bytes from a serial port
  /// @param buffer buffer to store the bytes
  /// @param len number of bytes to read
  /// @return true if succeed, false otherwise
  ///
  /// Typical usage:
  /// @code
  ///   // ...
  ///   auto len = available();
  ///   uint8_t buffer[128];
  ///   if (len > 0) {
  ///     auto status = read_array(&buffer, len)
  ///     // test status ...
  ///   }
  /// @endcode
  bool read_array(uint8_t *buffer, size_t len) override;

  /// @brief Reads first byte in FIFO without removing it
  /// @param buffer pointer to the byte
  /// @return true if succeed reading one byte, false if no character available
  ///
  /// This method returns the next byte from receiving buffer without
  /// removing it from the internal fifo. It returns true if a character
  /// is available and has been read, false otherwise.\n
  bool peek_byte(uint8_t *buffer) override { return this->receive_buffer_.peek(*buffer); }

  /// @brief Returns the number of bytes in the receive buffer
  /// @return the number of bytes available in the receiver fifo
  int available() override;

  /// @brief Flush the output fifo.
  ///
  /// If we refer to Serial.flush() in Arduino it says: ** Waits for the transmission
  /// of outgoing serial data to complete. (Prior to Arduino 1.0, this the method was
  /// removing any buffered incoming serial data.). **
  void flush() override;

 protected:
  friend class WK2132Component;

  /// @brief this cannot happen with external uart
  void check_logger_conflict() override {}

  void reset_fifo_();
  void set_line_param_();
  void set_baudrate_();
  void setup_channel_();

  /// @brief Returns the number of bytes in the receive fifo
  /// @return the number of bytes in the fifo
  size_t rx_in_fifo_();

  /// @brief Returns the number of bytes in the transmit fifo
  /// @return the number of bytes in the fifo
  size_t tx_in_fifo_();

  /// @brief test if transmit buffer is not empty
  /// @return true if not empty
  bool tx_fifo_is_not_empty_();

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

  size_t rx_fifo_to_buffer_();

  /// @brief the buffer where we store temporarily the bytes received
  RingBuffer<uint8_t, RING_BUFFER_SIZE> receive_buffer_;
  bool flush_requested_{false};  ///< flush was requested but not honored
  WK2132Component *parent_;      ///< Our WK2132component parent
  uint8_t channel_;              ///< Our Channel number
  uint8_t data_;                 ///< one byte buffer
  std::string name_;             ///< name of the entity
};

}  // namespace wk2132
}  // namespace esphome
