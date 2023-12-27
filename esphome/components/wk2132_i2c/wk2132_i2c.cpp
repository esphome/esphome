/// @file wk2132_i2c.cpp
/// @author DrCoolzic
/// @brief wk2132 classes implementation

#include "wk2132_i2c.h"
// #include "esphome/components/wk2132/wk2132.h"

namespace esphome {
namespace wk2132_i2c {

static const char *const TAG = "wk2132_i2c";

static const char *const REG_TO_STR_P0[] = {"GENA", "GRST", "GMUT",  "SPAGE", "SCR", "LCR", "FCR",
                                            "SIER", "SIFR", "TFCNT", "RFCNT", "FSR", "LSR", "FDAT"};
static const char *const REG_TO_STR_P1[] = {"GENA", "GRST", "GMUT",  "SPAGE", "BAUD1", "BAUD0", "PRES",
                                            "RFTL", "TFTL", "_INV_", "_INV_", "_INV_", "_INV_"};

// method to print a register value as text: used in the log messages ...
const char *reg_to_str(int reg, bool page1) { return page1 ? REG_TO_STR_P1[reg] : REG_TO_STR_P0[reg]; }

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }

/// Convert std::string to C string
#define I2CS(val) (i2s(val).c_str())

// /// @brief measure the time elapsed between two calls
// /// @param last_time time of the previous call
// /// @return the elapsed time in microseconds
// uint32_t elapsed_us(uint32_t &last_time) {
//   uint32_t e = micros() - last_time;
//   last_time = micros();
//   return e;
// };

/// @brief Computes the I²C bus's address used to access the component
/// @param base_address the base address of the component - set by the A1 A0 pins
/// @param channel (0-3) the UART channel
/// @param fifo (0-1) 0 = access to internal register, 1 = direct access to fifo
/// @return the i2c address to use
inline uint8_t i2c_address(uint8_t base_address, uint8_t channel, uint8_t fifo) {
  // the address of the device is:
  // +----+----+----+----+----+----+----+----+
  // |  0 | A1 | A0 |  1 |  0 | C1 | C0 |  F |
  // +----+----+----+----+----+----+----+----+
  // where:
  // - A1,A0 is the address read from A1,A0 switch
  // - C1,C0 is the channel number (in practice only 00 or 01)
  // - F is: 0 when accessing register, one when accessing FIFO
  uint8_t const addr = base_address | channel << 1 | fifo;
  return addr;
}

// /// @brief Converts the parity enum value to a C string
// /// @param parity enum
// /// @return the string
// const char *p2s(uart::UARTParityOptions parity) {
//   using namespace uart;
//   switch (parity) {
//     case UART_CONFIG_PARITY_NONE:
//       return "NONE";
//     case UART_CONFIG_PARITY_EVEN:
//       return "EVEN";
//     case UART_CONFIG_PARITY_ODD:
//       return "ODD";
//     default:
//       return "UNKNOWN";
//   }
// }

///////////////////////////////////////////////////////////////////////////////
// The WK2132Register methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WK2132RegisterI2C::get() const {
  uint8_t value = 0x00;
  WK2132ComponentI2C *rcp = static_cast<WK2132ComponentI2C *>(this->parent_);
  rcp->address_ = i2c_address(rcp->base_address_, this->channel_, this->fifo_);  // update the i2c bus address
  auto error = rcp->read_register(this->register_, &value, 1);
  if (error == i2c::ERROR_OK) {
    this->parent_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::get() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
              reg_to_str(this->register_, rcp->page1_), this->channel_, value, (int) error);
  } else {  // error
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::get() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
             reg_to_str(this->register_, rcp->page1_), this->channel_, value, (int) error);
  }
  return value;
}

void WK2132RegisterI2C::set(uint8_t value) {
  WK2132ComponentI2C *rcp = static_cast<WK2132ComponentI2C *>(this->parent_);
  rcp->address_ = i2c_address(rcp->base_address_, this->channel_, this->fifo_);  // update the i2c bus
  auto error = rcp->write_register(this->register_, &value, 1);
  if (error == i2c::ERROR_OK) {
    this->parent_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::set() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
              reg_to_str(this->register_, rcp->page1_), this->channel_, value, (int) error);
  } else {  // error
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::set() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
             reg_to_str(this->register_, rcp->page1_), this->channel_, value, (int) error);
  }
}

