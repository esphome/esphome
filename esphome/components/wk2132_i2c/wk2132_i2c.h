/// @file wk2132_i2c.h
/// @author DrCoolZic
/// @brief  wk2132 classes declaration

#pragma once
#include <bitset>
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/uart/uart.h"

/// when TEST_COMPONENT is define we include some auto-test functions
/// This has been used to test the software during development but it
/// can also be used to test if the component is working correctly.
#define TEST_COMPONENT

namespace esphome {
/// @brief The wk2132_i2c namespace
namespace wk2132_i2c {

/// @brief the max number of bytes we allow for transfer calls.
/// By default I²C bus allow a maximum transfer of 128 bytes
/// but this can be changed by defining I2C_BUFFER_LENGTH.
/// @bug However there is a bug in Arduino framework that limit the
/// maximum value to 255. So until fixed we follow this limit
/// @bug There is also a bug in i2c classes. Here we assume this
/// bug is fixed
#if (I2C_BUFFER_LENGTH < 256) && defined(USE_ESP32_FRAMEWORK_ARDUINO)
constexpr size_t XFER_MAX_SIZE = I2C_BUFFER_LENGTH;
#else  // until bug fixed in framework we limit size to 255
constexpr size_t XFER_MAX_SIZE = 255;
#endif

/// @brief size of the internal wk2132 FIFO
constexpr size_t FIFO_SIZE = 256;

/// @brief size of the ring buffer
/// @details We set the ring buffer to the same size as the XFER_MAX_SIZE
constexpr size_t RING_BUFFER_SIZE = XFER_MAX_SIZE;
///////////////////////////////////////////////////////////////////////////////
/// @brief This is an helper class that provides a simple ring buffers
/// that works as a FIFO
///
/// This ring buffer is used to buffer the exchanges between the receiver
/// HW fifo and the client. Usually to read characters from the line you first
/// check how many bytes were received and then you read them all at once.
/// But if you try to read the bytes one by one there is a potential problem.
/// When the registers are located on the chip everything is fine but with a
/// device like the wk2132 these registers are remote and therefore accessing
/// them requires several transactions on the I²C bus which is relatively slow.
/// As already described the solution is for the client to check the number of
/// bytes available and read them all at once using the read_array() method.
/// Unfortunately most client software I have reviewed are reading characters
/// one at a time in a while loop which is extremely inefficient for remote
/// registers. Therefore the solution I have chosen to elevate this problem
/// is to store received bytes locally in a ring buffer as soon as we read one.
/// With this solution the bytes are stored locally and therefore accessible
/// very quickly when requested one by one without the need of extra bus transitions.
///////////////////////////////////////////////////////////////////////////////
template<typename T, size_t SIZE> class RingBuffer {
 public:
  /// @brief pushes an item at the tail of the fifo
  /// @param item item to push
  /// @return true if item has been pushed, false il item was not pushed (buffer full)
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
  /// @return true if item has been retrieved, false il no item was found (buffer empty)
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
  /// @return true if item has been retrieved, false il no item was found (buffer empty)
  bool peek(T &item) {
    if (is_empty())
      return false;
    item = this->rb_[this->tail_];
    return true;
  }

  /// @brief test is the Ring Buffer is empty ?
  /// @return true if empty
  bool is_empty() { return (this->count_ == 0); }

  /// @brief test is the ring buffer is full ?
  /// @return true if full
  bool is_full() { return (this->count_ == SIZE); }

  /// @brief return the number of item in the ring buffer
  /// @return the number of items
  size_t count() { return this->count_; }

  /// @brief returns the number of free positions in the buffer
  /// @return how many items can be added
  size_t free() { return SIZE - this->count_; }

  /// @brief clear the buffer content
  void clear() { this->head_ = this->tail_ = this->count_ = 0; }

 private:
  std::array<T, SIZE> rb_{0};  // the ring buffer
  int head_{0};                // points to the next element to write
  int tail_{0};                // points to the next element to read
  size_t count_{0};            // count number of element in the buffer
};

/// @defgroup wk2132_gr_ WK2132 Global Registers
/// This topic groups all **Global Registers**: these registers are global to the
/// the WK2132 chip (i.e. independent of the UART channel used)
/// @note only registers and parameters used have been documented
/// @{

/// @brief Global Control Register
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  -------------------------------------------------------------------------
///  |   M0   |   M1   |                RSV                |  C2EN  |  C1EN  |
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_GENA = 0x00;
/// @brief Channel 2 enable clock (0: disable, 1: enable)
constexpr uint8_t GENA_C2EN = 1 << 1;
/// @brief Channel 1 enable clock (0: disable, 1: enable)
constexpr uint8_t GENA_C1EN = 1 << 0;

/// @brief Global Reset Register
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  -------------------------------------------------------------------------
///  |       RSV       | C2SLEEP| C1SLEEP|       RSV       |  C2RST |  C1RST |
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_GRST = 0x01;
/// @brief Channel 2 soft reset (0: not reset, 1: reset)
constexpr uint8_t GRST_C2RST = 1 << 1;
/// @brief Channel 1 soft reset (0: not reset, 1: reset)
constexpr uint8_t GRST_C1RST = 1 << 0;

/// @brief Global Master channel control register (not used)
constexpr uint8_t REG_WK2132_GMUT = 0x02;

/// @brief Global Page register
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                             RSV                              |  PAGE  |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_SPAGE = 0x03;

/// Global interrupt register (not used)
constexpr uint8_t REG_WK2132_GIR = 0x10;

/// Global interrupt flag register (not used)
constexpr uint8_t REG_WK2132_GIFR = 0x11;

/// @}
/// @defgroup wk2132_cr_ WK2132 Channel Registers
/// This topic groups all the **Channel Registers**: these registers are specific
/// to the a specific channel i.e. each channel has its own set of registers
/// @note only registers and parameters used have been documented
/// @{

/// @defgroup cr_p0 Channel registers for SPAGE=0
/// The channel registers are further splitted into two groups.
/// This first group is defined when the Global register REG_WK2132_SPAGE is 0
/// @{

/// @brief Serial Control Register
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  -------------------------------------------------------------------------
///  |                     RSV                    | SLEEPEN|  TXEN  |  RXEN  |
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_SCR = 0x04;
/// @brief transmission control (0: enable, 1: disable)
constexpr uint8_t SCR_TXEN = 1 << 1;
/// @brief receiving control (0: enable, 1: disable)
constexpr uint8_t SCR_RXEN = 1 << 0;

/// @brief Line Configuration Register:
/// @details @code
///  -------------------------------------------------------------------------
///  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
///  -------------------------------------------------------------------------
///  |        RSV      |  BREAK |  IREN  |  PAEN  |      PARITY     |  STPL  |
///  -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_LCR = 0x05;
/// @brief Parity enable (0: no check, 1: check)
constexpr uint8_t LCR_PAEN = 1 << 3;
/// @brief Parity force 0
constexpr uint8_t LCR_PAR_0 = 00 << 1;
/// @brief Parity odd
constexpr uint8_t LCR_PAR_ODD = 01 << 1;
/// @brief Parity even
constexpr uint8_t LCR_PAR_EVEN = 2 << 1;
/// @brief Parity force 1
constexpr uint8_t LCR_PAR_1 = 3 << 1;
/// @brief Stop length (0: 1 bit, 1: 2 bits)
constexpr uint8_t LCR_STPL = 1 << 0;

/// @brief FIFO Control Register
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |      TFTRIG     |      RFTRIG     |  TFEN  |  RFEN  |  TFRST |  RFRST |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_FCR = 0x06;
/// @brief Transmitter FIFO enable
constexpr uint8_t FCR_TFEN = 1 << 3;
/// @brief Receiver FIFO enable
constexpr uint8_t FCR_RFEN = 1 << 2;
/// @brief Transmitter FIFO reset
constexpr uint8_t FCR_TFRST = 1 << 3;
/// @brief Receiver FIFO reset
constexpr uint8_t FCR_RFRST = 1 << 3;

/// @brief Serial Interrupt Enable Register (not used)
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |FERR_IEN|            RSV           |TEMPTY_E|TTRIG_IE|RXOVT_EN|RFTRIG_E|
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_SIER = 0x07;

/// @brief Serial Interrupt Flag Register (not used)
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |      TFTRIG     |      RFTRIG     |  TFEN  |  RFEN  |  TFRST |  RFRST |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_SIFR = 0x08;

/// @brief Transmitter FIFO Count
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                  NUMBER OF DATA IN TRANSMITTER FIFO                   |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_TFCNT = 0x09;

/// @brief Receiver FIFO count
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                    NUMBER OF DATA IN RECEIVER FIFO                    |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_RFCNT = 0x0A;

/// @brief FIFO Status Register
/// @details @code
/// * -------------------------------------------------------------------------
/// * |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// * -------------------------------------------------------------------------
/// * |  RFOE  |  RFLB  |  RFFE  |  RFPE   | RFEMPT | TFEMPT | TFFULL |  TBUSY |
/// * -------------------------------------------------------------------------
/// @endcode
/// @warning The received buffer can hold 256 bytes. However, as the RFCNT reg
/// is 8 bits, in this case the value 256 is reported as 0 ! Therefore the RFCNT
/// count can be zero when there is 0 byte **or** 256 bytes in the buffer. If we
/// have RXDAT = 1 and RFCNT = 0 it should be interpreted as 256 bytes in the FIFO.
/// @note Note that in case of overflow the RFOE goes to one **but** as soon as you read
/// the FSR this bit is cleared. Therefore Overflow can be read only once even if
/// still in overflow.
/// @n The same remark applies to the transmit buffer but here we have to check the
/// TFULL flag. So if TFULL is set and TFCNT is 0 this should be interpreted as 256
constexpr uint8_t REG_WK2132_FSR = 0x0B;
/// @brief Receiver FIFO Overflow Error (0: no OE, 1: OE)
constexpr uint8_t FSR_RFOE = 1 << 7;
/// @brief Receiver FIFO Line Break (0: no LB, 1: LB)
constexpr uint8_t FSR_RFLB = 1 << 6;
/// @brief Receiver FIFO Frame Error (0: no FE, 1: FE)
constexpr uint8_t FSR_RFFE = 1 << 5;
/// @brief Receiver Parity Error (0: no PE, 1: PE)
constexpr uint8_t FSR_RFPE = 1 << 4;
/// @brief Receiver FIFO empty (0: empty, 1: not empty)
constexpr uint8_t FSR_RFEMPT = 1 << 3;
/// @brief Transmitter FIFO Empty (0: empty, 1: not empty)
constexpr uint8_t FSR_TFEMPT = 1 << 2;
/// @brief Transmitter FIFO full (0: not full, 1: full)
constexpr uint8_t FSR_TFFULL = 1 << 1;
/// @brief Transmitter busy (0 transmitter empty, 1: transmitter busy sending)
constexpr uint8_t FSR_TBUSY = 1 << 0;

/// @brief Line Status Register (not used - using FIFO)
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                 RSV               |  OVLE  |  BRKE  | FRAMEE |  PAR_E |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_LSR = 0x0C;

/// @brief FIFO Data Register (not used - does not seems to work)
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                        DATA_READ or DATA_TO_WRITE                     |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_FDAT = 0x0D;

/// @}
/// @defgroup cr_p1 Channel registers for SPAGE=1
/// The channel registers are further splitted into two groups.
/// This second group is defined when the Global register REG_WK2132_SPAGE is 1
/// @{

/// @brief Baud rate configuration register: high byte
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                      High byte of the baud rate                       |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_BRH = 0x04;

/// @brief Baud rate configuration register: low byte
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                       Low byte of the baud rate                       |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_BRL = 0x05;

/// @brief Baud rate configuration register decimal part
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                      decimal part of the baud rate                    |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_BRD = 0x06;

/// @brief Receive FIFO Interrupt trigger configuration (not used)
/// @details @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                      Receive FIFO contact control                     |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_RFI = 0x07;

/// @brief Transmit FIFO interrupt trigger configuration (not used)
/// @code
/// -------------------------------------------------------------------------
/// |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
/// -------------------------------------------------------------------------
/// |                       Send FIFO contact control                       |
/// -------------------------------------------------------------------------
/// @endcode
constexpr uint8_t REG_WK2132_TFI = 0x08;

/// @}
/// @}

class WK2132Component;  // forward declaration

/// @brief This class defines *proxy registers* that act as proxies to WK2132 internal register
/// @details This class is equivalent to thr i2c::I2CRegister class.
/// The reason this class exists is because the WK2132 uses an unusual addressing mechanism.
/// On a *standard* I2C device a **logical_register_address** is combined with the channel number
/// to give an **i2c_register_address** and all accesses are done at the same device address on the bus.
/// On the WK2132 i2c_register_address = logical_register_address and what is changing is the
/// device address on the bus. Therefore we have a base_address for global register, a different
/// addresses for channel 1 and 2 register, and yet different addresses for FIFO access.
/// For that reason on top of saving the register address we also nedd to save the address
/// of the device to use on the i2c bus to access this register.
/// @n typical usage:
/// @code
/// WK2132Register reg_1 = this->register(ADDR_REGISTER_1); // declare
/// reg_1 |= 0x01; // set bit
/// reg_1 &= ~0x01; // reset bit
/// reg_1 = 10; // Set value
/// uint val = reg_1.get(); // get value
/// @endcode
class WK2132Register {
 public:
  /// @brief overloads the = operator. This is used to set a value in the register
  /// @param value to be set
  /// @return this object
  WK2132Register &operator=(uint8_t value);

