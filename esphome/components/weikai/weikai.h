/// @file weikai.h
/// @author DrCoolZic
/// @brief  WeiKai component family - classes declaration
/// @date Last Modified: 2024/04/06 14:44:17
/// @details The classes declared in this file can be used by the Weikai family
/// of UART and GPIO expander components. As of today it provides support for
///     wk2124_spi, wk2132_spi, wk2168_spi, wk2204_spi, wk2212_spi,
///                 wk2132_i2c, wk2168_i2c, wk2204_i2c, wk2212_i2c

#pragma once
#include <algorithm>
#include <array>
#include <cinttypes>
#include <memory>
#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "wk_reg_def.h"

/// When the TEST_COMPONENT flag is defined we include some auto-test methods. Used to test the software during
/// development but can also be used in situ to test if the component is working correctly. For release we do
/// not set it by default but you can set it by using the following lines in you configuration file:
/// @code
/// esphome:
///   platformio_options:
///     build_flags:
///       - -DTEST_COMPONENT
/// @endcode
// #define TEST_COMPONENT

namespace esphome::weikai {

/// @brief XFER_MAX_SIZE defines the maximum number of bytes allowed during one transfer.
#if defined(I2C_BUFFER_LENGTH)
constexpr size_t XFER_MAX_SIZE = I2C_BUFFER_LENGTH;
#else
constexpr size_t XFER_MAX_SIZE = 128;
#endif

/// @brief size of the internal WeiKai FIFO
constexpr size_t FIFO_SIZE = 256;

/// @brief size of the ring buffer set to size of the FIFO
constexpr size_t RING_BUFFER_SIZE = FIFO_SIZE;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief This is an helper class that provides a simple ring buffers that works as a FIFO
/// @details This ring buffer is used to buffer the bytes received in the FIFO of the Weika device. The best way to read
/// characters from the device FIFO, is to first check how many bytes were received and then read them all at once.
/// Unfortunately in all the code I have reviewed the characters are read one by one in a while loop by checking if
/// bytes are available then reading the byte until no more byte available. This is pretty inefficient for two reasons:
/// - Fist you need to perform a test for each byte to read
/// - and second you call the read byte method for each character.
/// .
/// Assuming you need to read 100 bytes that results into 200 calls. This is to compare to 2 calls (one to find the
/// number of bytes available plus one to read all the bytes) in the best case! If the registers you read are located on
/// the micro-controller this is acceptable because the registers can be accessed fast. But when the registers are
/// located on a remote device accessing them requires several cycles on a slow bus. As it it not possible to fix this
/// problem by asking users to rewrite their code, I have implemented this ring buffer that store the bytes received
/// locally.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, size_t SIZE> class WKRingBuffer {
  static_assert(SIZE > 0 && (SIZE & (SIZE - 1)) == 0, "WKRingBuffer SIZE must be a power of two");
  static_assert(SIZE <= 0x8000, "WKRingBuffer SIZE too large for 16 bit indexes");

 public:
  /// @brief pushes an item at the tail of the fifo
  /// @param item item to push
  /// @return true if item has been pushed, false il item could not pushed (buffer full)
  bool push(const T item) {
    if (is_full())
      return false;
    this->rb_[this->head_] = item;
    this->head_ = (this->head_ + 1) & MASK;
    this->count_++;
    return true;
  }

  /// @brief return and remove the item at head of the fifo
  /// @param item item read
  /// @return true if an item has been retrieved, false il no item available (buffer empty)
  bool pop(T &item) {
    if (is_empty())
      return false;
    item = this->rb_[this->tail_];
    this->tail_ = (this->tail_ + 1) & MASK;
    if (--this->count_ == 0)
      this->head_ = this->tail_ = 0;  // empty: restart at the beginning so that bulk writes stay contiguous
    return true;
  }