void WK2132RegisterI2C::get_array(uint8_t *data, size_t length) {
  WK2132ComponentI2C *rcp = static_cast<WK2132ComponentI2C *>(this->parent_);
  rcp->address_ = i2c_address(rcp->base_address_, this->channel_, this->fifo_);  // fifo flag is set
  auto error = rcp->read(data, length);
  if (error == i2c::ERROR_OK) {
    this->parent_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::get_array() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
              reg_to_str(this->register_, rcp->page1_), this->channel_, length, (int) error);
  } else {  // error
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::get_array() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
             reg_to_str(this->register_, rcp->page1_), this->channel_, length, (int) error);
  }
}

void WK2132RegisterI2C::set_array(const uint8_t *data, size_t length) {
  WK2132ComponentI2C *rcp = static_cast<WK2132ComponentI2C *>(this->parent_);
  rcp->address_ = i2c_address(rcp->base_address_, this->channel_, this->fifo_);  // set fifo flag
  auto error = rcp->write(data, length);
  if (error == i2c::ERROR_OK) {
    this->parent_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2132Register::set_array() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
              reg_to_str(this->register_, rcp->page1_), this->channel_, length, (int) error);
  } else {  // error
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "WK2132Register::set_array() @%02X r=%s, ch=%d b=%02X, I2C_code:%d", rcp->address_,
             reg_to_str(this->register_, rcp->page1_), this->channel_, length, (int) error);
  }
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Component methods
///////////////////////////////////////////////////////////////////////////////
void WK2132ComponentI2C::setup() {
  using namespace wk2132;
  // before any manipulation we store the address to base_address_ for future use
  this->base_address_ = this->address_;
  ESP_LOGCONFIG(TAG, "Setting up wk2132_i2c: %s with %d UARTs at @%02X ...", this->get_name(), this->children_.size(),
                this->base_address_);

  // enable both channels
  this->component_reg(REG_WK2132_GENA) = GENA_C1EN | GENA_C2EN;
  // reset channels
  this->component_reg(REG_WK2132_GRST) = GRST_C1RST | GRST_C2RST;
  // initialize the spage register to page 0
  this->component_reg(REG_WK2132_SPAGE) = 0;
  this->page1_ = false;

  // we setup our children channels
  for (auto *child : this->children_) {
    child->setup_channel_();
  }
}

void WK2132ComponentI2C::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %d", this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  this->address_ = this->base_address_;  // we restore the base_address before display (less confusing)
  LOG_I2C_DEVICE(this);

  for (auto *child : this->children_) {
    child->dump_channel_();
  }
}

// ///////////////////////////////////////////////////////////////////////////////
// // The WK2132Channel methods
// ///////////////////////////////////////////////////////////////////////////////

// void WK2132Channel::setup_channel_() {
//   ESP_LOGCONFIG(TAG, "  Setting up UART %s:%s ...", this->parent_->get_name(), this->get_channel_name());
//   // we enable transmit and receive on this channel
//   this->channel_reg(REG_WK2132_SCR) = SCR_RXEN | SCR_TXEN;

//   this->reset_fifo_();
//   this->receive_buffer_.clear();
//   this->set_line_param_();
//   this->set_baudrate_();
// }

// void WK2132Channel::reset_fifo_() {
//   // we reset and enable all FIFO
//   this->channel_reg(REG_WK2132_FCR) = FCR_TFEN | FCR_RFEN | FCR_TFRST | FCR_RFRST;
// }

// void WK2132Channel::set_line_param_() {
//   this->data_bits_ = 8;  // always equal to 8 for WK2132 (cant be changed)

//   // WK2132Register lcr{this->parent_, REG_WK2132_LCR, this->channel_};
//   WK2132Register lcr = this->channel_reg(REG_WK2132_LCR);
//   lcr &= 0xF0;  // we clear the lower 4 bit of LCR
//   if (this->stop_bits_ == 2)
//     lcr |= LCR_STPL;
//   switch (this->parity_) {  // parity selection settings
//     case uart::UART_CONFIG_PARITY_ODD:
//       lcr |= (LCR_PAEN | LCR_PAR_ODD);
//       break;
//     case uart::UART_CONFIG_PARITY_EVEN:
//       lcr |= (LCR_PAEN | LCR_PAR_EVEN);
//       break;
//     default:
//       break;  // no parity 000x
//   }
//   ESP_LOGV(TAG, "    line config: %d data_bits, %d stop_bits, parity %s register [%s]", this->data_bits_,
//            this->stop_bits_, p2s(this->parity_), I2CS(lcr.get()));
// }

