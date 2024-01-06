/// @file wk2168.h
/// @author DrCoolZic
/// @brief  wk2168 classes declaration

#pragma once
#include <bitset>
#include <memory>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "wk2168_def.h"
#if defined(USE_ESP32_FRAMEWORK_ARDUINO) && defined(I2C_COMPILE)
#include "Wire.h"
#endif

/// When TEST_COMPONENT is defined we include some auto-test methods. Used to test the software during wk2168
/// development but can also be used in situ to test if the component is working correctly. In production we do not set
/// it you can add it by using the following lines in you configuration file:
/// @code
/// esphome:
///   name: test-wk2168-i2c-arduino
///   platformio_options:
///     build_flags:
///       - -DTEST_COMPONENT
// #define TEST_COMPONENT

namespace esphome {
namespace wk2168 {
/// @brief XFER_MAX_SIZE defines the maximum number of bytes we allow during one transfer.
/// When using the Arduino framework by default the maximum number of bytes that can be transferred is 128 bytes. But
/// this can be changed by defining the macro I2C_BUFFER_LENGTH during compilation. In fact the __init__.py file take
/// care of that when analyzing the Yaml configuration. When using the ESP-IDF Framework the maximum number of bytes
/// that can be transferred is 256 bytes.
/// @bug At the time of writing (Dec 2023) there is a bug in the Arduino framework in the TwoWire::requestFrom() method
/// that limits the number of bytes we can read to 255. For this reasons we limit the XFER_MAX_SIZE to 255.

#if defined(USE_ESP8266)
constexpr size_t XFER_MAX_SIZE = 128;  // ESP8266 to be checked

#elif defined(USE_ESP32_FRAMEWORK_ESP_IDF)  // ESP32 and framework IDF
constexpr size_t XFER_MAX_SIZE = 256;

// ESP32 and framework Arduino
#if !defined(I2C_BUFFER_LENGTH)             // this is only to make clang-tidy Arduino 4/4 happy
constexpr size_t I2C_BUFFER_LENGTH = 255;
#endif
#elif I2C_BUFFER_LENGTH < 256  // Here we are using an USE_ESP32_FRAMEWORK_ARDUINO
constexpr size_t XFER_MAX_SIZE = I2C_BUFFER_LENGTH;  // ESP32 & FRAMEWORK_ARDUINO
#else
constexpr size_t XFER_MAX_SIZE = 255;  // ESP32 & FRAMEWORK_ARDUINO we limit to 255 because Arduino' framework error
#endif

/// @brief size of the internal WK2168 FIFO
constexpr size_t FIFO_SIZE = 256;

/// @brief size of the ring buffer
/// @details We set the size of ring buffer to XFER_MAX_SIZE
constexpr size_t RING_BUFFER_SIZE = XFER_MAX_SIZE;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief This is an helper class that provides a simple ring buffers that works as a FIFO
/// @details This ring buffer is used to buffer the bytes received in the FIFO of the I2C device. The best way to read
/// characters from the line, is to first check how many bytes were received and then read them all at once.
/// Unfortunately on almost all the code I have reviewed the characters are read one by one in a while loop: check if
/// bytes are available then read the next one until no more byte available. This is pretty inefficient for two reasons:
/// - Fist you need to perform a test for each byte to read
/// - and second you call the read byte method for each character.
/// Assuming you need to read 100 bytes that results into 200 calls instead of 100. Where if you had followed the good
/// practice this could be done in 2 calls (one to find the number of bytes available plus one to read all the bytes! If
/// the registers you read are located on the micro-controller this is not too bad even if it roughly double the process
/// time. But when the registers to check are located on the WK2168 device the performance can get pretty bad as each
/// access to a register requires several cycles on the slow I2C bus. To fix this problem we could ask the users to
/// rewrite their code to follow the good practice but this is not obviously not realistic.
/// @n Therefore the solution I have implemented is to store the bytes received in the FIFO on a local ring buffer.
/// The carefully crafted algorithm used reduces drastically the number of transactions on the i2c bus but more
/// importantly it improve the performance as if the remote registers were located on the micro-controller
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T, size_t SIZE> class RingBuffer {
 public:
  /// @brief pushes an item at the tail of the fifo
  /// @param item item to push
  /// @return true if item has been pushed, false il item could not pushed (buffer full)
  bool push(const T item) {
    if (is_full())
      return false;
    this->rb_[this->head_] = item;
    this->head_ = (this->head_ + 1) % SIZE;
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
    this->tail_ = (this->tail_ + 1) % SIZE;
    this->count_--;
    return true;
  }

  /// @brief return the value of the item at fifo's head without removing it
  /// @param item pointer to item to return
  /// @return true if item has been retrieved, false il no item available (buffer empty)
  bool peek(T &item) {
    if (is_empty())
      return false;
    item = this->rb_[this->tail_];
    return true;
  }

  /// @brief test is the Ring Buffer is empty ?
  /// @return true if empty
  inline bool is_empty() { return (this->count_ == 0); }

  /// @brief test is the ring buffer is full ?
  /// @return true if full
  inline bool is_full() { return (this->count_ == SIZE); }

  /// @brief return the number of item in the ring buffer
  /// @return the number of items
  inline size_t count() { return this->count_; }

  /// @brief returns the number of free positions in the buffer
  /// @return how many items can be added
  inline size_t free() { return SIZE - this->count_; }

  /// @brief clear the buffer content
  inline void clear() { this->head_ = this->tail_ = this->count_ = 0; }

 private:
  std::array<T, SIZE> rb_{0};  ///< the ring buffer
  int tail_{0};                ///< position of the next element to read
  int head_{0};                ///< position of the next element to write
  size_t count_{0};            ///< count number of element in the buffer
};

class WK2168Component;  // forward declaration
class WK2168ComponentI2C;
class WK2168ComponentSPI;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief This helper class creates Register objects that act as proxies to access WK2168 register
/// @details This is a pure virtual (interface) class that provides all the necessary access to registers while hiding
/// the actual implementation. This allow the wk2168 component to accesses the registers independently of the actual
/// bus. The derived classes will actually performs the specific bus operations dependant of the bus used.
/// @n typical usage of WK2168Reg:
/// @code
///   WK2168Reg reg_1 {&WK2168Component, ADDR_REGISTER, CHANNEL_NUM, FIFO}  // declaration
///   reg_1 |= 0x01;    // set bit 0 of the wk2168 register
///   reg_1 &= ~0x01;   // reset bit 0 of the wk2168 register
///   reg_1 = 10;       // Set the value of wk2168 register
///   uint val = reg_1; // get the value of wk2168 register
/// @endcode
class WK2168Reg {
 public:
  /// @brief WK2168Reg constructor.
  /// @param parent our creator
  /// @param reg address of the i2c register
  /// @param channel the channel of this register
  /// @param fifo 0 for register transfer, 1 for fifo transfer
  WK2168Reg(WK2168Component *const comp, uint8_t reg, uint8_t channel)
      : comp_(comp), register_(reg), channel_(channel) {}
  virtual ~WK2168Reg() {}

