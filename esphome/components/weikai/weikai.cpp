/// @file weikai.cpp
/// @author DrCoolzic
/// @brief weikai classes implementation

#include "weikai.h"

namespace esphome {
namespace weikai {

/*! @mainpage Weikai source code documentation
 This documentation provides information about the implementation of the family of WeiKai Components in ESPHome.

  @section WKRingBuffer_ The RingBuffer template class
The RingBuffer template class has it names implies implement a simple ring buffer helper class. This straightforward
container implements FIFO functionality, enabling bytes to be pushed into one side and popped from the other in the
order of entry. Implementation is classic and therefore not described in any details.

  @section WKBaseRegister_ The WeikaiRegister class
 The WeikaiRegister helper class creates objects that act as proxies to device register.
 @details This is an abstract virtual class (interface) that provides all the necessary access to registers while hiding
 the actual implementation. The access to the registers can be made through an I²C bus in for example for wk2168_i2c
 component or through a SPI bus for example in the case of the wk2168_spi component. Derived classes will actually
 performs the specific bus operations.

 @section WKBaseRegI2C_ WeikaiRegisterI2C
 The WeikaiRegisterI2C class implements the virtual methods of the WeikaiRegister virtual class for an I2C bus.

  @section WKBaseRegSPI_ WKBaseSPII2C
 The WeikaiRegisterSPI class implements the virtual methods of the WeikaiRegister virtual class for an SPI bus.

 @section WKBaseComponent_ The WeikaiComponent class
The WeikaiComponent class stores the information global to a WK family component and provides methods to set/access
this information. It also serves as a container for WeikaiChannel instances. This is done by maintaining an array of
references these WeikaiChannel instances. This class derives from two ESPHome classes. This class override
esphome::Component::loop() method which is to facilitate the seamless transfer of accumulated bytes from the receive
FIFO into the ring buffer. This process ensures quick access to the stored bytes, enhancing the overall efficiency of
the component.

 @section WKGPIOPin_ WeikaiGPIOPin class
 The WeikaiGPIOPin class is an helper class to expose the GPIO pins of WK family components as if they were internal
GPIO pins. It also provides the setup() and dump_summary() methods.

 @section WKBaseChannel_ The WeikaiChannel class
The WeikaiChannel class is used to implement all the virtual methods of the ESPHome uart::UARTComponent class. An
individual instance of this class is created for each UART channel. It has a link back to the WeikaiComponent object it
belongs to. This class derives from the uart::UARTComponent class. It collaborates through an aggregation with
WeikaiComponent. This implies that WeikaiComponent acts as a container, housing several WeikaiChannel instances.
Furthermore, the WeikaiChannel class derives from the ESPHome uart::UARTComponent class, it also has an association
relationship with the RingBuffer and WeikaiRegister helper classes. Consequently, when a WeikaiChannel instance is
destroyed, the associated RingBuffer instance is also destroyed.
*/

static const char *const TAG = "weikai";

/// @brief convert an int to binary representation as C++ std::string
/// @param val integer to convert
/// @return a std::string
inline std::string i2s(uint8_t val) { return std::bitset<8>(val).to_string(); }
/// Convert std::string to C string
#define I2S2CS(val) (i2s(val).c_str())

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

/// @brief Display a buffer in hexadecimal format (32 hex values / line).
void print_buffer(const uint8_t *data, size_t length) {
  char hex_buffer[100];
  hex_buffer[(3 * 32) + 1] = 0;
  for (size_t i = 0; i < length; i++) {
    snprintf(&hex_buffer[3 * (i % 32)], sizeof(hex_buffer), "%02X ", data[i]);
    if (i % 32 == 31) {
      ESP_LOGVV(TAG, "   %s", hex_buffer);
    }
  }
  if (length % 32) {
    // null terminate if incomplete line
    hex_buffer[3 * (length % 32) + 2] = 0;
    ESP_LOGVV(TAG, "   %s", hex_buffer);
  }
}

static const char *const REG_TO_STR_P0[16] = {"GENA", "GRST",  "GMUT",  "SPAGE", "SCR", "LCR",  "FCR",  "SIER",
                                              "SIFR", "TFCNT", "RFCNT", "FSR",   "LSR", "FDAT", "FWCR", "RS485"};
static const char *const REG_TO_STR_P1[16] = {"GENA", "GRST", "GMUT", "SPAGE", "BAUD1", "BAUD0", "PRES", "RFTL",
                                              "TFTL", "FWTH", "FWTL", "XON1",  "XOFF1", "SADR",  "SAEN", "RTSDLY"};

// method to print a register value as text: used in the log messages ...
const char *reg_to_str(int reg, bool page1) {
  if (reg == WKREG_GPDAT) {
    return "GPDAT";
  } else if (reg == WKREG_GPDIR) {
    return "GPDIR";
  } else {
    return page1 ? REG_TO_STR_P1[reg & 0x0F] : REG_TO_STR_P0[reg & 0x0F];
  }
}
enum RegType { REG = 0, FIFO = 1 };  ///< Register or FIFO

#ifdef USE_SPI_BUS
enum CmdType { WRITE_CMD = 0, READ_CMD = 1 };  ///< Read or Write transfer

/// @brief Computes the SPI command byte
/// @param transfer_type read or write command
/// @param reg (0-15) the address of the register
/// @param channel (0-3) the UART channel
/// @param fifo (0-1) 0 = access to internal register, 1 = direct access to fifo
/// @return the spi command byte
/// @details
/// +------+------+------+------+------+------+------+------+
/// | FIFO | R/W  |    C1-C0    |           A3-A0           |
/// +------+------+-------------+---------------------------+
/// FIFO: 0 = register, 1 = FIFO
/// R/W: 0 = write, 1 = read
/// C1-C0: Channel (0-1)
/// A3-A0: Address (0-F)
inline static uint8_t cmd_byte(RegType fifo, CmdType transfer_type, uint8_t channel, uint8_t reg) {
  return (fifo << 7 | transfer_type << 6 | channel << 4 | reg << 0);
}
#endif

#ifdef USE_I2C_BUS
/// @brief Computes the I²C bus's address used to access the component
/// @param base_address the base address of the component - set by the A1 A0 pins
/// @param channel (0-3) the UART channel
/// @param fifo (0-1) 0 = access to internal register, 1 = direct access to fifo
/// @return the i2c address to use
inline uint8_t i2c_address(uint8_t base_address, uint8_t channel, RegType fifo) {
  // the address of the device is:
  // +----+----+----+----+----+----+----+----+
  // |  0 | A1 | A0 |  1 |  0 | C1 | C0 |  F |
  // +----+----+----+----+----+----+----+----+
  // where:
  // - A1,A0 is the address read from A1,A0 switch
  // - C1,C0 is the channel number (in practice only 00 or 01)
  // - F is: 0 when accessing register, one when accessing FIFO
  uint8_t const addr = base_address | channel << 1 | fifo << 0;
  return addr;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// The WeikaiRegister methods
///////////////////////////////////////////////////////////////////////////////
WeikaiRegister &WeikaiRegister::operator=(uint8_t value) {
  write_reg(value);
  return *this;
}

WeikaiRegister &WeikaiRegister::operator&=(uint8_t value) {
  value &= read_reg();
  write_reg(value);
  return *this;
}

WeikaiRegister &WeikaiRegister::operator|=(uint8_t value) {
  value |= read_reg();
  write_reg(value);
  return *this;
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiComponent methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiComponent::loop() {
  if ((this->component_state_ & COMPONENT_STATE_MASK) != COMPONENT_STATE_LOOP)
    return;

  // If there are some bytes in the receive FIFO we transfers them to the ring buffers
  size_t transferred = 0;
  for (auto *child : this->children_) {
    // we look if some characters has been received in the fifo
    transferred += child->xfer_fifo_to_buffer_();
  }
  if (transferred > 0) {
    ESP_LOGV(TAG, "we transferred %d bytes from fifo to buffer...", transferred);
  }

#ifdef TEST_COMPONENT
  static uint32_t loop_time = 0;
  static uint32_t loop_count = 0;
  uint32_t time = 0;

  if (test_mode_ == 1) {  // test component in loopback
    ESP_LOGI(TAG, "Component loop %d for %s : %d ms since last call ...", loop_count++, this->get_name(),
             millis() - loop_time);
    loop_time = millis();
    char message[64];
    elapsed_ms(time);  // set time to now
    for (int i = 0; i < this->children_.size(); i++) {
      if (i != ((loop_count - 1) % this->children_.size()))  // we do only one per loop
        continue;
      snprintf(message, sizeof(message), "%s:%s", this->get_name(), children_[i]->get_channel_name());
      children_[i]->uart_send_test_(message);
      uint32_t const start_time = millis();
      while (children_[i]->tx_fifo_is_not_empty_()) {  // wait until buffer empty
        if (millis() - start_time > 1500) {
          ESP_LOGE(TAG, "timeout while flushing - %d bytes left in buffer...", children_[i]->tx_in_fifo_());
          break;
        }
        yield();  // reschedule our thread to avoid blocking
      }
      bool status = children_[i]->uart_receive_test_(message);
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
#ifdef HAS_GPIO_PIN
  if (test_mode_ == 3) {
    test_gpio_input_();
  }

  if (test_mode_ == 4) {
    test_gpio_output_();
  }
#endif
#endif
}

#if defined(TEST_COMPONENT) && defined(HAS_GPIO_PIN)
void WeikaiComponent::test_gpio_input_() {
  static bool init_input{false};
  static uint8_t state{0};
  uint8_t value;
  if (!init_input) {
    init_input = true;
    // set all pins in input mode
    this->reg(WKREG_GPDIR, 0) = 0x00;
    ESP_LOGI(TAG, "initializing all pins to input mode");
    state = this->reg(WKREG_GPDAT, 0);
    ESP_LOGI(TAG, "initial input data state = %02X (%s)", state, I2S2CS(state));
  }
  value = this->reg(WKREG_GPDAT, 0);
  if (value != state) {
    ESP_LOGI(TAG, "Input data changed from %02X to %02X (%s)", state, value, I2S2CS(value));
    state = value;
  }
}

void WeikaiComponent::test_gpio_output_() {
  static bool init_output{false};
  static uint8_t state{0};
  if (!init_output) {
    init_output = true;
    // set all pins in output mode
    this->reg(WKREG_GPDIR, 0) = 0xFF;
    ESP_LOGI(TAG, "initializing all pins to output mode");
    this->reg(WKREG_GPDAT, 0) = state;
    ESP_LOGI(TAG, "setting all outputs to 0");
  }
  state = ~state;
  this->reg(WKREG_GPDAT, 0) = state;
  ESP_LOGI(TAG, "Flipping all outputs to %02X (%s)", state, I2S2CS(state));
  delay(100);  // NOLINT
}
#endif

#ifdef HAS_GPIO_PIN
///////////////////////////////////////////////////////////////////////////////
// The WeikaiGPIOPin methods
///////////////////////////////////////////////////////////////////////////////
bool WeikaiComponent::read_pin_val_(uint8_t pin) {
  this->input_state_ = this->reg(WKREG_GPDAT, 0);
  ESP_LOGVV(TAG, "reading input pin %d = %d in_state %s", pin, this->input_state_ & (1 << pin), I2S2CS(input_state_));
  return this->input_state_ & (1 << pin);
}

void WeikaiComponent::write_pin_val_(uint8_t pin, bool value) {
  if (value) {
    this->output_state_ |= (1 << pin);
  } else {
    this->output_state_ &= ~(1 << pin);
  }
  ESP_LOGVV(TAG, "writing output pin %d with %d out_state %s", pin, value, I2S2CS(this->output_state_));
  this->reg(WKREG_GPDAT, 0) = this->output_state_;
}

void WeikaiComponent::set_pin_direction_(uint8_t pin, gpio::Flags flags) {
  if (flags == gpio::FLAG_INPUT) {
    this->pin_config_ &= ~(1 << pin);  // clear bit (input mode)
  } else {
    if (flags == gpio::FLAG_OUTPUT) {
      this->pin_config_ |= 1 << pin;  // set bit (output mode)
    } else {
      ESP_LOGE(TAG, "pin %d direction invalid", pin);
    }
  }
  ESP_LOGVV(TAG, "setting pin %d direction to %d pin_config=%s", pin, flags, I2S2CS(this->pin_config_));
  this->reg(WKREG_GPDIR, 0) = this->pin_config_;  // TODO check ~
}

void WeikaiGPIOPin::setup() {
  ESP_LOGCONFIG(TAG, "Setting GPIO pin %d mode to %s", this->pin_,
                flags_ == gpio::FLAG_INPUT          ? "Input"
                : this->flags_ == gpio::FLAG_OUTPUT ? "Output"
                                                    : "NOT SPECIFIED");
  // ESP_LOGCONFIG(TAG, "Setting GPIO pins mode to '%s' %02X", I2S2CS(this->flags_), this->flags_);
  this->pin_mode(this->flags_);
}

std::string WeikaiGPIOPin::dump_summary() const {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%u via WK2168 %s", this->pin_, this->parent_->get_name());
  return buffer;
}
#endif

#ifdef USE_I2C_BUS
///////////////////////////////////////////////////////////////////////////////
// The WeikaiRegisterI2C methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WeikaiRegisterI2C::read_reg() const {
  uint8_t value = 0x00;
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  comp_i2c->address_ = i2c_address(comp_i2c->base_address_, this->channel_, REG);  // update the i2c bus address
  auto error = comp_i2c->read_register(this->register_, &value, 1);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WeikaiRegisterI2C::read_reg() @%02X reg=%s ch=%d I2C_code:%d, buf=%02X", comp_i2c->address_,
              reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WeikaiRegisterI2C::read_reg() @%02X reg=%s ch=%d I2C_code:%d, buf=%02X", comp_i2c->address_,
             reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
  }
  return value;
}

void WeikaiRegisterI2C::read_fifo(uint8_t *data, size_t length) const {
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  comp_i2c->address_ = i2c_address(comp_i2c->base_address_, this->channel_, FIFO);
  auto error = comp_i2c->read(data, length);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "WeikaiRegisterI2C::read_fifo() @%02X ch=%d I2C_code:%d len=%d buffer", comp_i2c->address_,
              this->channel_, (int) error, length);
    print_buffer(data, length);
#endif
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WeikaiRegisterI2C::read_fifo() @%02X reg=N/A ch=%d I2C_code:%d len=%d buf=%02X...",
             comp_i2c->address_, this->channel_, (int) error, length, data[0]);
  }
}

void WeikaiRegisterI2C::write_reg(uint8_t value) {
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  comp_i2c->address_ = i2c_address(comp_i2c->base_address_, this->channel_, REG);  // update the i2c bus
  auto error = comp_i2c->write_register(this->register_, &value, 1);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
    ESP_LOGVV(TAG, "WK2168Reg::write_reg() @%02X reg=%s ch=%d I2C_code:%d buf=%02X", comp_i2c->address_,
              reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2168Reg::write_reg() @%02X reg=%s ch=%d I2C_code:%d buf=%d", comp_i2c->address_,
             reg_to_str(this->register_, comp_i2c->page1()), this->channel_, (int) error, value);
  }
}

