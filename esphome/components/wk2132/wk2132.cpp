/// @file wk2132.cpp
/// @author @DrCoolzic
/// @brief wk2132 implementation

#include "wk2132.h"

namespace esphome {
namespace wk2132 {

static const char *const TAG = "wk2132";

static const char *const REG_TO_STR_P0[] = {"GENA", "GRST", "GMUT",  "SPAGE", "SCR", "LCR", "FCR",
                                            "SIER", "SIFR", "TFCNT", "RFCNT", "FSR", "LSR", "FDAT"};
static const char *const REG_TO_STR_P1[] = {"GENA", "GRST", "GMUT",  "SPAGE", "BAUD1", "BAUD0", "PRES",
                                            "RFTL", "TFTL", "_INV_", "_INV_", "_INV_", "_INV_"};

// convert an int to binary string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
#define I2CS(val) (i2s(val).c_str())

/// @brief Computes the I²C Address to access the component
/// @param base_address the base address of the component as set by the A1 A0 pins
/// @param channel (0-3) the UART channel
/// @param fifo (0-1) if 0 access to internal register, if 1 direct access to fifo
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

/// @brief Converts the parity enum to a string
/// @param parity enum
/// @return the string
const char *parity2string(uart::UARTParityOptions parity) {
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
// The WK2132Component methods
///////////////////////////////////////////////////////////////////////////////

// method to print registers as text: used in log messages ...
const char *WK2132Component::reg_to_str_(int val) { return this->page1_ ? REG_TO_STR_P1[val] : REG_TO_STR_P0[val]; }

void WK2132Component::write_wk2132_register_(uint8_t reg_number, uint8_t channel, const uint8_t *buffer, size_t len) {
  this->address_ = i2c_address(this->base_address_, channel, 0);  // update the I²C address
  auto error = this->write_register(reg_number, buffer, len);     // write to the register
  if (error == i2c::ERROR_OK) {
    this->status_clear_warning();
    ESP_LOGVV(TAG, "write_wk2132_register_(@%02X %s, ch=%d b=%02X [%s], len=%d): I2C code %d", this->address_,
              this->reg_to_str_(reg_number), channel, *buffer, I2CS(*buffer), len, (int) error);
  } else {  // error
    this->status_set_warning();
    ESP_LOGE(TAG, "write_wk2132_register_(@%02X %s, ch=%d b=%02X [%s], len=%d): I2C code %d", this->address_,
             this->reg_to_str_(reg_number), channel, *buffer, I2CS(*buffer), len, (int) error);
  }
}

uint8_t WK2132Component::read_wk2132_register_(uint8_t reg_number, uint8_t channel, uint8_t *buffer, size_t len) {
  this->address_ = i2c_address(this->base_address_, channel, 0);  // update the i2c address
  auto error = this->read_register(reg_number, buffer, len);
  if (error == i2c::ERROR_OK) {
    this->status_clear_warning();
    ESP_LOGVV(TAG, "read_wk2132_register_(@%02X %s, ch=%d b=%02X [%s], len=%d): I2C code %d", this->address_,
              this->reg_to_str_(reg_number), channel, *buffer, I2CS(*buffer), len, (int) error);
  } else {  // error
    this->status_set_warning();
    ESP_LOGE(TAG, "read_wk2132_register_(@%02X %s, ch=%d b=%02X [%s], len=%d): I2C code %d", this->address_,
             this->reg_to_str_(reg_number), channel, *buffer, I2CS(*buffer), len, (int) error);
  }
  return *buffer;
}

//
// overloaded methods from Component
//

void WK2132Component::setup() {
  this->base_address_ = this->address_;  // TODO should not be necessary done in the ctor
  ESP_LOGCONFIG(TAG, "Setting up WK2132:@%02X with %d UARTs...", this->get_num_(), (int) this->children_.size());
  // we setup our children
  for (auto *child : this->children_)
    child->setup_channel_();
}

void WK2132Component::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of configuration WK2132:@%02X with %d UARTs completed", this->get_num_(),
                (int) this->children_.size());
  ESP_LOGCONFIG(TAG, "  crystal %d", this->crystal_);
  int tm = this->test_mode_.to_ulong();
  ESP_LOGCONFIG(TAG, "  test_mode %d", tm);
  LOG_I2C_DEVICE(this);