// void WK2132Channel::set_baudrate_() {
//   uint16_t const val_int = this->parent_->crystal_ / (this->baud_rate_ * 16) - 1;
//   uint16_t val_dec = (this->parent_->crystal_ % (this->baud_rate_ * 16)) / (this->baud_rate_ * 16);
//   uint8_t const baud_high = (uint8_t) (val_int >> 8);
//   uint8_t const baud_low = (uint8_t) (val_int & 0xFF);
//   while (val_dec > 0x0A)
//     val_dec /= 0x0A;
//   uint8_t const baud_dec = (uint8_t) (val_dec);

//   // switch to page 1
//   this->parent_->page1_ = true;
//   this->channel_reg(REG_WK2132_SPAGE) = 1;
//   this->channel_reg(REG_WK2132_BRH) = baud_high;
//   this->channel_reg(REG_WK2132_BRL) = baud_low;
//   this->channel_reg(REG_WK2132_BRD) = baud_dec;
//   // switch back to page 0
//   this->parent_->page1_ = false;
//   this->channel_reg(REG_WK2132_SPAGE) = 0;

//   ESP_LOGV(TAG, "    Crystal=%d baudrate=%d => registers [%d %d %d]", this->parent_->crystal_, this->baud_rate_,
//            baud_high, baud_low, baud_dec);
// }

// inline bool WK2132Channel::tx_fifo_is_not_empty_() { return this->channel_reg(REG_WK2132_FSR) & FSR_TFDAT; }

// size_t WK2132Channel::tx_in_fifo_() {
//   size_t tfcnt = this->channel_reg(REG_WK2132_TFCNT);
//   if (tfcnt == 0) {
//     uint8_t const fsr = this->channel_reg(REG_WK2132_FSR);
//     if (fsr & FSR_TFFULL) {
//       ESP_LOGVV(TAG, "tx_in_fifo full FSR=%s", I2CS(fsr));
//       tfcnt = FIFO_SIZE;
//     }
//   }
//   ESP_LOGVV(TAG, "tx_in_fifo %d", tfcnt);
//   return tfcnt;
// }

// /// @brief number of bytes in the receive fifo
// /// @return number of bytes
// size_t WK2132Channel::rx_in_fifo_() {
//   size_t available = this->channel_reg(REG_WK2132_RFCNT);
//   uint8_t const fsr = this->channel_reg(REG_WK2132_FSR);
//   if (fsr & (FSR_RFOE | FSR_RFLB | FSR_RFFE | FSR_RFPE)) {
//     if (fsr & FSR_RFOE)
//       ESP_LOGE(TAG, "Receive data overflow FSR=%s", I2CS(fsr));
//     if (fsr & FSR_RFLB)
//       ESP_LOGE(TAG, "Receive line break FSR=%s", I2CS(fsr));
//     if (fsr & FSR_RFFE)
//       ESP_LOGE(TAG, "Receive frame error FSR=%s", I2CS(fsr));
//     if (fsr & FSR_RFPE)
//       ESP_LOGE(TAG, "Receive parity error FSR=%s", I2CS(fsr));
//   }
//   if ((available == 0) && (fsr & FSR_RFDAT)) {
//     // here we should be very careful because we can have something like this:
//     // -  at time t0 we read RFCNT=0 because nothing yet received
//     // -  at time t0+delta we might read FIFO not empty because one byte has just been received
//     // -  so to be sure we need to do another read of RFCNT. If still zero -> buffer full
//     available = this->channel_reg(REG_WK2132_RFCNT);
//     if (available == 0) {  // still zero ?
//       ESP_LOGVV(TAG, "rx_in_fifo full FSR=%s", I2CS(fsr));
//       available = FIFO_SIZE;
//     }
//   }
//   ESP_LOGVV(TAG, "rx_in_fifo bytes=%d status=%s", available, I2CS(fsr));
//   return available;
// }

// bool WK2132Channel::peek_byte(uint8_t *buffer) {
//   auto available = this->receive_buffer_.count();
//   if (!available)
//     xfer_fifo_to_buffer_();
//   return this->receive_buffer_.peek(*buffer);
// }

// int WK2132Channel::available() {
//   size_t available = this->receive_buffer_.count();
//   if (!available)
//     available = xfer_fifo_to_buffer_();
//   return available;
// }

// bool WK2132Channel::read_array(uint8_t *buffer, size_t length) {
//   bool status = true;
//   auto available = this->receive_buffer_.count();
//   if (length > available) {
//     ESP_LOGW(TAG, "read_array: buffer underflow requested %d bytes only %d available...", length, available);
//     length = available;
//     status = false;
//   }
//   // retrieve the bytes from ring buffer
//   for (size_t i = 0; i < length; i++) {
//     this->receive_buffer_.pop(buffer[i]);
//   }
//   ESP_LOGVV(TAG, "read_array(ch=%d buffer[0]=%02X, length=%d): status %s", this->channel_, *buffer, length,
//             status ? "OK" : "ERROR");
//   return status;
// }