  /// @brief overloads the compound &= operator. This is often used to reset bits in the register
  /// @param value performs an & operation with value and store the result
  /// @return this object
  WK2132Register &operator&=(uint8_t value);

  /// @brief overloads the compound |= operator. This is often used to set bits in the register
  /// @param value performs an | operation with value and store the result
  /// @return this object
  WK2132Register &operator|=(uint8_t value);

  /// @brief cast operator
  operator uint8_t() const { return get(); }

  /// @brief returns the register value
  /// @return the value
  uint8_t get() const;

  /// @brief sets the register value
  /// @param value to set
  void set(uint8_t value);

 protected:
  friend class WK2132Component;
  friend class WK2132Channel;

  /// @brief protected ctor. Only friends can create an I2CRegister
  /// @param parent our parent
  /// @param a_register address of the i2c register
  WK2132Register(WK2132Component *parent, uint8_t reg, uint8_t channel)
      : parent_(parent), register_(reg), channel_(channel) {}

  WK2132Component *parent_;  ///< parent we belongs to
  uint8_t register_;         ///< the address of the register
  uint8_t channel_;          ///< the channel for the register
};

class WK2132Channel;  // forward declaration

///////////////////////////////////////////////////////////////////////////////
/// @brief The WK2132 I²C component class
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
  /// @brief store crystal frequency
  /// @param crystal frequency
  void set_crystal(uint32_t crystal) { this->crystal_ = crystal; }