void WeikaiRegisterI2C::write_fifo(uint8_t *data, size_t length) {
  WeikaiComponentI2C *comp_i2c = static_cast<WeikaiComponentI2C *>(this->comp_);
  comp_i2c->address_ = i2c_address(comp_i2c->base_address_, this->channel_, FIFO);  // set fifo flag
  auto error = comp_i2c->write(data, length);
  if (error == i2c::NO_ERROR) {
    this->comp_->status_clear_warning();
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "WK2168Reg::write_fifo() @%02X ch=%d I2C_code:%d len=%d buffer", comp_i2c->address_, this->channel_,
              (int) error, length);
    print_buffer(data, length);
#endif
  } else {  // error
    this->comp_->status_set_warning();
    ESP_LOGE(TAG, "WK2168Reg::write_fifo() @%02X reg=N/A, ch=%d I2C_code:%d len=%d, buf=%02X...", comp_i2c->address_,
             this->channel_, (int) error, length, data[0]);
  }
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiComponentI2C methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiComponentI2C::setup() {
  // before any manipulation we store the address to base_address_ for future use
  this->base_address_ = this->address_;
  ESP_LOGCONFIG(TAG, "Setting up wk2168_i2c: %s with %d UARTs at @%02X ...", this->get_name(), this->children_.size(),
                this->base_address_);

  // enable all channels
  this->reg(WKREG_GENA, 0) = GENA_C1EN | GENA_C2EN | GENA_C3EN | GENA_C4EN;
  // reset all channels
  this->reg(WKREG_GRST, 0) = GRST_C1RST | GRST_C2RST | GRST_C3RST | GRST_C4RST;
  // initialize the spage register to page 0
  this->reg(WKREG_SPAGE, 0) = 0;
  this->page1_ = false;

  // we setup our children channels
  for (auto *child : this->children_) {
    child->setup_channel();
  }
}