  /// @brief overloads the = operator. This is used to set a value into the wk2168 register
  /// @param value to be set
  /// @return this object
  WK2168Reg &operator=(uint8_t value);

  /// @brief overloads the compound &= operator. This is often used to reset bits in the wk2168 register
  /// @param value performs an & operation with value and store the result
  /// @return this object
  WK2168Reg &operator&=(uint8_t value);

  /// @brief overloads the compound |= operator. This is often used to set bits in the wk2168 register
  /// @param value performs an | operation with value and store the result
  /// @return this object
  WK2168Reg &operator|=(uint8_t value);

  /// @brief cast operator that returns the content of the wk2168 register
  operator uint8_t() const { return read_reg(); }

  /// @brief reads the wk2168 register
  /// @return the value of the wk2168 register
  virtual uint8_t read_reg() const = 0;

  /// @brief writes the wk2168 register
  /// @param value to write in the register
  virtual void write_reg(uint8_t value) = 0;

  /// @brief read an array of bytes from the receiver fifo
  /// @param data pointer to data buffer
  /// @param length number of bytes to read
  virtual void read_fifo(uint8_t *data, size_t length) const = 0;

  /// @brief write an array of bytes to the transmitter fifo
  /// @param data pointer to data buffer
  /// @param length number of bytes to write
  virtual void write_fifo(uint8_t *data, size_t length) = 0;

 protected:
  WK2168Component *const comp_;  ///< pointer to our parent (aggregation)
  uint8_t register_;             ///< address of the register
  uint8_t channel_;              ///< channel for this register
};

class WK2168Channel;  // forward declaration

////////////////////////////////////////////////////////////////////////////////////
/// @brief The WK2168Component class stores the information global to the WK2168 component
/// and provides methods to set/access this information.
/// @details For more information please refer to @ref WK2168Component_
////////////////////////////////////////////////////////////////////////////////////
class WK2168Component : public Component {
 public:
  /// @brief virtual destructor
  virtual ~WK2168Component() {}

  /// @brief store crystal frequency
  /// @param crystal frequency
  void set_crystal(uint32_t crystal) { this->crystal_ = crystal; }