  /// @brief store the component in test mode only use for debug purpose
  /// @param test_mode 0=normal other means component in test mode
  void set_test_mode(int test_mode) { this->test_mode_ = test_mode; }

  /// @brief store the name for the component
  /// @param name the name as defined by the python code generator
  void set_name(std::string name) { this->name_ = std::move(name); }

  /// @brief Get the name of the component
  /// @return the name
  const char *get_name() { return this->name_.c_str(); }

  /// @brief call the WK2132Register ctor
  /// @param a_register address of the register
  /// @return an WK2132Register proxy to the register at a_address
  WK2132Register component_reg(uint8_t a_register) { return {this, a_register, 0}; }

  //
  //  override virtual Component methods
  //

  void setup() override;
  void dump_config() override;
  void loop() override;

  /// @brief Set the priority of the component
  /// @return the priority
  ///
  /// The priority is set just a bit  below setup_priority::BUS because we use
  /// the i2c bus (which has a priority of BUS) to communicate and the WK2132
  /// will be used by our client as if ir was a bus.
  float get_setup_priority() const override { return setup_priority::BUS - 0.1F; }

 protected:
  friend class WK2132Channel;
  friend class WK2132Register;

  // /// @brief convert the register number into a string easier to understand
  // /// @param reg register value
  // /// @return name of the register
  // const char *reg_to_str_(int reg);  // for debug