  /// @brief removes up to n items from the buffer and copies them to dst (at most two contiguous copies)
  /// @param dst destination array
  /// @param n maximum number of items to retrieve
  /// @return number of items actually retrieved
  size_t pop_bulk(T *dst, size_t n) {
    n = std::min<size_t>(n, this->count_);
    size_t const first = std::min<size_t>(n, SIZE - this->tail_);
    std::copy_n(this->rb_.data() + this->tail_, first, dst);
    std::copy_n(this->rb_.data(), n - first, dst + first);
    this->tail_ = (this->tail_ + n) & MASK;
    this->count_ -= n;
    if (this->count_ == 0)
      this->head_ = this->tail_ = 0;  // empty: restart at the beginning so that bulk writes stay contiguous
    return n;
  }

  /// @brief return the value of the item at fifo's head without removing it
  /// @param item pointer to item to return
  /// @return true if item has been retrieved, false il no item available (buffer empty)
  bool peek(T &item) const {
    if (is_empty())
      return false;
    item = this->rb_[this->tail_];
    return true;
  }

  /// @brief pointer to the next free slot. Up to contiguous_free() items can be written there directly
  /// (for example by a bus read) and then validated with commit().
  inline T *write_ptr() { return this->rb_.data() + this->head_; }

  /// @brief number of free slots that can be written starting at write_ptr() without wrapping around
  inline size_t contiguous_free() const { return std::min<size_t>(SIZE - this->count_, SIZE - this->head_); }

  /// @brief validate n items that have been written directly at write_ptr()
  /// @param n number of items written (must be <= contiguous_free())
  inline void commit(size_t n) {
    this->head_ = (this->head_ + n) & MASK;
    this->count_ += n;
  }

  /// @brief test is the Ring Buffer is empty ?
  /// @return true if empty
  inline bool is_empty() const { return (this->count_ == 0); }

  /// @brief test is the ring buffer is full ?
  /// @return true if full
  inline bool is_full() const { return (this->count_ == SIZE); }

  /// @brief return the number of item in the ring buffer
  /// @return the number of items
  inline size_t count() const { return this->count_; }

  /// @brief returns the number of free positions in the buffer
  /// @return how many items can be added
  inline size_t free() const { return SIZE - this->count_; }

  /// @brief clear the buffer content
  inline void clear() { this->head_ = this->tail_ = this->count_ = 0; }

 protected:
  static constexpr uint16_t MASK = SIZE - 1;
  std::array<T, SIZE> rb_{};  ///< the ring buffer
  uint16_t tail_{0};          ///< position of the next element to read
  uint16_t head_{0};          ///< position of the next element to write
  uint16_t count_{0};         ///< count number of element in the buffer
};

/// @brief Converts the parity enum value to a C string (shared by the SPI/I2C variants)
const char *p2s(uart::UARTParityOptions parity);

/// @brief Converts a register address to a C string (used in log messages)
const char *reg_to_str(int reg, bool page1);

/// @brief Displays a buffer in hexadecimal format (32 hex values / line) at VERY_VERBOSE level
void print_buffer(const uint8_t *data, size_t length);

class WeikaiComponent;
// class WeikaiComponentSPI;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief WeikaiRegister objects acts as proxies to access remote register independently of the bus type.
/// @details This is an abstract interface class that provides many operations to access to registers while hiding
/// the actual implementation. This allow to accesses the registers in the Weikai component abstract class independently
/// of the actual bus (I2C, SPI). The derived classes will actually implements the specific bus operations dependant of
/// the bus used.
/// @n typical usage of WeikaiRegister:
/// @code
///   WeikaiRegister reg_X {&WeikaiComponent, ADDR_REGISTER_X, CHANNEL_NUM}  // declaration
///   reg_X |= 0x01;    // set bit 0 of the weikai register
///   reg_X &= ~0x01;   // reset bit 0 of the weikai register
///   reg_X = 10;       // Set the value of weikai register
///   uint val = reg_X; // get the value of weikai register
/// @endcode
class WeikaiRegister {
 public:
  /// @brief WeikaiRegister constructor.
  /// @param comp our parent WeikaiComponent
  /// @param reg address of the register
  /// @param channel the channel of this register
  WeikaiRegister(WeikaiComponent *const comp, uint8_t reg, uint8_t channel)
      : comp_(comp), register_(reg), channel_(channel) {}
  virtual ~WeikaiRegister() {}

  /// @brief overloads the = operator. This is used to set a value into the weikai register
  /// @param value to be set
  /// @return this object
  WeikaiRegister &operator=(uint8_t value);