  for (auto i = 0; i < this->children_.size(); i++) {
    ESP_LOGCONFIG(TAG, "  UART @%02X:%d...", this->get_num_(), i);
    ESP_LOGCONFIG(TAG, "    baudrate %d Bd", this->children_[i]->baud_rate_);
    ESP_LOGCONFIG(TAG, "    data_bits %d", this->children_[i]->data_bits_);
    ESP_LOGCONFIG(TAG, "    stop_bits %d", this->children_[i]->stop_bits_);
    ESP_LOGCONFIG(TAG, "    parity %s", parity2string(this->children_[i]->parity_));
  }
  this->initialized_ = true;  // we can safely use the wk2132
}

///////////////////////////////////////////////////////////////////////////////
// The WK2132Channel methods
///////////////////////////////////////////////////////////////////////////////

void WK2132Channel::setup_channel_() {
  ESP_LOGCONFIG(TAG, "  Setting up UART @%02X:%d...", this->parent_->get_num_(), this->channel_);

  // we first do the global register (common to both channel)

  //  GENA description of global control register:
  //  * -------------------------------------------------------------------------
  //  * |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
  //  * -------------------------------------------------------------------------
  //  * |   M1   |   M0   |              RESERVED             |  UT2EN |  UT1EN |
  //  * -------------------------------------------------------------------------
  uint8_t gena;
  this->parent_->read_wk2132_register_(REG_WK2132_GENA, 0, &gena, 1);
  (this->channel_ == 0) ? gena |= 0x01 : gena |= 0x02;
  this->parent_->write_wk2132_register_(REG_WK2132_GENA, 0, &gena, 1);

  //  GRST description of global reset register:
  //  * -------------------------------------------------------------------------
  //  * |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
  //  * -------------------------------------------------------------------------
  //  * |       RSV       | UT2SLE | UT1SLE |       RSV       | UT2RST | UT1RST |
  //  * -------------------------------------------------------------------------
  // software reset UART channels
  uint8_t grst = 0;
  this->parent_->read_wk2132_register_(REG_WK2132_GRST, 0, &gena, 1);
  (this->channel_ == 0) ? grst |= 0x01 : grst |= 0x02;
  this->parent_->write_wk2132_register_(REG_WK2132_GRST, 0, &grst, 1);

  // now we initialize the channel register
  uint8_t const page = 0;
  this->parent_->page1_ = false;
  this->parent_->write_wk2132_register_(REG_WK2132_SPAGE, channel_, &page, 1);

  // FCR description of UART FIFO control register:
  // -------------------------------------------------------------------------
  // |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
  // -------------------------------------------------------------------------
  // |      TFTRIG     |      RFTRIG     |  TFEN  |  RFEN  |  TFRST |  RFRST |
  // -------------------------------------------------------------------------
  uint8_t const fcr = 0x0F;  // 0000 1111 reset fifo and enable the two fifo ...
  this->parent_->write_wk2132_register_(REG_WK2132_FCR, this->channel_, &fcr, 1);

  // SCR description of UART control register:
  //  -------------------------------------------------------------------------
  //  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
  //  -------------------------------------------------------------------------
  //  |                     RSV                    | SLEEPEN|  TXEN  |  RXEN  |
  //  -------------------------------------------------------------------------
  uint8_t const scr = 0x3;  // 0000 0011 enable receive and transmit
  this->parent_->write_wk2132_register_(REG_WK2132_SCR, this->channel_, &scr, 1);

  this->set_baudrate_();
  this->set_line_param_();
}

void WK2132Channel::set_baudrate_() {
  uint16_t const val_int = this->parent_->crystal_ / (this->baud_rate_ * 16) - 1;
  uint16_t val_dec = (this->parent_->crystal_ % (this->baud_rate_ * 16)) / (this->baud_rate_ * 16);
  uint8_t const baud_high = (uint8_t) (val_int >> 8);
  uint8_t const baud_low = (uint8_t) (val_int & 0xFF);
  while (val_dec > 0x0A)
    val_dec /= 0x0A;
  uint8_t const baud_dec = (uint8_t) (val_dec);

  uint8_t page = 1;  // switch to page 1
  this->parent_->write_wk2132_register_(REG_WK2132_SPAGE, this->channel_, &page, 1);
  this->parent_->page1_ = true;
  this->parent_->write_wk2132_register_(REG_WK2132_BRH, this->channel_, &baud_high, 1);
  this->parent_->write_wk2132_register_(REG_WK2132_BRL, this->channel_, &baud_low, 1);
  this->parent_->write_wk2132_register_(REG_WK2132_BRD, this->channel_, &baud_dec, 1);
  page = 0;  // switch back to page 0
  this->parent_->write_wk2132_register_(REG_WK2132_SPAGE, this->channel_, &page, 1);
  this->parent_->page1_ = false;

  ESP_LOGCONFIG(TAG, "  Crystal=%d baudrate=%d => registers [%d %d %d]", this->parent_->crystal_, this->baud_rate_,
                baud_high, baud_low, baud_dec);
}

void WK2132Channel::set_line_param_() {
  this->data_bits_ = 8;  // always 8 for WK2132
  uint8_t lcr;
  this->parent_->read_wk2132_register_(REG_WK2132_LCR, this->channel_, &lcr, 1);
  // LCR description of line configuration register:
  //  -------------------------------------------------------------------------
  //  |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
  //  -------------------------------------------------------------------------
  //  |        RSV      |  BREAK |  IREN  |  PAEN  |      PAM        |  STPL  |
  //  -------------------------------------------------------------------------
  lcr &= 0xF0;  // Clear the lower 4 bit of LCR
  if (this->stop_bits_ == 2)
    lcr |= 0x01;  // 0001

  switch (this->parity_) {              // parity selection settings
    case uart::UART_CONFIG_PARITY_ODD:  // odd parity
      lcr |= 0x5 << 1;                  // 101x
      break;
    case uart::UART_CONFIG_PARITY_EVEN:  // even parity
      lcr |= 0x6 << 1;                   // 110x
      break;
    default:
      break;  // no parity 000x
  }
  this->parent_->write_wk2132_register_(REG_WK2132_LCR, this->channel_, &lcr, 1);
  ESP_LOGCONFIG(TAG, "  line config: %d data_bits, %d stop_bits, parity %s register [%s]", this->data_bits_,
                this->stop_bits_, parity2string(this->parity_), I2CS(lcr));
}

size_t WK2132Channel::tx_in_fifo_() {
  // FSR description of line configuration register:
  //  * -------------------------------------------------------------------------
  //  * |   b7   |   b6   |   b5   |   b4   |   b3   |   b2   |   b1   |   b0   |
  //  * -------------------------------------------------------------------------
  //  * |  RFOE  |  RFBI  |  RFFE  |  RFPE  |  RDAT  |  TDAT  |  TFULL |  TBUSY |
  //  * -------------------------------------------------------------------------
  // WARNING:
  // The received buffer can hold 256 bytes. However, as the RFCNT reg is 8 bits,
  // in this case the value 256 is reported as 0 ! Therefore the RFCNT count can be
  // o when there is 0 byte or 256 bytes in the buffer. So if we have RXDAT = 1
  // and RFCNT = 0 it should be interpreted as there is 256 bytes in the FIFO.
  // Note that in case of overflow the RFOE goes to one **but** as soon as you read
  // the FSR this bit is cleared. Therefore Overflow can be read only once even if
  // still in overflow.
  //
  // The same remark is valid for the transmit buffer but here we have to check the
  // TFULL flag. So if TFULL is set and TFCNT is 0 this should be interpreted as 256
  //
  size_t tfcnt = this->parent_->read_wk2132_register_(REG_WK2132_TFCNT, this->channel_, &this->data_, 1);
  uint8_t const fsr = this->parent_->read_wk2132_register_(REG_WK2132_FSR, this->channel_, &this->data_, 1);
  if ((fsr & 0x02) && (tfcnt == 0)) {
    ESP_LOGVV(TAG, "tx_in_fifo overflow (256) FSR=%s", I2CS(fsr));
    tfcnt = 256;
  }
  return tfcnt;
}

size_t WK2132Channel::rx_in_fifo_() {
  size_t available = 0;

  available = this->parent_->read_wk2132_register_(REG_WK2132_RFCNT, this->channel_, &this->data_, 1);
  if (available == 0) {
    uint8_t const fsr = this->parent_->read_wk2132_register_(REG_WK2132_FSR, this->channel_, &this->data_, 1);
    if (fsr & 0x8) {  // if RDAT bit is set we set available to 256
      ESP_LOGVV(TAG, "rx_in_fifo overflow (256) FSR=%s", I2CS(fsr));
      available = 256;
    }
  }

  if (available > MAX_SIZE)  // no more than what is set in the fifo_size
    available = MAX_SIZE;

  ESP_LOGVV(TAG, "rx_in_fifo %d", available);
  return available;
}

bool WK2132Channel::read_data_(uint8_t *buffer, size_t len) {
  this->parent_->address_ = i2c_address(this->parent_->base_address_, this->channel_, 1);  // set fifo flag
  // With the WK2132 we need to read data directly from the fifo buffer without passing through a register
  // note: that theoretically it should be possible to read through the REG_WK2132_FDA register
  // but beware that it does not work !
  auto error = this->parent_->read(buffer, len);
  if (error == i2c::ERROR_OK) {
    this->parent_->status_clear_warning();
    ESP_LOGV(TAG, "read_data(ch=%d buffer[0]=%02X [%s], len=%d): I2C code %d", this->channel_, *buffer, I2CS(*buffer),
             len, (int) error);
    return true;
  } else {  // error
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "read_data(ch=%d buffer[0]=%02X [%s], len=%d): I2C code %d", this->channel_, *buffer, I2CS(*buffer),
             len, (int) error);
    return false;
  }
}