void WeikaiComponentI2C::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %d", this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  this->address_ = this->base_address_;  // we restore the base_address before display (less confusing)
  LOG_I2C_DEVICE(this);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}
#endif

#ifdef USE_SPI_BUS
///////////////////////////////////////////////////////////////////////////////
// The WeikaiRegisterSPI methods
///////////////////////////////////////////////////////////////////////////////
uint8_t WeikaiRegisterSPI::read_reg() const {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(REG, READ_CMD, this->channel_, this->register_);
  spi_comp->enable();
  spi_comp->write_byte(cmd);
  uint8_t val = spi_comp->read_byte();
  spi_comp->disable();
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::read_reg() cmd=%s(%02X) reg=%s ch=%d buf=%02X", I2S2CS(cmd), cmd,
            reg_to_str(this->register_, this->comp_->page1()), this->channel_, val);
  return val;
}

void WeikaiRegisterSPI::read_fifo(uint8_t *data, size_t length) const {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(FIFO, READ_CMD, this->channel_, this->register_);
  spi_comp->enable();
  spi_comp->write_byte(cmd);
  spi_comp->read_array(data, length);
  spi_comp->disable();
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::read_fifo() cmd=%s(%02X) ch=%d len=%d buffer", I2S2CS(cmd), cmd, this->channel_,
            length);
  print_buffer(data, length);
