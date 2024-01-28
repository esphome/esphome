/// @file wk_base.cpp
/// @author DrCoolzic
/// @brief wk_base classes implementation

#include "wk_base.h"

namespace esphome {
namespace wk_base {

/*! @mainpage WKBaseComponent source code documentation
 This documentation provides information about the implementation of the WKBaseComponent in ESPHome.
 The WKBaseComponent utilizes two primary classes: the WKBaseComponent class, the WKBaseChannel class,
 along with the RingBuffer and WKBaseRegister helper classes. Below, you'll find short descriptions
 of these three classes.

  @section RingBuffer_ The RingBuffer template class
The RingBuffer template class has it names implies implement a simple ring buffer helper class. This straightforward
container implements FIFO functionality, enabling bytes to be pushed into one side and popped from the other in the
order of entry. Implementation is classic and therefore not described in any details.

  @section WKBaseRegister_ The WKBaseRegister class
 The WKBaseRegister helper class creates objects that act as proxies to device register.
 @details This is an abstract virtual class (interface) that provides all the necessary access to registers while hiding
 the actual implementation. The access to the registers can be made through an I²C bus in for example for wk2168_i2c
 component or through a SPI bus for example in the case of the wk2168_spi component. Derived classes will actually
 performs the specific bus operations.

 @section WK2168Component_ The WKBaseComponent class
The WKBaseComponent class stores the information global to a WK family component and provides methods to set/access
this information. It also serves as a container for WKBaseChannel instances. This is done by maintaining an array of
references these WKBaseChannel instances. This class derives from two ESPHome classes. The esphome::Component class and
the i2c::I2CDevice class. This class override esphome::Component::loop() method which is to facilitate the seamless
transfer of accumulated bytes from the receive FIFO into the ring buffer. This process ensures quick access to the
stored bytes, enhancing the overall efficiency of the component.

 @section WKBaseChannel_ The WKBaseChannel class
The WKBaseChannel class is used to implement all the virtual methods of the ESPHome uart::UARTComponent class. An
individual instance of this class is created for each UART channel. It has a link back to the WKBaseComponent object it
belongs to. This class derives from the uart::UARTComponent class. It collaborates through an aggregation with
WKBaseComponent. This implies that WKBaseComponent acts as a container, housing several WKBaseChannel instances.
Furthermore, the WKBaseChannel class derives from the ESPHome uart::UARTComponent class, it also has an association
relationship with the RingBuffer and WKBaseRegister helper classes. Consequently, when a WKBaseChannel instance is
destroyed, the associated RingBuffer instance is also destroyed.
*/

static const char *const TAG = "wk_base";

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
/// Convert std::string to C string
#define I2CS(val) (i2s(val).c_str())

/// @brief measure the time elapsed between two calls
/// @param last_time time of the previous call
/// @return the elapsed time in microseconds
uint32_t elapsed_ms(uint32_t &last_time) {
  uint32_t e = millis() - last_time;
  last_time = millis();
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
// The WKBaseRegister methods
///////////////////////////////////////////////////////////////////////////////
WKBaseRegister &WKBaseRegister::operator=(uint8_t value) {
  write_reg(value);
  return *this;
}

WKBaseRegister &WKBaseRegister::operator&=(uint8_t value) {
  value &= read_reg();
  write_reg(value);
  return *this;
}

WKBaseRegister &WKBaseRegister::operator|=(uint8_t value) {
  value |= read_reg();
  write_reg(value);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
// The WKBaseComponent methods
///////////////////////////////////////////////////////////////////////////////
void WKBaseComponent::loop() {
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
  elapsed_ms(time);  // set time to now
  size_t transferred = 0;
  for (auto *child : this->children_) {
    // we look if some characters has been received in the fifo
    transferred += child->xfer_fifo_to_buffer_();
  }
  if ((test_mode_ > 0) && (transferred > 0)) {
    ESP_LOGI(TAG, "we transferred %d bytes from fifo to buffer - execution time %d ms...", transferred,
             elapsed_ms(time));
  }

#ifdef TEST_COMPONENT
  if (test_mode_ == 1) {  // test component in loopback
    char message[64];
    elapsed_ms(time);  // set time to now
    for (auto *child : this->children_) {
      snprintf(message, sizeof(message), "%s:%s", this->get_name(), child->get_channel_name());
      child->uart_send_test_(message);
      uint32_t const start_time = millis();
      while (child->tx_fifo_is_not_empty_()) {  // wait until buffer empty
        if (millis() - start_time > 1500) {
          ESP_LOGE(TAG, "timeout while flushing - %d bytes left in buffer...", child->tx_in_fifo_());
          break;
        }
        yield();  // reschedule our thread to avoid blocking
      }
      bool status = child->uart_receive_test_(message);
      ESP_LOGI(TAG, "Test %s => send/received %d bytes %s - execution time %d ms...", message, XFER_MAX_SIZE,
               status ? "correctly" : "with error", elapsed_ms(time));
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
// The WKBaseChannel methods
///////////////////////////////////////////////////////////////////////////////
void WKBaseChannel::setup_channel() {
  ESP_LOGCONFIG(TAG, "  Setting up UART %s:%s ...", this->parent_->get_name(), this->get_channel_name());
  // we enable transmit and receive on this channel
  if (this->check_channel_down()) {
    ESP_LOGCONFIG(TAG, "  Error channel %s not working...", this->get_channel_name());
  }

  this->reg_(WKREG_SCR) = SCR_RXEN | SCR_TXEN;
  this->reset_fifo_();
  this->receive_buffer_.clear();
  this->set_line_param_();
  this->set_baudrate_();
}

void WKBaseChannel::dump_channel() {
  ESP_LOGCONFIG(TAG, "  UART %s ...", this->get_channel_name());
  ESP_LOGCONFIG(TAG, "    Baud rate: %d Bd", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "    Data bits: %d", this->data_bits_);
  ESP_LOGCONFIG(TAG, "    Stop bits: %d", this->stop_bits_);
  ESP_LOGCONFIG(TAG, "    Parity: %s", p2s(this->parity_));
}

void WKBaseChannel::reset_fifo_() {
  // we reset and enable all FIFO
  this->reg_(WKREG_FCR) = FCR_TFEN | FCR_RFEN | FCR_TFRST | FCR_RFRST;
}

void WKBaseChannel::set_line_param_() {
  this->data_bits_ = 8;  // always equal to 8 for WK2168 (cant be changed)
  WKBaseRegister &lcr = this->reg_(WKREG_LCR);
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
           this->stop_bits_, p2s(this->parity_), I2CS(lcr.read_reg()));
}

void WKBaseChannel::set_baudrate_() {
  if (this->baud_rate_ > this->parent_->crystal_ / 16) {
    baud_rate_ = this->parent_->crystal_ / 16;
    ESP_LOGE(TAG, " Requested baudrate too high for crystal %d was reduced to %d Bd", this->parent_->crystal_,
             this->baud_rate_);
  };
  uint16_t const val_int = this->parent_->crystal_ / (this->baud_rate_ * 16) - 1;
  uint16_t val_dec = (this->parent_->crystal_ % (this->baud_rate_ * 16)) / (this->baud_rate_ * 16);
  uint8_t const baud_high = (uint8_t) (val_int >> 8);
  uint8_t const baud_low = (uint8_t) (val_int & 0xFF);
  while (val_dec > 0x0A)
    val_dec /= 0x0A;
  uint8_t const baud_dec = (uint8_t) (val_dec);

  this->parent_->page1_ = true;  // switch to page 1
  this->reg_(WKREG_SPAGE) = 1;
  this->reg_(WKREG_BRH) = baud_high;
  this->reg_(WKREG_BRL) = baud_low;
  this->reg_(WKREG_BRD) = baud_dec;
  this->parent_->page1_ = false;  // switch back to page 0
  this->reg_(WKREG_SPAGE) = 0;

  ESP_LOGV(TAG, "    Crystal=%d baudrate=%d => registers [%d %d %d]", this->parent_->crystal_, this->baud_rate_,
           baud_high, baud_low, baud_dec);
}

inline bool WKBaseChannel::tx_fifo_is_not_empty_() { return this->reg_(WKREG_FSR) & FSR_TFDAT; }

size_t WKBaseChannel::tx_in_fifo_() {
  size_t tfcnt = this->reg_(WKREG_TFCNT);
  if (tfcnt == 0) {
    uint8_t const fsr = this->reg_(WKREG_FSR);
    if (fsr & FSR_TFFULL) {
      ESP_LOGV(TAG, "tx FIFO full FSR=%s", I2CS(fsr));
      tfcnt = FIFO_SIZE;
    }
  }
  ESP_LOGVV(TAG, "tx FIFO contains %d bytes", tfcnt);
  return tfcnt;
}

size_t WKBaseChannel::rx_in_fifo_() {
  size_t available = this->reg_(WKREG_RFCNT);
  uint8_t const fsr = this->reg_(WKREG_FSR);
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
    // -  so to be sure we need to do another read of RFCNT and if it is still zero -> buffer full
    available = this->reg_(WKREG_RFCNT);
    if (available == 0) {  // still zero ?
      ESP_LOGV(TAG, "rx FIFO is full FSR=%s", I2CS(fsr));
      available = FIFO_SIZE;
    }
  }
  ESP_LOGVV(TAG, "rx FIFO contain %d bytes - FSR status=%s", available, I2CS(fsr));
  return available;
}

bool WKBaseChannel::check_channel_down() {
  // to check if we channel is up we write to the LCR W/R register
  WKBaseRegister &lcr = this->reg_(WKREG_LCR);
  lcr = 0x3F;
  uint8_t val = lcr;
  if (val != 0x3F) {
    ESP_LOGE(TAG, "R/W of LCR failed expected 0x3F received 0x%02X", val);
    return true;
  }
  lcr = 0;
  val = lcr;
  if (val != 0x00) {
    ESP_LOGE(TAG, "R/W of LCR failed expected 0x00 received 0x%02X", val);
    return true;
  }
  return false;
}

bool WKBaseChannel::peek_byte(uint8_t *buffer) {
  auto available = this->receive_buffer_.count();
  if (!available)
    xfer_fifo_to_buffer_();
  return this->receive_buffer_.peek(*buffer);
}

int WKBaseChannel::available() {
  size_t available = this->receive_buffer_.count();
  if (!available)
    available = xfer_fifo_to_buffer_();
  return available;
}

bool WKBaseChannel::read_array(uint8_t *buffer, size_t length) {
  bool status = true;
  auto available = this->receive_buffer_.count();
  if (length > available) {
    ESP_LOGW(TAG, "read_array: buffer underflow requested %d bytes only %d bytes available...", length, available);
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

void WKBaseChannel::write_array(const uint8_t *buffer, size_t length) {
  if (length > XFER_MAX_SIZE) {
    ESP_LOGE(TAG, "Write_array: invalid call - requested %d bytes but max size %d ...", length, XFER_MAX_SIZE);
    length = XFER_MAX_SIZE;
  }
  this->reg_(0).write_fifo(const_cast<uint8_t *>(buffer), length);
}

void WKBaseChannel::flush() {
  uint32_t const start_time = millis();
  while (this->tx_fifo_is_not_empty_()) {  // wait until buffer empty
    if (millis() - start_time > 200) {
      ESP_LOGW(TAG, "WARNING flush timeout - still %d bytes not sent after 200 ms...", this->tx_in_fifo_());
      return;
    }
    yield();  // reschedule our thread to avoid blocking
  }
}

size_t WKBaseChannel::xfer_fifo_to_buffer_() {
  size_t to_transfer = this->rx_in_fifo_();
  size_t free = this->receive_buffer_.free();
  if (to_transfer > XFER_MAX_SIZE)
    to_transfer = XFER_MAX_SIZE;
  if (to_transfer > free)
    to_transfer = free;  // we'll do the rest next time
  if (to_transfer) {
    uint8_t data[to_transfer];
    this->reg_(0).read_fifo(data, to_transfer);
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

/// @brief Hex converter to print/display a buffer in hexadecimal format (32 hex values / line).
/// @param buffer contains the values to display
void print_buffer(std::vector<uint8_t> buffer) {
  char hex_buffer[100];
  hex_buffer[(3 * 32) + 1] = 0;
  for (size_t i = 0; i < buffer.size(); i++) {
    snprintf(&hex_buffer[3 * (i % 32)], sizeof(hex_buffer), "%02X ", buffer[i]);
    if (i % 32 == 31)
      ESP_LOGI(TAG, "   %s", hex_buffer);
  }
  if (buffer.size() % 32) {
    // null terminate if incomplete line
    hex_buffer[3 * (buffer.size() % 32) + 1] = 0;
    ESP_LOGI(TAG, "   %s", hex_buffer);
  }
}

/// @brief test the write_array method
void WKBaseChannel::uart_send_test_(char *message) {
  auto start_exec = micros();
  std::vector<uint8_t> output_buffer(XFER_MAX_SIZE);
  generate(output_buffer.begin(), output_buffer.end(), Increment());  // fill with incrementing number
  this->write_array(&output_buffer[0], XFER_MAX_SIZE);                // we send the buffer
  ESP_LOGV(TAG, "%s => sent %d bytes - exec time %d µs ...", message, XFER_MAX_SIZE, micros() - start_exec);
}

/// @brief test read_array method
bool WKBaseChannel::uart_receive_test_(char *message) {
  auto start_exec = micros();
  bool status = true;
  size_t received;
  std::vector<uint8_t> buffer(XFER_MAX_SIZE);

  // we wait until we have received all the bytes
  uint32_t const start_time = millis();
  while (XFER_MAX_SIZE > (received = this->available())) {
    this->xfer_fifo_to_buffer_();
    if (millis() - start_time > 1500) {
      ESP_LOGE(TAG, "uart_receive_test_() timeout: only %d bytes received...", received);
      break;
    }
    yield();  // reschedule our thread to avoid blocking
  }

  uint8_t peek_value = 0;
  this->peek_byte(&peek_value);
  if (peek_value != 0) {
    ESP_LOGE(TAG, "Peek first byte value error...");
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

}  // namespace wk_base
}  // namespace esphome