bool WK2132Channel::write_data_(const uint8_t *buffer, size_t len) {
  this->parent_->address_ = i2c_address(this->parent_->base_address_, this->channel_, 1);  // set fifo flag

  // With the WK2132 we need to write to the fifo buffer without passing through a register
  // note: that theoretically it should be possible to write through the REG_WK2132_FDA register
  // but beware that it does not seems to work !
  auto error = this->parent_->write(buffer, len);
  if (error == i2c::ERROR_OK) {
    this->parent_->status_clear_warning();
    ESP_LOGV(TAG, "write_data(ch=%d buffer[0]=%02X [%s], len=%d): I2C code %d", this->channel_, *buffer, I2CS(*buffer),
             len, (int) error);
    return true;
  } else {  // error
    this->parent_->status_set_warning();
    ESP_LOGE(TAG, "write_data(ch=%d buffer[0]=%02X [%s], len=%d): I2C code %d", this->channel_, *buffer, I2CS(*buffer),
             len, (int) error);
    return false;
  }
}

bool WK2132Channel::read_array(uint8_t *buffer, size_t len) {
  if (len > MAX_SIZE) {
    ESP_LOGE(TAG, "Read buffer invalid call: requested %d bytes max size %d ...", len, MAX_SIZE);
    return false;
  }
  auto available = this->receive_buffer_.count();
  if (len > available) {
    ESP_LOGE(TAG, "read_array buffer underflow requested %d bytes available %d ...", len, available);
    len = available;
  }
  for (size_t i = 0; i < len; i++) {
    this->receive_buffer_.pop(buffer[i]);
  }
  return true;
}