// void WK2132Channel::write_array(const uint8_t *buffer, size_t length) {
//   if (length > XFER_MAX_SIZE) {
//     ESP_LOGE(TAG, "Write_array: invalid call - requested %d bytes max size %d ...", length, XFER_MAX_SIZE);
//     length = XFER_MAX_SIZE;
//   }

//   this->parent_->address_ = i2c_address(this->parent_->base_address_, this->channel_, 1);  // set fifo flag
//   auto error = this->parent_->write(buffer, length);
//   if (error == i2c::ERROR_OK) {
//     this->parent_->status_clear_warning();
//     ESP_LOGVV(TAG, "write_array(ch=%d buffer[0]=%02X, length=%d): I2C code %d", this->channel_, *buffer, length,
//               (int) error);
//   } else {  // error
//     this->parent_->status_set_warning();
//     ESP_LOGE(TAG, "write_array(ch=%d buffer[0]=%02X, length=%d): I2C code %d", this->channel_, *buffer, length,
//              (int) error);
//   }
// }

// void WK2132Channel::flush() {
//   uint32_t const start_time = millis();
//   while (this->tx_fifo_is_not_empty_()) {  // wait until buffer empty
//     if (millis() - start_time > 100) {
//       ESP_LOGE(TAG, "flush: timed out - still %d bytes not sent...", this->tx_in_fifo_());
//       return;
//     }
//     yield();  // reschedule our thread to avoid blocking
//   }
// }

// size_t WK2132Channel::xfer_fifo_to_buffer_() {
//   size_t to_transfer = this->rx_in_fifo_();
//   size_t free = this->receive_buffer_.free();
//   if (to_transfer > XFER_MAX_SIZE)
//     to_transfer = XFER_MAX_SIZE;
//   if (to_transfer > free)
//     to_transfer = free;  // we'll do the rest next time
//   if (to_transfer) {
//     uint8_t data[to_transfer];
//     this->parent_->address_ = i2c_address(this->parent_->base_address_, this->channel_, 1);  // fifo flag is set
//     auto error = this->parent_->read(data, to_transfer);
//     if (error == i2c::ERROR_OK) {
//       ESP_LOGVV(TAG, "xfer_fifo_to_buffer_: transferred %d bytes from fifo to buffer", to_transfer);
//     } else {
//       ESP_LOGE(TAG, "xfer_fifo_to_buffer_: error i2c=%d while reading", (int) error);
//     }

//     for (size_t i = 0; i < to_transfer; i++)
//       this->receive_buffer_.push(data[i]);
//   } else {
//     ESP_LOGVV(TAG, "xfer_fifo_to_buffer: nothing to to_transfer");
//   }
//   return to_transfer;
// }

// void WK2132Component::loop() {
//   if (this->component_state_ & (COMPONENT_STATE_MASK != COMPONENT_STATE_LOOP))
//     return;

//   static uint32_t loop_time = 0;
//   static uint32_t loop_count = 0;
//   uint32_t time = 0;

//   if (test_mode_) {
//     ESP_LOGI(TAG, "Component loop %d for %s : %d ms since last call ...", loop_count++, this->get_name(),
//              millis() - loop_time);
//   }
//   loop_time = millis();

//   // If there are some bytes in the receive FIFO we transfers them to the ring buffers
//   elapsed_us(time);  // set time to now
//   size_t transferred = 0;
//   for (auto *child : this->children_) {
//     // we look if some characters has been received in the fifo
//     transferred += child->xfer_fifo_to_buffer_();
//   }
//   if ((test_mode_ > 0) && (transferred > 0))
//     ESP_LOGI(TAG, "transferred %d bytes from fifo to buffer - execution time %d µs...", transferred,
//     elapsed_us(time));

// #ifdef TEST_COMPONENT
//   if (test_mode_ == 1) {  // test component in loop back
//     char message[64];
//     elapsed_us(time);  // set time to now
//     for (auto *child : this->children_) {
//       snprintf(message, sizeof(message), "%s:%s", this->get_name(), child->get_channel_name());
//       child->uart_send_test_(message);
//       uint32_t const start_time = millis();
//       while (child->tx_fifo_is_not_empty_()) {  // wait until buffer empty
//         if (millis() - start_time > 100) {
//           ESP_LOGE(TAG, "Timed out flushing - %d bytes in buffer...", child->tx_in_fifo_());
//           break;
//         }
//         yield();  // reschedule our thread to avoid blocking
//       }
//       bool status = child->uart_receive_test_(message);
//       ESP_LOGI(TAG, "Test %s => send/received %d bytes %s - execution time %d µs...", message, XFER_MAX_SIZE,
//                status ? "correctly" : "with error", elapsed_us(time));
//     }
//   }

