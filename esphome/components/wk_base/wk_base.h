/// @file wk_base.h
/// @author DrCoolZic
/// @brief  wk_base classes declaration
/// @details The classes declared in this file can be used by a family
/// of Weikai UART bridge components.
/// As of today used by wk2132_spi, wk2132_i2c, wk2168_spi, wk2168_i2c

#pragma once
#include <bitset>
#include <memory>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "wk_reg_def.h"

#if defined(USE_ESP32_FRAMEWORK_ARDUINO) && defined(I2C_COMPILE)
#include "Wire.h"
#endif

/// When the TEST_COMPONENT flag is defined we include some auto-test methods. Used to test the software during
/// development but can also be used in situ to test if the component is working correctly. In production we do
/// not set it by default but you can add it by using the following lines in you configuration file:
/// @code
/// esphome:
///   name: test-wk_base-i2c-arduino
///   platformio_options:
///     build_flags:
///       - -DTEST_COMPONENT
// #define TEST_COMPONENT

namespace esphome {
namespace wk_base {

/// @brief XFER_MAX_SIZE defines the maximum number of bytes allowed during one transfer.
/// - When using the Arduino framework by default the maximum number of bytes that can be transferred is 128 bytes. But
///   this can be changed by defining the macro I2C_BUFFER_LENGTH during compilation. This is done automatically by the
///   __init__.py file when generating the code from the Yaml configuration file.
/// - When using the ESP-IDF Framework the maximum number of bytes allowed during transfer is 256 bytes.
/// @bug At the time of writing (Jan 2024) there is a bug in the Arduino framework in the TwoWire::requestFrom() method.
/// This bug limits the number of bytes we can read to 255. For this reasons we limit the XFER_MAX_SIZE to 255.
/// - When we use an ESP8286 CPU we limit the transfer to 128

#if defined(USE_ESP8266)  // ESP8286
constexpr size_t XFER_MAX_SIZE = 128;

#elif defined(USE_ESP32_FRAMEWORK_ESP_IDF)                     // ESP32 and framework IDF
constexpr size_t XFER_MAX_SIZE = 256;

// ESP32 and framework Arduino
#elif defined(I2C_BUFFER_LENGTH) && (I2C_BUFFER_LENGTH < 256)  // Here we are using an USE_ESP32_FRAMEWORK_ARDUINO
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
/// characters from the device FIFO, is to first check how many bytes were received and then read them all at once.
/// Unfortunately in all the code I have reviewed the characters are read one by one in a while loop by checking if
/// bytes are available then reading the byte until no more byte available. This is pretty inefficient for two reasons:
/// - Fist you need to perform a test for each byte to read
/// - and second you call the read byte method for each character.
/// Assuming you need to read 100 bytes that results into 200 calls. This is to compare to 2 calls (one to find the
/// number of bytes available plus one to read all the bytes) in the best case! If the registers you read are located on
/// the micro-controller this is acceptable (even if it roughly double the process time) because the registers can be
/// accessed fast. But when the registers are located on a remote device accessing them requires several cycles on the
/// slow bus. As it it not possible to fix this problem by asking users to rewrite their code, I have implemented this
/// ring buffer solution that store the bytes received locally.
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

class WKBaseComponent;  // forward declaration
class WKBaseI2C;
class WKBaseSPI;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief Used to create WKBaseRegister objects that act as proxies to access remote register independently of the
/// bus type.
/// @details This is an abstract interface class that provides many operations to access to registers while hiding
/// the actual implementation. This allow to accesses the registers in the WKBase component abstract class independently
/// of the actual bus (I2C, SPI). The derived classes will actually implements the specific bus operations dependant of
/// the bus used.
/// @n typical usage of WKBaseRegister:
/// @code
///   WKBaseRegister reg_1 {&WKBaseComponent, ADDR_REGISTER, CHANNEL_NUM, FIFO}  // declaration
///   reg_1 |= 0x01;    // set bit 0 of the wk_base register
///   reg_1 &= ~0x01;   // reset bit 0 of the wk_base register
///   reg_1 = 10;       // Set the value of wk_base register
///   uint val = reg_1; // get the value of wk_base register
/// @endcode
class WKBaseRegister {
 public:
  /// @brief WKBaseRegister constructor.
  /// @param comp our parent WKBaseComponent
  /// @param reg address of the register
  /// @param channel the channel of this register
  WKBaseRegister(WKBaseComponent *const comp, uint8_t reg, uint8_t channel)
      : comp_(comp), register_(reg), channel_(channel) {}
  virtual ~WKBaseRegister() {}