  /// @brief overloads the compound &= operator. This is often used to reset bits in the weikai register
  /// @param value performs an & operation with value and store the result
  /// @return this object
  WeikaiRegister &operator&=(uint8_t value);

  /// @brief overloads the compound |= operator. This is often used to set bits in the weikai register
  /// @param value performs an | operation with value and store the result
  /// @return this object
  WeikaiRegister &operator|=(uint8_t value);

  /// @brief cast operator that returns the content of the weikai register
  operator uint8_t() const { return read_reg(); }

  /// @brief reads the register
  /// @return the value read from the register
  virtual uint8_t read_reg() const = 0;

  /// @brief writes the register
  /// @param value to write in the register
  virtual void write_reg(uint8_t value) = 0;

  /// @brief read an array of bytes from the receiver fifo
  /// @param data pointer to data buffer
  /// @param length number of bytes to read (at most XFER_MAX_SIZE)
  virtual void read_fifo(uint8_t *data, size_t length) const = 0;

  /// @brief write an array of bytes to the transmitter fifo
  /// @param data pointer to data buffer
  /// @param length number of bytes to write (at most XFER_MAX_SIZE)
  virtual void write_fifo(uint8_t *data, size_t length) = 0;

  WeikaiComponent *const comp_;  ///< pointer to our parent (aggregation)
  uint8_t register_;             ///< address of the register
  uint8_t channel_;              ///< channel for this register
};

class WeikaiChannel;  // forward declaration
////////////////////////////////////////////////////////////////////////////////////
/// @brief The WeikaiComponent class stores the information global to the WeiKai component
/// and provides methods to set/access this information. It is also the container of
/// the WeikaiChannel children objects. This class is derived from esphome::Component
/// class.
////////////////////////////////////////////////////////////////////////////////////
class WeikaiComponent : public Component {
 public:
  /// @brief virtual destructor
  virtual ~WeikaiComponent() {}

  /// @brief store crystal frequency
  /// @param crystal frequency
  void set_crystal(uint32_t crystal) { this->crystal_ = crystal; }

  /// @brief store if the component is in test mode
  /// @param test_mode 0=normal mode any other values mean component in test mode
  void set_test_mode(int test_mode) { this->test_mode_ = test_mode; }

  /// @brief store the name for the component
  /// @param name the name as defined by the python code generator (must have static storage duration:
  /// the code generator always passes a string literal)
  void set_name(const char *name) { this->name_ = name; }

  /// @brief Get the name of the component
  /// @return the name
  const char *get_name() const { return this->name_; }

  /// @brief override the Component loop()
  void loop() override;

  bool page1() { return page1_; }

  /// @brief Factory method to create a Register object
  /// @param reg address of the register
  /// @param channel channel associated with this register
  /// @return a reference to WeikaiRegister
  virtual WeikaiRegister &reg(uint8_t reg, uint8_t channel) = 0;

 protected:
  friend class WeikaiChannel;

  /// @brief Get the priority of the component
  /// @return the priority
  /// @details The priority is set  below setup_priority::BUS because we use
  /// the spi/i2c busses (which has a priority of BUS) to communicate and the WeiKai
  /// therefore it is seen by our client almost as if it was a bus.
  float get_setup_priority() const override { return setup_priority::BUS - 0.1F; }

  friend class WeikaiGPIOPin;
  /// Helper method to read the value of a pin.
  bool read_pin_val_(uint8_t pin);

  /// Helper method to write the value of a pin.
  void write_pin_val_(uint8_t pin, bool value);

  /// Helper method to set the pin mode of a pin.
  void set_pin_direction_(uint8_t pin, gpio::Flags flags);

#ifdef TEST_COMPONENT
  /// @defgroup test_ Test component information
  /// @brief Contains information about the auto-tests of the component
  /// @{
  void test_gpio_input_();
  void test_gpio_output_();
  /// @}
#endif