#endif
}

void WeikaiRegisterSPI::write_reg(uint8_t value) {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t buf[2]{cmd_byte(REG, WRITE_CMD, this->channel_, this->register_), value};
  spi_comp->enable();
  spi_comp->write_array(buf, 2);
  spi_comp->disable();
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::write_reg() cmd=%s(%02X) reg=%s ch=%d buf=%02X", I2S2CS(buf[0]), buf[0],
            reg_to_str(this->register_, this->comp_->page1()), this->channel_, buf[1]);
}

void WeikaiRegisterSPI::write_fifo(uint8_t *data, size_t length) {
  auto *spi_comp = static_cast<WeikaiComponentSPI *>(this->comp_);
  uint8_t cmd = cmd_byte(FIFO, WRITE_CMD, this->channel_, this->register_);
  spi_comp->enable();
  spi_comp->write_byte(cmd);
  spi_comp->write_array(data, length);
  spi_comp->disable();

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  ESP_LOGVV(TAG, "WeikaiRegisterSPI::write_fifo() cmd=%s(%02X) ch=%d len=%d buffer", I2S2CS(cmd), cmd, this->channel_,
            length);
  print_buffer(data, length);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// The WeikaiComponentSPI methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiComponentSPI::setup() {
  using namespace weikai;
  ESP_LOGCONFIG(TAG, "Setting up wk2168_spi: %s with %d UARTs...", this->get_name(), this->children_.size());
  this->spi_setup();
  // enable all channels
  this->reg(WKREG_GENA, 0) = GENA_C1EN | GENA_C2EN | GENA_C3EN | GENA_C4EN;
  // reset all channels
  this->reg(WKREG_GRST, 0) = GRST_C1RST | GRST_C2RST | GRST_C3RST | GRST_C4RST;
  // initialize the spage register to page 0
  this->reg(WKREG_SPAGE, 0) = 0;
  this->page1_ = false;

  // we setup our children channels
  for (auto *child : this->children_) {
    child->setup_channel();
  }
}