  uint32_t crystal_;                         ///< crystal value;
  uint8_t base_address_;                     ///< base address of I2C device
  int test_mode_;                            ///< test mode value (0 -> no tests)
  bool page1_{false};                        ///< set to true when in "page1 mode"
  std::vector<WK2132Channel *> children_{};  ///< the list of WK2132Channel UART children
  std::string name_;                         ///< name of entity
};

////////////////// /////////////////////////////////////////////////////////////
/// @brief The UART channel class of a WK2132 I²C component.
///
/// This class derives from the virtual @ref uart::UARTComponent class.
///
/// Unfortunately I have not found **any documentation** about the
/// uart::UARTDevice and uart::UARTComponent classes in @ref ESPHome.\n
/// However it seems that both of them are based on equivalent classes in
/// Arduino library.\n
///
/// Most of the interfaces provided by the Arduino Serial library are **poorly
/// defined** and it seems that the API has even \b changed over time!\n
/// The esphome::uart::UARTDevice class directly relates to the **Serial Class**
/// in Arduino and that derives from **Stream class**.\n
/// For compatibility reason (?) many helper methods are made available in
/// ESPHome to read and write. Unfortunately in many cases these helpers
/// are missing the critical status information and therefore are even
/// more badly defined and more unsafe to use...\n
///////////////////////////////////////////////////////////////////////////////
class WK2132Channel : public uart::UARTComponent {
 public:
  /// @brief We belong to the parent WK2132Component
  /// @param parent pointer to the component we belongs to
  void set_parent(WK2132Component *parent) {
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

  /// @brief call the WK2132Register ctor
  /// @param a_register address of the register
  /// @return an WK2132Register proxy to the register at a_address
  WK2132Register channel_reg(uint8_t a_register) { return {this->parent_, a_register, this->channel_}; }

  //
  // we implement the virtual class from UARTComponent
  //

  /// @brief Writes a specified number of bytes to a serial port
  /// @param buffer pointer to the buffer
  /// @param length number of bytes to write
  ///
  /// This method sends 'length' characters from the buffer to the serial line.
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
  ///   write_array(&buffer, length);
  ///   flush();
  ///   // ...
  /// @endcode
  void write_array(const uint8_t *buffer, size_t length) override;

  /// @brief Reads a specified number of bytes from a serial port
  /// @param buffer buffer to store the bytes
  /// @param length number of bytes to read
  /// @return true if succeed, false otherwise
  ///
  /// Typical usage:
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
  ///
  /// This method returns the next byte from receiving buffer without
  /// removing it from the internal fifo. It returns true if a character
  /// is available and has been read, false otherwise.\n
  bool peek_byte(uint8_t *buffer) override;

  /// @brief Returns the number of bytes in the receive buffer
  /// @return the number of bytes in the receiver fifo
  int available() override;

  /// @brief Flush the output fifo.
  ///
  /// If we refer to Serial.flush() in Arduino it says: ** Waits for the transmission
  /// of outgoing serial data to complete. (Prior to Arduino 1.0, this the method was
  /// removing any buffered incoming serial data.). **
  /// Therefore we wait until all bytes are gone with a timeout of 100 ms
  void flush() override;

 protected:
  friend class WK2132Component;

  /// @brief this cannot happen with external uart therefore we do nothing
  void check_logger_conflict() override {}

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

  /// @brief reset the wk2132 internal FIFO
  void reset_fifo_();

  /// @brief set the line parameters
  void set_line_param_();

  /// @brief set the baud rate
  void set_baudrate_();

  /// @brief Setup the channel
  void setup_channel_();

  /// @brief Returns the number of bytes in the receive fifo
  /// @return the number of bytes in the fifo
  size_t rx_in_fifo_();

  /// @brief Returns the number of bytes in the transmit fifo
  /// @return the number of bytes in the fifo
  size_t tx_in_fifo_();

  /// @brief test if transmit buffer is not empty in the status register
  /// @return true if not empty
  bool tx_fifo_is_not_empty_();

  /// @brief transfer bytes from the wk2132 internal FIFO to the buffer (if any)
  /// @return number of bytes transferred
  size_t xfer_fifo_to_buffer_();

  /// @brief the buffer where we store temporarily the bytes received
  RingBuffer<uint8_t, RING_BUFFER_SIZE> receive_buffer_;
  WK2132Component *parent_;  ///< Our WK2132component parent
  uint8_t channel_;          ///< Our Channel number
  uint8_t data_;             ///< one byte buffer for register read storage
  std::string name_;         ///< name of the entity
};

}  // namespace wk2132_i2c
}  // namespace esphome
