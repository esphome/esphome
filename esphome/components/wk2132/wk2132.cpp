/// @file wk2132.cpp
/// @author DrCoolzic
/// @brief wk2132 classes implementation

#include "wk2132.h"

namespace esphome {
namespace wk2132 {

/*! @mainpage WK2132 source code documentation
 This documentation provides information about the implementation of the WK2132Component in ESPHome.
 The WK2132Component utilizes two primary classes: the WK2132Component class, the WK2132Channel class,
 along with the RingBuffer and WK2132Register helper classes. Below, you'll find short descriptions
 of these three classes.
 @n Before delving into these classes, let's explore the interactions of these classes with the other ESPHome classes
 through the UML class diagram presented below:
 <img src="WK2132 Class diagram.png" align="left" width="1024">
 <div style="clear: both"></div>

  @section RingBuffer_ The RingBuffer template class
The RingBuffer template class has it names implies implement a simple ring buffer helper class. This straightforward
container implements FIFO functionality, enabling bytes to be pushed into one side and popped from the other in the
order of entry. Implementation is classic and therefore not described in any details.

  @section WK2132Register_ The WK2132Register class
 The WK2132Register helper class creates objects that act as proxies to WK2132 register.
 @details This is a pure virtual (interface) class that provides all the necessary access to registers while hiding
 the actual implementation. In the case of the wk2132 the access to the registers can be made through an I²C bus in
 the case of the wk2132_i2c component our through an SPI bus in the case of the wk2132_spi component. Derived classes
 will actually performs the specific bus operations.
 @n typical usage of WK2132Register:
 @code
   WK2132Register reg_1 {&WK2132Component_1, ADDR_REGISTER_1, CHANNEL_NUM}  // declaration
   reg_1 |= 0x01; // set bit 0 of the wk2132 register
   reg_1 &= ~0x01; // reset bit 0 of the wk2132 register
   reg_1 = 10; // Set the value of wk2132 register
   uint val = reg_1; // get the value of wk2132 register
 @endcode

 @section WK2132Component_ The WK2132Component class
The WK2132Component class stores the information global to the WK2132 component and provides methods to set/access
this information. It also serves as a container for WK2132Channel instances. This is done by maintaining an array of
references these WK2132Channel instances. This class derives from two ESPHome classes. The esphome::Component class and
the i2c::I2CDevice class. This class override the esphome::Component::setup() and the esphome::Component::loop() methods
called respectively to initialize the component and to perform task on a regular basis. The loop() method is used to
facilitate the seamless transfer of accumulated bytes from the receive FIFO into the ring buffer. This process ensures
quick access to the stored bytes, enhancing the overall efficiency of the component. It uses the read and write methods
from the i2c::I2CDevice class to communicate through the i2c::I2CBus class with the WK2132 device.

 @section WK2132Channel_ The WK2132Channel class
The WK2132Channel class is used to implement all the virtual methods of the ESPHome uart::UARTComponent class. An
individual instance of this class is created for each UART channel. It has a link back to the WK2132Component object it
belongs to as well as channel information. This class derives from the uart::UARTComponent class. It implements all
virtual methods from this class. It collaborates through an aggregation with WK2132Component. This implies that
WK2132Component acts as a container, housing one or two WK2132Channel instances. It's important to note that this is a
weak dependency, allowing WK2132Channel instances to persist even if the associated WK2132Component is destroyed (an
eventuality that never occurs in ESPHome). Furthermore, the WK2132Channel class derives from the ESPHome
uart::UARTComponent class, it also has an association relationship with the RingBuffer and WK2132Register helper
classes. Consequently, when a WK2132Channel instance is destroyed, the associated RingBuffer instance is also destroyed.

 @note Regrettably, there is a notable absence of documentation concerning the uart::UARTDevice and uart::UARTComponent
classes within the ESPHome reference. However, it appears that both classes draw inspiration from equivalent Arduino
classes. This lack of documentation poses a challenge, as there is no comprehensive resource for understanding these
ESPHome classes. The situation is somewhat mitigated by the fact that we can reference the Arduino library for similar
methods. Yet, this approach is not without its pitfalls, given the poorly defined interfaces in the Arduino Serial
library, which have **undergone many changes over time**.
 @n It's worth noting that ESPHome provides several helper methods, possibly for compatibility reasons. However, it's
crucial to acknowledge that these helpers often lack critical status information. Consequently, utilizing them may pose
increased risks compared to employing the original methods. Careful consideration and caution are advised when working
with these helper methods in @ref ESPHome.

Lets first describe the optimum usage of the read/write methods to get the best performance.
 @n Usually UART are used to communicate with remote devices following this kind of protocol:
-# the initiator (the micro-controller) asks the remote device for information by sending a specific command/control
frame of a few dozen bytes.
-# in response the remote send back the information requested by the initiator as a frame with often several hundred
bytes
-# the requestor process the information received and prepare the frame for the next request.
-# and these steps repeat forever

With such protocol and the available uart::UARTComponent methods, the optimum sequence of calls would look like this:
 @code
constexpr size_t CMD_SIZE = 23;
constexpr size_t BUF_SIZE
uint8_t command_buffer[CMD_SIZE];
uint8_t receive_buffer[BUF_SIZE]
auto uart = new uart::UARTDevice();
// infinite loop to do requests and get responses
while (true) {
  // prepare the command buffer
  // ...
  uart->flush();  // wait until the transmit FIFO is empty
  uart->write_array(command_buffer, CMD_SIZE); // we send all bytes
  int available = 0;
  // loop until all bytes processed
  while ((available = uart->available())) {
    uart->read_array(receive_buffer, available);
    // here we loop for all received bytes
    for (for i = 0; i < available; i++) {
      // here we process each byte received
      // ...
    }
  }
}
 @endcode

But unfortunately all the code that I have reviewed is not using this kind of sequence. They actually look more like
this:
  @code
 constexpr size_t CMD_SIZE = 23;
 uint8_t command_buffer[CMD_SIZE];
 auto uart = new uart::UARTDevice();
 // infinite loop to do requests and get responses
 while (true) {
   //
   // prepare the command buffer
   // ...
   uart->write_array(command_buffer, CMD_SIZE);
   uart->flush();  // we get rid of bytes still in UART FIFO just after write!
   while (uart->available()) {
     uint8_t byte = uart->read();
     //
     // here we process each byte received
     // ...
   }
 }
 @endcode

So what is \b wrong about this last code ?
- using uart::UARTComponent::write_array() followed immediately by a uart::UARTComponent::flush() method forces to wait
for all bytes in the fifo to be gone. And this can take a lot of time especially if the line speed is low. With an
optimum code, we reverse the call: we call the uart::UARTComponent::flush() method first and the
uart::UARTComponent::write_array() method after. In most cases the flush() method should return immediately because
there is usually a lot of time spend in processing the received bytes before we do the flush() in the next loop.
- using uart::UARTComponent::available() just to test if received bytes are present and following this test by reading
one byte with uart::UARTComponent::read() in a loop is very inefficient because it causes a lot of calls to the read()
method. If you are using an UART directly located on the micro-processor this is not too bad as the access time to the
registers is fast. However if the UART is located remotely and communicating through an I²C bus then this becomes very
problematic because this will induce a lot of transactions on the bus to access the different registers all this for
reading one byte.
- In an optimum code, we use the uart::UARTComponent::available() method to find out how many bytes are available and we
read them all with one call. Proceeding like this could easily improve the speed by a factor of more than six.

So what can we do to improve the efficiently of the WK2132 component without asking the clients to rewrite their code?
After testing the timings of different strategies, I finally came up with the following solution: I use a ring buffer to
store locally the bytes received in the FIFO. Therefore even if the client is reading bytes one by one the usage of a
local ring buffer **minimize the number of transactions on the bus**. The transfer of the bytes from the FIFO to the
buffer is performed whenever using uart::UARTComponent::read_array() or uart::UARTComponent::available() and it is also
performed in advance each time the esphome::Component::loop() method is executed.
*/

static const char *const TAG = "wk2132";

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
/// Convert std::string to C string
#define I2CS(val) (i2s(val).c_str())

/// @brief measure the time elapsed between two calls
/// @param last_time time of the previous call
/// @return the elapsed time in microseconds
uint32_t elapsed_us(uint32_t &last_time) {
  uint32_t e = micros() - last_time;
  last_time = micros();
  return e;
};

/// @brief Converts the parity enum value to a C string
/// @param parity enum
/// @return the string
const char *p2s(uart::UARTParityOptions parity) {
  using namespace uart;
  switch (parity) {
    case UART_CONFIG_PARITY_NONE:
      return "NONE";
    case UART_CONFIG_PARITY_EVEN:
      return "EVEN";
    case UART_CONFIG_PARITY_ODD:
      return "ODD";
    default:
      return "UNKNOWN";
  }
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Register methods
///////////////////////////////////////////////////////////////////////////////
WK2132Register &WK2132Register::operator=(uint8_t value) {
  set(value);
  return *this;
}

WK2132Register &WK2132Register::operator&=(uint8_t value) {
  value &= get();
  set(value);
  return *this;
}

WK2132Register &WK2132Register::operator|=(uint8_t value) {
  value |= get();
  set(value);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Component methods
///////////////////////////////////////////////////////////////////////////////
void WK2132Component::loop() {
  if (this->component_state_ & (COMPONENT_STATE_MASK != COMPONENT_STATE_LOOP))
    return;

  static uint32_t loop_time = 0;
  static uint32_t loop_count = 0;
  uint32_t time = 0;

  if (test_mode_) {
    ESP_LOGI(TAG, "Component loop %d for %s : %d ms since last call ...", loop_count++, this->get_name(),
             millis() - loop_time);
  }
  loop_time = millis();

  // If there are some bytes in the receive FIFO we transfers them to the ring buffers
  elapsed_us(time);  // set time to now
  size_t transferred = 0;
  for (auto *child : this->children_) {
    // we look if some characters has been received in the fifo
    transferred += child->xfer_fifo_to_buffer_();
  }
  if ((test_mode_ > 0) && (transferred > 0))
    ESP_LOGI(TAG, "transferred %d bytes from fifo to buffer - execution time %d µs...", transferred, elapsed_us(time));

#ifdef TEST_COMPONENT
  if (test_mode_ == 1) {  // test component in loop back
    char message[64];
    elapsed_us(time);  // set time to now
    for (auto *child : this->children_) {
      snprintf(message, sizeof(message), "%s:%s", this->get_name(), child->get_channel_name());
      child->uart_send_test_(message);
      uint32_t const start_time = millis();
      while (child->tx_fifo_is_not_empty_()) {  // wait until buffer empty
        if (millis() - start_time > 100) {
          ESP_LOGE(TAG, "Timed out flushing - %d bytes in buffer...", child->tx_in_fifo_());
          break;
        }
        yield();  // reschedule our thread to avoid blocking
      }
      bool status = child->uart_receive_test_(message);
      ESP_LOGI(TAG, "Test %s => send/received %d bytes %s - execution time %d µs...", message, XFER_MAX_SIZE,
               status ? "correctly" : "with error", elapsed_us(time));
    }
  }

  if (this->test_mode_ == 2) {  // test component in echo mode
    for (auto *child : this->children_) {
      uint8_t data = 0;
      if (child->available()) {
        child->read_byte(&data);
        ESP_LOGI(TAG, "echo mode: read -> send %02X", data);
        child->write_byte(data);
      }
    }
  }
#endif

  if (this->test_mode_)
    ESP_LOGI(TAG, "loop execution time %d ms...", millis() - loop_time);
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Channel methods
///////////////////////////////////////////////////////////////////////////////
void WK2132Channel::setup_channel() {
  ESP_LOGCONFIG(TAG, "  Setting up UART %s:%s ...", this->parent_->get_name(), this->get_channel_name());
  // we enable transmit and receive on this channel
  this->channel_reg_(REG_WK2132_SCR) = SCR_RXEN | SCR_TXEN;
  this->reset_fifo_();
  this->receive_buffer_.clear();
  this->set_line_param_();
  this->set_baudrate_();
}

void WK2132Channel::dump_channel() {
  ESP_LOGCONFIG(TAG, "  UART %s ...", this->get_channel_name());
  ESP_LOGCONFIG(TAG, "    Baud rate: %d Bd", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "    Data bits: %d", this->data_bits_);
  ESP_LOGCONFIG(TAG, "    Stop bits: %d", this->stop_bits_);
  ESP_LOGCONFIG(TAG, "    Parity: %s", p2s(this->parity_));
}

void WK2132Channel::reset_fifo_() {
  // we reset and enable all FIFO
  this->channel_reg_(REG_WK2132_FCR) = FCR_TFEN | FCR_RFEN | FCR_TFRST | FCR_RFRST;
}

void WK2132Channel::set_line_param_() {
  this->data_bits_ = 8;  // always equal to 8 for WK2132 (cant be changed)
  WK2132Register &lcr = this->channel_reg_(REG_WK2132_LCR);
  lcr &= 0xF0;  // we clear the lower 4 bit of LCR
  if (this->stop_bits_ == 2)
    lcr |= LCR_STPL;
  switch (this->parity_) {  // parity selection settings
    case uart::UART_CONFIG_PARITY_ODD:
      lcr |= (LCR_PAEN | LCR_PAR_ODD);
      break;
    case uart::UART_CONFIG_PARITY_EVEN:
      lcr |= (LCR_PAEN | LCR_PAR_EVEN);
      break;
    default:
      break;  // no parity 000x
  }
  ESP_LOGV(TAG, "    line config: %d data_bits, %d stop_bits, parity %s register [%s]", this->data_bits_,
           this->stop_bits_, p2s(this->parity_), I2CS(lcr.get()));
}

void WK2132Channel::set_baudrate_() {
  uint16_t const val_int = this->parent_->crystal_ / (this->baud_rate_ * 16) - 1;
  uint16_t val_dec = (this->parent_->crystal_ % (this->baud_rate_ * 16)) / (this->baud_rate_ * 16);
  uint8_t const baud_high = (uint8_t) (val_int >> 8);
  uint8_t const baud_low = (uint8_t) (val_int & 0xFF);
  while (val_dec > 0x0A)
    val_dec /= 0x0A;
  uint8_t const baud_dec = (uint8_t) (val_dec);

  this->parent_->page1_ = true;  // switch to page 1
  this->channel_reg_(REG_WK2132_SPAGE) = 1;
  this->channel_reg_(REG_WK2132_BRH) = baud_high;
  this->channel_reg_(REG_WK2132_BRL) = baud_low;
  this->channel_reg_(REG_WK2132_BRD) = baud_dec;
  this->parent_->page1_ = false;  // switch back to page 0
  this->channel_reg_(REG_WK2132_SPAGE) = 0;

  ESP_LOGV(TAG, "    Crystal=%d baudrate=%d => registers [%d %d %d]", this->parent_->crystal_, this->baud_rate_,
           baud_high, baud_low, baud_dec);
}

inline bool WK2132Channel::tx_fifo_is_not_empty_() { return this->channel_reg_(REG_WK2132_FSR) & FSR_TFDAT; }

size_t WK2132Channel::tx_in_fifo_() {
  size_t tfcnt = this->channel_reg_(REG_WK2132_TFCNT);
  if (tfcnt == 0) {
    uint8_t const fsr = this->channel_reg_(REG_WK2132_FSR);
    if (fsr & FSR_TFFULL) {
      ESP_LOGVV(TAG, "tx_in_fifo full FSR=%s", I2CS(fsr));
      tfcnt = FIFO_SIZE;
    }
  }
  ESP_LOGVV(TAG, "tx_in_fifo %d", tfcnt);
  return tfcnt;
}

size_t WK2132Channel::rx_in_fifo_() {
  size_t available = this->channel_reg_(REG_WK2132_RFCNT);
  uint8_t const fsr = this->channel_reg_(REG_WK2132_FSR);
  if (fsr & (FSR_RFOE | FSR_RFLB | FSR_RFFE | FSR_RFPE)) {
    if (fsr & FSR_RFOE)
      ESP_LOGE(TAG, "Receive data overflow FSR=%s", I2CS(fsr));
    if (fsr & FSR_RFLB)
      ESP_LOGE(TAG, "Receive line break FSR=%s", I2CS(fsr));
    if (fsr & FSR_RFFE)
      ESP_LOGE(TAG, "Receive frame error FSR=%s", I2CS(fsr));
    if (fsr & FSR_RFPE)
      ESP_LOGE(TAG, "Receive parity error FSR=%s", I2CS(fsr));
  }
  if ((available == 0) && (fsr & FSR_RFDAT)) {
    // here we should be very careful because we can have something like this:
    // -  at time t0 we read RFCNT=0 because nothing yet received
    // -  at time t0+delta we might read FIFO not empty because one byte has just been received
    // -  so to be sure we need to do another read of RFCNT. If still zero -> buffer full
    available = this->channel_reg_(REG_WK2132_RFCNT);
    if (available == 0) {  // still zero ?
      ESP_LOGVV(TAG, "rx_in_fifo full FSR=%s", I2CS(fsr));
      available = FIFO_SIZE;
    }
  }
  ESP_LOGVV(TAG, "rx_in_fifo bytes=%d status=%s", available, I2CS(fsr));
  return available;
}

bool WK2132Channel::peek_byte(uint8_t *buffer) {
  auto available = this->receive_buffer_.count();
  if (!available)
    xfer_fifo_to_buffer_();
  return this->receive_buffer_.peek(*buffer);
}

int WK2132Channel::available() {
  size_t available = this->receive_buffer_.count();
  if (!available)
    available = xfer_fifo_to_buffer_();
  return available;
}

bool WK2132Channel::read_array(uint8_t *buffer, size_t length) {
  bool status = true;
  auto available = this->receive_buffer_.count();
  if (length > available) {
    ESP_LOGW(TAG, "read_array: buffer underflow requested %d bytes only %d available...", length, available);
    length = available;
    status = false;
  }
  // retrieve the bytes from ring buffer
  for (size_t i = 0; i < length; i++) {
    this->receive_buffer_.pop(buffer[i]);
  }
  ESP_LOGVV(TAG, "read_array(ch=%d buffer[0]=%02X, length=%d): status %s", this->channel_, *buffer, length,
            status ? "OK" : "ERROR");
  return status;
}

void WK2132Channel::write_array(const uint8_t *buffer, size_t length) {
  if (length > XFER_MAX_SIZE) {
    ESP_LOGE(TAG, "Write_array: invalid call - requested %d bytes max size %d ...", length, XFER_MAX_SIZE);
    length = XFER_MAX_SIZE;
  }
  this->fifo_reg_().write_fifo(buffer, length);
}

void WK2132Channel::flush() {
  uint32_t const start_time = millis();
  while (this->tx_fifo_is_not_empty_()) {  // wait until buffer empty
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "flush: timed out - still %d bytes not sent...", this->tx_in_fifo_());
      return;
    }
    yield();  // reschedule our thread to avoid blocking
  }
}

size_t WK2132Channel::xfer_fifo_to_buffer_() {
  size_t to_transfer = this->rx_in_fifo_();
  size_t free = this->receive_buffer_.free();
  if (to_transfer > XFER_MAX_SIZE)
    to_transfer = XFER_MAX_SIZE;
  if (to_transfer > free)
    to_transfer = free;  // we'll do the rest next time
  if (to_transfer) {
    uint8_t data[to_transfer];
    this->fifo_reg_().read_fifo(data, to_transfer);
    for (size_t i = 0; i < to_transfer; i++)
      this->receive_buffer_.push(data[i]);
  } else {
    ESP_LOGVV(TAG, "xfer_fifo_to_buffer: nothing to to_transfer");
  }
  return to_transfer;
}

///
// TEST COMPONENT
//
#ifdef TEST_COMPONENT
/// @addtogroup test_ Test component information
/// @{

/// @brief An increment "Functor" (i.e. a class object that acts like a method with state!)
///
/// Functors are objects that can be treated as though they are a function or function pointer.
class Increment {
 public:
  /// @brief constructor: initialize current value to 0
  Increment() : i_(0) {}
  /// @brief overload of the parenthesis operator.
  /// Returns the current value and auto increment it
  /// @return the current value.
  uint8_t operator()() { return i_++; }

 private:
  uint8_t i_;
};

/// @brief Hex converter to print/display a buffer in hexadecimal format (16 hex values / line).
/// @param buffer contains the values to display
void print_buffer(std::vector<uint8_t> buffer) {
  char hex_buffer[80];
  hex_buffer[50] = 0;
  for (size_t i = 0; i < buffer.size(); i++) {
    snprintf(&hex_buffer[3 * (i % 16)], sizeof(hex_buffer), "%02X ", buffer[i]);
    if (i % 16 == 15)
      ESP_LOGI(TAG, "   %s", hex_buffer);
  }
  if (buffer.size() % 16) {
    // null terminate if incomplete line
    hex_buffer[3 * (buffer.size() % 16) + 2] = 0;
    ESP_LOGI(TAG, "   %s", hex_buffer);
  }
}

/// @brief test the write_array method
void WK2132Channel::uart_send_test_(char *message) {
  auto start_exec = micros();
  std::vector<uint8_t> output_buffer(XFER_MAX_SIZE);
  generate(output_buffer.begin(), output_buffer.end(), Increment());  // fill with incrementing number
  this->write_array(&output_buffer[0], XFER_MAX_SIZE);                // we send the buffer
  ESP_LOGV(TAG, "%s => sent %d bytes - exec time %d µs ...", message, XFER_MAX_SIZE, micros() - start_exec);
}

/// @brief test read_array method
bool WK2132Channel::uart_receive_test_(char *message) {
  auto start_exec = micros();
  bool status = true;
  size_t received;
  std::vector<uint8_t> buffer(XFER_MAX_SIZE);

  // we wait until we have received all the bytes
  uint32_t const start_time = millis();
  while (XFER_MAX_SIZE > (received = this->available())) {
    this->xfer_fifo_to_buffer_();
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "uart_receive_test_() timeout: only %d bytes received...", received);
      break;
    }
    yield();  // reschedule our thread to avoid blocking
  }

  uint8_t peek_value = 0;
  this->peek_byte(&peek_value);
  if (peek_value != 0) {
    ESP_LOGE(TAG, "Peek first byte value error...");
    print_buffer(buffer);
    status = false;
  }

  status = this->read_array(&buffer[0], XFER_MAX_SIZE) && status;
  for (size_t i = 0; i < XFER_MAX_SIZE; i++) {
    if (buffer[i] != i) {
      ESP_LOGE(TAG, "Read buffer contains error...");
      print_buffer(buffer);
      status = false;
      break;
    }
  }

  ESP_LOGV(TAG, "%s => received %d bytes  status %s - exec time %d µs ...", message, received, status ? "OK" : "ERROR",
           micros() - start_exec);
  return status;
}

/// @}
#endif

}  // namespace wk2132
}  // namespace esphome