  /// @brief overloads the = operator. This is used to set a value into the wk_base register
  /// @param value to be set
  /// @return this object
  WKBaseRegister &operator=(uint8_t value);

  /// @brief overloads the compound &= operator. This is often used to reset bits in the wk_base register
  /// @param value performs an & operation with value and store the result
  /// @return this object
  WKBaseRegister &operator&=(uint8_t value);

  /// @brief overloads the compound |= operator. This is often used to set bits in the wk_base register
  /// @param value performs an | operation with value and store the result
  /// @return this object
  WKBaseRegister &operator|=(uint8_t value);

  /// @brief cast operator that returns the content of the wk_base register
  operator uint8_t() const { return read_reg(); }

  /// @brief reads the register
  /// @return the value read from the register
  virtual uint8_t read_reg() const = 0;

  /// @brief writes the register
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
  WKBaseComponent *const comp_;  ///< pointer to our parent (aggregation)
  uint8_t register_;             ///< address of the register
  uint8_t channel_;              ///< channel for this register
};

class WKBaseChannel;  // forward declaration
////////////////////////////////////////////////////////////////////////////////////
/// @brief The WKBaseComponent class stores the information global to the WK component
/// and provides methods to set/access this information. It is also the container of
/// the WKBaseChannel children objects. This class is derived from esphome::Component
/// class.
////////////////////////////////////////////////////////////////////////////////////
class WKBaseComponent : public Component {
 public:
  /// @brief virtual destructor
  virtual ~WKBaseComponent() {}

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

  bool page1() { return page1_; }

  /// @brief Factory method to create a Register object
  /// @param reg address of the register
  /// @param channel channel associated with this register
  /// @return a reference to WKBaseRegister
  virtual WKBaseRegister &reg(uint8_t reg, uint8_t channel) = 0;

 protected:
  friend class WKBaseChannel;

  /// @brief Get the priority of the component
  /// @return the priority
  /// @details The priority is set  below setup_priority::BUS because we use
  /// the i2c bus (which has a priority of BUS) to communicate and the WK2168
  /// therefore it is seen by our client almost as if it was a bus.
  float get_setup_priority() const override { return setup_priority::BUS - 0.1F; }

  uint32_t crystal_;                         ///< crystal value;
  int test_mode_;                            ///< test mode value (0 -> no tests)
  bool page1_{false};                        ///< set to true when in "page1 mode"
  std::vector<WKBaseChannel *> children_{};  ///< the list of WKBaseChannel UART children
  std::string name_;                         ///< name of entity
};

///////////////////////////////////////////////////////////////////////////////////////////////////
/// @brief The WKBaseChannel class is used to implement all the virtual methods of the ESPHome
/// uart::UARTComponent virtual class. This class is common to the different members of the Weikai components family
/// and therefore avoid code duplication. Currently this is used for the wk2132_i2c, wk2132_spi, wk2168_i2c, wk2168_spi
/// components.
///////////////////////////////////////////////////////////////////////////////////////////////////
class WKBaseChannel : public uart::UARTComponent {
 public:
  /// @brief We belongs to this WKBaseComponent
  /// @param parent pointer to the component we belongs to
  void set_parent(WKBaseComponent *parent) {
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
  void virtual setup_channel();

  /// @brief dump channel information
  void virtual dump_channel();

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
  int available() override;

  /// @brief Flush the output fifo.
  /// @details If we refer to Serial.flush() in Arduino it says: ** Waits for the transmission of outgoing serial data
  /// to complete. (Prior to Arduino 1.0, this the method was removing any buffered incoming serial data.). ** Therefore
  /// we wait until all bytes are gone with a timeout of 100 ms
  void flush() override;

 protected:
  friend class WKBaseComponent;

  /// @brief this cannot happen with external uart therefore we do nothing
  void check_logger_conflict() override {}

  /// @brief Factory method to create a WKBaseRegister proxy object
  /// @param reg address of the register
  /// @return a reference to WKBaseRegister
  WKBaseRegister &reg_(uint8_t reg) { return this->parent_->reg(reg, channel_); }

  /// @brief reset the wk_base internal FIFO
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

  /// @brief transfer bytes from the wk_base internal FIFO to the buffer (if any)
  /// @return number of bytes transferred
  size_t xfer_fifo_to_buffer_();

  /// @brief check if channel is alive
  /// @return true if OK
  bool virtual check_channel_down();

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
  WKBaseComponent *parent_;  ///< our WK2168component parent
  uint8_t channel_;          ///< our Channel number
  uint8_t data_;             ///< a one byte buffer for register read storage
  std::string name_;         ///< name of the entity
};

}  // namespace wk_base
}  // namespace esphome