//   if (this->test_mode_ == 2) {  // test component in echo mode
//     for (auto *child : this->children_) {
//       uint8_t data = 0;
//       if (child->available()) {
//         child->read_byte(&data);
//         ESP_LOGI(TAG, "echo mode: read -> send %02X", data);
//         child->write_byte(data);
//       }
//     }
//   }
// #endif

//   if (this->test_mode_)
//     ESP_LOGI(TAG, "loop execution time %d ms...", millis() - loop_time);
// }

// ///
// // TEST COMPONENT
// //
// #ifdef TEST_COMPONENT
// /// @addtogroup test_ Test component information
// /// @{

// /// @brief An increment "Functor" (i.e. a class object that acts like a method with state!)
// ///
// /// Functors are objects that can be treated as though they are a function or function pointer.
// class Increment {
//  public:
//   /// @brief constructor: initialize current value to 0
//   Increment() : i_(0) {}
//   /// @brief overload of the parenthesis operator.
//   /// Returns the current value and auto increment it
//   /// @return the current value.
//   uint8_t operator()() { return i_++; }

//  private:
//   uint8_t i_;
// };

// /// @brief Hex converter to print/display a buffer in hexadecimal format (16 hex values / line).
// /// @param buffer contains the values to display
// void print_buffer(std::vector<uint8_t> buffer) {
//   char hex_buffer[80];
//   hex_buffer[50] = 0;
//   for (size_t i = 0; i < buffer.size(); i++) {
//     snprintf(&hex_buffer[3 * (i % 16)], sizeof(hex_buffer), "%02X ", buffer[i]);
//     if (i % 16 == 15)
//       ESP_LOGI(TAG, "   %s", hex_buffer);
//   }
//   if (buffer.size() % 16) {
//     // null terminate if incomplete line
//     hex_buffer[3 * (buffer.size() % 16) + 2] = 0;
//     ESP_LOGI(TAG, "   %s", hex_buffer);
//   }
// }

// /// @brief test the write_array method
// void WK2132Channel::uart_send_test_(char *message) {
//   auto start_exec = micros();
//   std::vector<uint8_t> output_buffer(XFER_MAX_SIZE);
//   generate(output_buffer.begin(), output_buffer.end(), Increment());  // fill with incrementing number
//   this->write_array(&output_buffer[0], XFER_MAX_SIZE);                // we send the buffer
//   ESP_LOGV(TAG, "%s => sent %d bytes - exec time %d µs ...", message, XFER_MAX_SIZE, micros() - start_exec);
// }

// /// @brief test read_array method
// bool WK2132Channel::uart_receive_test_(char *message) {
//   auto start_exec = micros();
//   bool status = true;
//   size_t received;
//   std::vector<uint8_t> buffer(XFER_MAX_SIZE);

//   // we wait until we have received all the bytes
//   uint32_t const start_time = millis();
//   while (XFER_MAX_SIZE > (received = this->available())) {
//     this->xfer_fifo_to_buffer_();
//     if (millis() - start_time > 100) {
//       ESP_LOGE(TAG, "uart_receive_test_() timeout: only %d bytes received...", received);
//       break;
//     }
//     yield();  // reschedule our thread to avoid blocking
//   }

//   uint8_t peek_value = 0;
//   this->peek_byte(&peek_value);
//   if (peek_value != 0) {
//     ESP_LOGE(TAG, "Peek first byte value error...");
//     print_buffer(buffer);
//     status = false;
//   }

//   status = this->read_array(&buffer[0], XFER_MAX_SIZE) && status;
//   for (size_t i = 0; i < XFER_MAX_SIZE; i++) {
//     if (buffer[i] != i) {
//       ESP_LOGE(TAG, "Read buffer contains error...");
//       print_buffer(buffer);
//       status = false;
//       break;
//     }
//   }

//   ESP_LOGV(TAG, "%s => received %d bytes  status %s - exec time %d µs ...", message, received, status ? "OK" :
//   "ERROR",
//            micros() - start_exec);
//   return status;
// }

// /// @}
// #endif

}  // namespace wk2132_i2c
}  // namespace esphome