  uint8_t pin_config_{0x00};                 ///< pin config mask: 1 means OUTPUT, 0 means INPUT
  uint8_t output_state_{0x00};               ///< output state: 1 means HIGH, 0 means LOW
  uint8_t input_state_{0x00};                ///< input pin states: 1 means HIGH, 0 means LOW
  uint32_t crystal_;                         ///< crystal value;
  int test_mode_;                            ///< test mode value (0 -> no tests)
  bool page1_{false};                        ///< set to true when in "page1 mode"
  std::vector<WeikaiChannel *> children_{};  ///< the list of WeikaiChannel UART children
  const char *name_{""};                     ///< name of entity (string literal, no heap allocation)
};

///////////////////////////////////////////////////////////////////////////////
/// @brief Helper class to expose a WeiKai family IO pin as an internal GPIO pin.
///////////////////////////////////////////////////////////////////////////////
class WeikaiGPIOPin final : public GPIOPin {
 public:
  void set_parent(WeikaiComponent *parent) { this->parent_ = parent; }
  void set_pin(uint8_t pin) { this->pin_ = pin; }
  void set_inverted(bool inverted) { this->inverted_ = inverted; }
  void set_flags(gpio::Flags flags) { this->flags_ = flags; }

  gpio::Flags get_flags() const override { return this->flags_; }

  void setup() override;
  size_t dump_summary(char *buffer, size_t len) const override;
  void pin_mode(gpio::Flags flags) override { this->parent_->set_pin_direction_(this->pin_, flags); }
  bool digital_read() override { return this->parent_->read_pin_val_(this->pin_) != this->inverted_; }
  void digital_write(bool value) override { this->parent_->write_pin_val_(this->pin_, value != this->inverted_); }

 protected:
  WeikaiComponent *parent_{nullptr};
  uint8_t pin_;
  bool inverted_;
  gpio::Flags flags_;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief The WeikaiChannel class is used to implement all the virtual methods of the ESPHome
/// uart::UARTComponent virtual class. This class is common to the different members of the Weikai
/// components family and therefore avoid code duplication.
///////////////////////////////////////////////////////////////////////////////////////////////////
class WeikaiChannel final : public uart::UARTComponent {
 public:
  /// @brief We belongs to this WeikaiComponent
  /// @param parent pointer to the component we belongs to
  void set_parent(WeikaiComponent *parent) {
    this->parent_ = parent;
    this->parent_->children_.push_back(this);  // add ourself to the list (vector)
  }

  /// @brief Sets the channel number
  /// @param channel number
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  /// @brief The name as generated by the Python code generator
  /// @param name of the channel
  void set_channel_name(const char *name) { this->name_ = name; }

  /// @brief Get the channel name
  /// @return the name
  const char *get_channel_name() const { return this->name_; }

  /// @brief Setup the channel
  void setup_channel();

  /// @brief dump channel information
  void dump_channel();

  /// @brief Factory method to create a WeikaiRegister proxy object
  /// @param reg address of the register
  /// @return a reference to WeikaiRegister
  WeikaiRegister &reg(uint8_t reg) { return this->parent_->reg(reg, channel_); }

  //
  // we implements/overrides the virtual class from UARTComponent
  //

  /// @brief Writes a specified number of bytes to a serial port
  /// @param buffer pointer to the buffer
  /// @param length number of bytes to write
  /// @details This method sends 'length' characters from the buffer to the serial line. Unfortunately (unlike the
  /// Arduino equivalent) this method does not return any flag and therefore it is not possible to know if any/all bytes
  /// have been transmitted correctly. Another problem is that it is not possible to know ahead of time how many bytes
  /// we can safely send as there is no tx_available() method provided! To avoid overrun when using the write method you
  /// can use the flush() method to wait until the transmit fifo is empty.
  /// @n Typical usage could be:
  /// @code
  ///   // ...
  ///   uint8_t buffer[128];
  ///   // ...
  ///   write_array(&buffer, length);
  ///   flush();
  ///   // ...
  /// @endcode
  void write_array(const uint8_t *buffer, size_t length) override;

  /// @brief Reads a specified number of bytes from a serial port
  /// @param buffer buffer to store the bytes
  /// @param length number of bytes to read
  /// @return true if succeed, false otherwise
  /// @details Typical usage:
  /// @code
  ///   // ...
  ///   auto length = available();
  ///   uint8_t buffer[128];
  ///   if (length > 0) {
  ///     auto status = read_array(&buffer, length)
  ///     // test status ...
  ///   }
  /// @endcode
  bool read_array(uint8_t *buffer, size_t length) override;