void WeikaiComponentSPI::dump_config() {
  ESP_LOGCONFIG(TAG, "Initialization of %s with %d UARTs completed", this->get_name(), this->children_.size());
  ESP_LOGCONFIG(TAG, "  Crystal: %d", this->crystal_);
  if (test_mode_)
    ESP_LOGCONFIG(TAG, "  Test mode: %d", test_mode_);
  LOG_PIN("  CS Pin: ", this->cs_);

  for (auto *child : this->children_) {
    child->dump_channel();
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////
// The WeikaiChannel methods
///////////////////////////////////////////////////////////////////////////////
void WeikaiChannel::setup_channel() {
  ESP_LOGCONFIG(TAG, "  Setting up UART %s:%s ...", this->parent_->get_name(), this->get_channel_name());
  // we enable transmit and receive on this channel
  if (this->check_channel_down()) {
    ESP_LOGCONFIG(TAG, "  Error channel %s not working...", this->get_channel_name());
  }
  this->reset_fifo_();
  this->receive_buffer_.clear();
  this->set_line_param_();
  this->set_baudrate_();
}

void WeikaiChannel::dump_channel() {
  ESP_LOGCONFIG(TAG, "  UART %s ...", this->get_channel_name());
  ESP_LOGCONFIG(TAG, "    Baud rate: %d Bd", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "    Data bits: %d", this->data_bits_);
  ESP_LOGCONFIG(TAG, "    Stop bits: %d", this->stop_bits_);
  ESP_LOGCONFIG(TAG, "    Parity: %s", p2s(this->parity_));
}

void WeikaiChannel::reset_fifo_() {
  // enable transmission and reception
  this->reg_(WKREG_SCR) = SCR_RXEN | SCR_TXEN;
  // we reset and enable transmit and receive FIFO
  this->reg_(WKREG_FCR) = FCR_TFEN | FCR_RFEN | FCR_TFRST | FCR_RFRST;
}

void WeikaiChannel::set_line_param_() {
  this->data_bits_ = 8;  // always equal to 8 for WK2168 (cant be changed)
  uint8_t lcr = 0;
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
  this->reg_(WKREG_LCR) = lcr;  // write LCR
  ESP_LOGV(TAG, "    line config: %d data_bits, %d stop_bits, parity %s register [%s]", this->data_bits_,
           this->stop_bits_, p2s(this->parity_), I2S2CS(lcr));
}

void WeikaiChannel::set_baudrate_() {
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

inline bool WeikaiChannel::tx_fifo_is_not_empty_() { return this->reg_(WKREG_FSR) & FSR_TFDAT; }

size_t WeikaiChannel::tx_in_fifo_() {
  size_t tfcnt = this->reg_(WKREG_TFCNT);
  if (tfcnt == 0) {
    uint8_t const fsr = this->reg_(WKREG_FSR);
    if (fsr & FSR_TFFULL) {
      ESP_LOGVV(TAG, "tx FIFO full FSR=%s", I2S2CS(fsr));
      tfcnt = FIFO_SIZE;
    }
  }
  ESP_LOGVV(TAG, "tx FIFO contains %d bytes", tfcnt);
  return tfcnt;
}

size_t WeikaiChannel::rx_in_fifo_() {
  size_t available = this->reg_(WKREG_RFCNT);
  uint8_t const fsr = this->reg_(WKREG_FSR);
  if (fsr & (FSR_RFOE | FSR_RFLB | FSR_RFFE | FSR_RFPE)) {
    if (fsr & FSR_RFOE)
      ESP_LOGE(TAG, "Receive data overflow FSR=%s", I2S2CS(fsr));
    if (fsr & FSR_RFLB)
      ESP_LOGE(TAG, "Receive line break FSR=%s", I2S2CS(fsr));
    if (fsr & FSR_RFFE)
      ESP_LOGE(TAG, "Receive frame error FSR=%s", I2S2CS(fsr));
    if (fsr & FSR_RFPE)
      ESP_LOGE(TAG, "Receive parity error FSR=%s", I2S2CS(fsr));
  }
  if ((available == 0) && (fsr & FSR_RFDAT)) {
    // here we should be very careful because we can have something like this:
    // -  at time t0 we read RFCNT=0 because nothing yet received
    // -  at time t0+delta we might read FIFO not empty because one byte has just been received
    // -  so to be sure we need to do another read of RFCNT and if it is still zero -> buffer full
    available = this->reg_(WKREG_RFCNT);
    if (available == 0) {  // still zero ?
      ESP_LOGV(TAG, "rx FIFO is full FSR=%s", I2S2CS(fsr));
      available = FIFO_SIZE;
    }
  }
  ESP_LOGVV(TAG, "rx FIFO contain %d bytes - FSR status=%s", available, I2S2CS(fsr));
  return available;
}

bool WeikaiChannel::check_channel_down() {
  // to check if we channel is up we write to the LCR W/R register
  // note that this will put a break on the tx line for few ms
  WeikaiRegister &lcr = this->reg_(WKREG_LCR);
  lcr = 0x3F;
  uint8_t val = lcr;
  if (val != 0x3F) {
    ESP_LOGE(TAG, "R/W of register failed expected 0x3F received 0x%02X", val);
    return true;
  }
  lcr = 0;
  val = lcr;
  if (val != 0x00) {
    ESP_LOGE(TAG, "R/W of register failed expected 0x00 received 0x%02X", val);
    return true;
  }
  return false;
}

bool WeikaiChannel::peek_byte(uint8_t *buffer) {
  auto available = this->receive_buffer_.count();
  if (!available)
    xfer_fifo_to_buffer_();
  return this->receive_buffer_.peek(*buffer);
}

int WeikaiChannel::available() {
  size_t available = this->receive_buffer_.count();
  if (!available)
    available = xfer_fifo_to_buffer_();
  return available;
}

bool WeikaiChannel::read_array(uint8_t *buffer, size_t length) {
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

void WeikaiChannel::write_array(const uint8_t *buffer, size_t length) {
  if (length > XFER_MAX_SIZE) {
    ESP_LOGE(TAG, "Write_array: invalid call - requested %d bytes but max size %d ...", length, XFER_MAX_SIZE);
    length = XFER_MAX_SIZE;
  }
  this->reg_(0).write_fifo(const_cast<uint8_t *>(buffer), length);
}

void WeikaiChannel::flush() {
  uint32_t const start_time = millis();
  while (this->tx_fifo_is_not_empty_()) {  // wait until buffer empty
    if (millis() - start_time > 200) {
      ESP_LOGW(TAG, "WARNING flush timeout - still %d bytes not sent after 200 ms...", this->tx_in_fifo_());
      return;
    }
    yield();  // reschedule our thread to avoid blocking
  }
}

size_t WeikaiChannel::xfer_fifo_to_buffer_() {
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
void WeikaiChannel::uart_send_test_(char *message) {
  auto start_exec = micros();
  std::vector<uint8_t> output_buffer(XFER_MAX_SIZE);
  generate(output_buffer.begin(), output_buffer.end(), Increment());  // fill with incrementing number
  this->write_array(&output_buffer[0], XFER_MAX_SIZE);                // we send the buffer
  ESP_LOGV(TAG, "%s => sent %d bytes - exec time %d µs ...", message, XFER_MAX_SIZE, micros() - start_exec);
}

/// @brief test read_array method
bool WeikaiChannel::uart_receive_test_(char *message) {
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

}  // namespace weikai
}  // namespace esphome