  /// @brief store if the component is in test mode
  /// @param test_mode 0=normal mode any other values mean component in test mode
  void set_test_mode(int test_mode) { this->test_mode_ = test_mode; }

  /// @brief store the name for the component
  /// @param name the name as defined by the python code generator
  void set_name(std::string name) { this->name_ = std::move(name); }

  /// @brief Get the name of the component
  /// @return the name
  const char *get_name() { return this->name_.c_str(); }

  /// @brief override the Component loop()
  void loop() override;

  /// @brief Get the priority of the component
  /// @return the priority
  /// @details The priority is set  below setup_priority::BUS because we use
  /// the i2c bus (which has a priority of BUS) to communicate and the WK2168
  /// therefore it is seen by our client almost as if it was a bus.
  float get_setup_priority() const override { return setup_priority::BUS - 0.1F; }

  bool page1() { return page1_; }

 protected:
  friend class WK2168Channel;
  friend class WK2168Reg;

  /// @brief Factory method to create a Register object
  /// @param reg address of the register
  /// @param channel channel associated with this register
  /// @return a reference to WK2168Reg
  virtual WK2168Reg &reg(uint8_t reg, uint8_t channel) = 0;

  uint32_t crystal_;                         ///< crystal value;
  int test_mode_;                            ///< test mode value (0 -> no tests)
  bool page1_{false};                        ///< set to true when in "page1 mode"
  std::vector<WK2168Channel *> children_{};  ///< the list of WK2168Channel UART children
  std::string name_;                         ///< name of entity
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief The WK2168Channel class is used to implement all the virtual methods of the ESPHome uart::UARTComponent
/// class.
/// @details For more information see @ref WK2168Channel_
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class WK2168Channel : public uart::UARTComponent {
 public:
  /// @brief We belong to the parent WK2168Component
  /// @param parent pointer to the component we belongs to
  void set_parent(WK2168Component *parent) {
    this->parent_ = parent;
    this->parent_->children_.push_back(this);  // add ourself to the list (vector)
  }

  /// @brief Sets the channel number
  /// @param channel number
  void set_channel(uint8_t channel) { this->channel_ = channel; }

  /// @brief The name as generated by the Python code generator
  /// @param name of the channel
  void set_channel_name(std::string name) { this->name_ = std::move(name); }

  /// @brief Get the channel name
  /// @return the name
  const char *get_channel_name() { return this->name_.c_str(); }

  /// @brief Setup the channel
  void setup_channel();

  /// @brief dump channel information
  void dump_channel();

  //
  // we implement the virtual class from UARTComponent
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
  /// @return the number of bytes in the receiver fifo
  int available() override;

  /// @brief Flush the output fifo.
  /// @details If we refer to Serial.flush() in Arduino it says: ** Waits for the transmission of outgoing serial data
  /// to complete. (Prior to Arduino 1.0, this the method was removing any buffered incoming serial data.). ** Therefore
  /// we wait until all bytes are gone with a timeout of 100 ms
  void flush() override;

 protected:
  friend class WK2168Component;
  friend class WK2168ComponentI2C;
  friend class WK2168ComponentSPI;

  /// @brief this cannot happen with external uart therefore we do nothing
  void check_logger_conflict() override {}

  /// @brief Factory method to create a Register object for accessing a register on current channel
  /// @param reg address of the register
  /// @return a reference to WK2168Reg
  WK2168Reg &reg_(uint8_t reg) { return this->parent_->reg(reg, channel_); }

  /// @brief reset the wk2168 internal FIFO
  void reset_fifo_();

  /// @brief set the line parameters
  void set_line_param_();

  /// @brief set the baud rate
  void set_baudrate_();

  /// @brief Returns the number of bytes in the receive fifo
  /// @return the number of bytes in the fifo
  size_t rx_in_fifo_();

  /// @brief Returns the number of bytes in the transmit fifo
  /// @return the number of bytes in the fifo
  size_t tx_in_fifo_();

  /// @brief test if transmit buffer is not empty in the status register (optimization)
  /// @return true if not empty
  bool tx_fifo_is_not_empty_();

  /// @brief transfer bytes from the wk2168 internal FIFO to the buffer (if any)
  /// @return number of bytes transferred
  size_t xfer_fifo_to_buffer_();

#ifdef TEST_COMPONENT
  /// @defgroup test_ Test component information
  /// This group contains information about the test of the component
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
  RingBuffer<uint8_t, RING_BUFFER_SIZE> receive_buffer_;
  WK2168Component *parent_;  ///< our WK2168component parent
  uint8_t channel_;          ///< our Channel number
  uint8_t data_;             ///< a one byte buffer for register read storage
  std::string name_;         ///< name of the entity
};

}  // namespace wk2168
}  // namespace esphome