  /// @brief Reads the first byte in FIFO without removing it
  /// @param buffer pointer to the byte
  /// @return true if succeed reading one byte, false if no character available
  /// @details This method returns the next byte from receiving buffer without removing it from the internal fifo. It
  /// returns true if a character is available and has been read, false otherwise.\n
  bool peek_byte(uint8_t *buffer) override;

  /// @brief Returns the number of bytes in the receive buffer
  /// @return the number of bytes available in the receiver fifo
  size_t available() override;

  /// @brief Flush the output fifo.
  /// @details If we refer to Serial.flush() in Arduino it says: ** Waits for the transmission of outgoing serial data
  /// to complete. (Prior to Arduino 1.0, this the method was removing any buffered incoming serial data.). ** Therefore
  /// we wait until all bytes are gone with a timeout of 100 ms
  uart::UARTFlushResult flush() override;

#if defined(USE_ESP8266) || defined(USE_ESP32)
  /// @brief Re-apply the current line settings (baud, parity, etc) to the channel.
  void load_settings(bool dump_config) override {
    this->set_line_param_();
    this->set_baudrate_();
  }
  using UARTComponent::load_settings;  // also bring in the no-arg overload for convenience
#endif

 protected:
  friend class WeikaiComponent;

  /// @brief this cannot happen with external uart therefore we do nothing
  void check_logger_conflict() override {}

  /// @brief reset the weikai internal FIFO
  void reset_fifo_();

  /// @brief set the line parameters
  void set_line_param_();

  /// @brief set the baud rate
  void set_baudrate_();

  /// @brief Reads the FIFO status register (FSR) of the channel.
  /// @details Reading FSR clears the RFOE (receive overflow) flag in the chip, so every FSR read goes through this
  /// method which remembers the flag until rx_in_fifo_() has reported it.
  /// @return the value of the FSR register
  uint8_t read_fsr_();

  /// @brief Returns the number of bytes in the receive fifo
  /// @return the number of bytes in the fifo
  size_t rx_in_fifo_();

  /// @brief Returns the number of bytes in the transmit fifo
  /// @return the number of bytes in the fifo
  size_t tx_in_fifo_();

  /// @brief number of bytes that can still be written to the transmit FIFO
  size_t tx_room_();

  /// @brief time in ms needed to send 'bytes' bytes with the current line settings (rounded down)
  uint32_t tx_time_ms_(size_t bytes) const;

  /// @brief test if transmit buffer is not empty in the status register (optimization)
  /// @return true if not emptygroup test_
  bool tx_fifo_is_not_empty_();

  /// @brief transfer bytes from the weikai internal FIFO to the buffer (if any)
  /// @return number of bytes transferred
  /// @details The FIFO content is read directly into the ring buffer (no intermediate copy). The receive FIFO
  /// is polled only once per call: bytes received in the meantime are picked up by the next call.
  size_t xfer_fifo_to_buffer_();

  /// @brief check if channel is alive
  /// @return true if OK
  bool check_channel_down_();

#ifdef TEST_COMPONENT
  /// @ingroup test_
  /// @{

  /// @brief Test the write_array() method
  /// @param message to display
  void uart_send_test_(char *message);

  /// @brief Test the read_array() method
  /// @param message to display
  /// @return true if success
  bool uart_receive_test_(char *message);
  /// @}
#endif

  /// @brief the buffer where we store temporarily the bytes received
  WKRingBuffer<uint8_t, RING_BUFFER_SIZE> receive_buffer_;
  WeikaiComponent *parent_;  ///< our WK2168component parent
  uint8_t channel_;          ///< our Channel number
  const char *name_{""};     ///< name of the entity (string literal, no heap allocation)
  /// Lower bound of the free room in the transmit FIFO: the chip only empties it, only we fill it.
  /// Saves reading the room before every write.
  uint16_t tx_room_estimate_{0};
  bool rfoe_latched_{false};  ///< RFOE seen by an FSR read that was not made by rx_in_fifo_()
};

}  // namespace esphome::weikai