void WK2132Channel::write_array(const uint8_t *buffer, size_t len) {
  if (len > MAX_SIZE) {
    ESP_LOGE(TAG, "Write buffer invalid call: requested %d bytes max size %d ...", len, MAX_SIZE);
    len = MAX_SIZE;
  }
  this->write_data_(buffer, len);
}

void WK2132Channel::flush() {
  uint32_t const start_time = millis();
  while (this->tx_in_fifo_()) {  // wait until buffer empty
    if (millis() - start_time > 100) {
      ESP_LOGE(TAG, "Flush timed out: still %d bytes not sent...", this->tx_in_fifo_());
      return;
    }
    yield();  // reschedule thread to avoid blocking
  }
}

///////////////////////////////////////////////////////////////////////////////
/// AUTOTEST FUNCTIONS BELOW
///////////////////////////////////////////////////////////////////////////////
#ifdef AUTOTEST_COMPONENT

class Increment {  // A "Functor" (A class object that acts like a method with state!)
 public:
  Increment() : i_(0) {}
  uint8_t operator()() { return i_++; }

 private:
  uint8_t i_;
};

void print_buffer(std::vector<uint8_t> buffer) {
  // quick and ugly hex converter to display buffer in hex format
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
void WK2132Channel::uart_send_test_(char *preamble) {
  auto start_exec = millis();
  // we send the maximum possible
  this->flush();
  size_t const to_send = MAX_SIZE - tx_in_fifo_();
  if (to_send > 0) {
    std::vector<uint8_t> output_buffer(to_send);
    generate(output_buffer.begin(), output_buffer.end(), Increment());  // fill with incrementing number
    output_buffer[0] = to_send;                     // we send as the first byte the length of the buffer
    this->write_array(&output_buffer[0], to_send);  // we send the buffer
    this->flush();                                  // we wait until they are gone
    ESP_LOGI(TAG, "%s => sending  %d bytes - exec time %d ms ...", preamble, to_send, millis() - start_exec);
  }
}

/// @brief test read_array method
void WK2132Channel::uart_receive_test_(char *preamble, bool print_buf) {
  auto start_exec = millis();
  bool status = false;
  uint8_t const to_read = this->available();
  if (to_read > 0) {
    std::vector<uint8_t> buffer(to_read);
    status = this->read_array(&buffer[0], to_read);
    if (print_buf)
      print_buffer(buffer);
  }
  ESP_LOGI(TAG, "%s => received %d bytes - exec time %d ms ...", preamble, to_read, millis() - start_exec);
}

#endif

void WK2132Component::loop() {
  // if the component is not fully initialized we return
  if (!this->initialized_)
    return;

  // here we transfer from fifo to ring buffer
  for (auto *child : this->children_) {
    uint8_t data[RING_BUFFER_SIZE];
    // we look if some characters has been received in the fifo
    if (auto to_transfer = child->rx_in_fifo_()) {
      child->read_data_(data, to_transfer);
      auto free = child->receive_buffer_.free();
      if (to_transfer > free) {
        ESP_LOGV(TAG, "Ring buffer full --> requested %d available %d", to_transfer, free);
        to_transfer = free;  // hopefully will do the rest next time
      }
      ESP_LOGV(TAG, "Transferred %d bytes from rx_fifo to buffer ring", to_transfer);
      for (size_t i = 0; i < to_transfer; i++)
        child->receive_buffer_.push(data[i]);
    }
  }

#ifdef AUTOTEST_COMPONENT
  //
  // This loop is used only if the wk2132 component is in test mode
  //
  static uint16_t loop_calls = 0;

  if (this->test_mode_.test(0)) {  // test echo mode (bit 0)
    for (auto *child : this->children_) {
      uint8_t data;
      if (child->available()) {
        child->read_byte(&data);
        ESP_LOGI(TAG, "echo mode: read -> send %02X", data);
        child->write_byte(data);
      }
    }
  }

  if (test_mode_.test(1)) {  // test loop mode (bit 1)
    static uint32_t loop_time = 0;
    ESP_LOGI(TAG, "loop %d : %d ms since last call ...", loop_calls++, millis() - loop_time);
    loop_time = millis();

    char preamble[64];
    for (size_t i = 0; i < this->children_.size(); i++) {
      snprintf(preamble, sizeof(preamble), "WK2132@%02X:%d", this->get_num_(), i);
      this->children_[i]->uart_receive_test_(preamble);
      this->children_[i]->uart_send_test_(preamble);
    }
    ESP_LOGI(TAG, "loop execution time %d ms...", millis() - loop_time);
  }
#endif
}

}  // namespace wk2132
}  // namespace esphome
