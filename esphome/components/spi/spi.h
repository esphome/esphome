#pragma once

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <map>
#include <utility>
#include <vector>

#ifdef USE_ARDUINO

#include <SPI.h>

#ifdef USE_RP2040
using SPIInterface = SPIClassRP2040 *;
#else
using SPIInterface = SPIClass *;
#endif

#endif

#ifdef USE_ESP_IDF

#include "driver/spi_master.h"

using SPIInterface = spi_host_device_t;

#endif  // USE_ESP_IDF

/**
 * Implementation of SPI Controller mode.
 */
namespace esphome {
namespace spi {

/// The bit-order for SPI devices. This defines how the data read from and written to the device is interpreted.
enum SPIBitOrder {
  /// The least significant bit is transmitted/received first.
  BIT_ORDER_LSB_FIRST,
  /// The most significant bit is transmitted/received first.
  BIT_ORDER_MSB_FIRST,
};
/** The SPI clock signal polarity,
 *
 * This defines how the clock signal is used. Flipping this effectively inverts the clock signal.
 */
enum SPIClockPolarity {
  /** The clock signal idles on LOW. (CPOL=0)
   *
   * A rising edge means a leading edge for the clock.
   */
  CLOCK_POLARITY_LOW = false,
  /** The clock signal idles on HIGH. (CPOL=1)
   *
   * A falling edge means a trailing edge for the clock.
   */
  CLOCK_POLARITY_HIGH = true,
};
/** The SPI clock signal phase.
 *
 * This defines when the data signals are sampled. Most SPI devices use the LEADING clock phase.
 */
enum SPIClockPhase {
  /// The data is sampled on a leading clock edge. (CPHA=0)
  CLOCK_PHASE_LEADING,
  /// The data is sampled on a trailing clock edge. (CPHA=1)
  CLOCK_PHASE_TRAILING,
};

/**
 * Modes mapping to clock phase and polarity.
 *
 */

enum SPIMode {
  MODE0 = 0,
  MODE1 = 1,
  MODE2 = 2,
  MODE3 = 3,
};
/** The SPI clock signal frequency, which determines the transfer bit rate/second.
 *
 * Implementations can use the pre-defined constants here, or use an integer in the template definition
 * to manually use a specific data rate.
 */
enum SPIDataRate : uint32_t {
  DATA_RATE_1KHZ = 1000,
  DATA_RATE_75KHZ = 75000,
  DATA_RATE_200KHZ = 200000,
  DATA_RATE_1MHZ = 1000000,
  DATA_RATE_2MHZ = 2000000,
  DATA_RATE_4MHZ = 4000000,
  DATA_RATE_5MHZ = 5000000,
  DATA_RATE_8MHZ = 8000000,
  DATA_RATE_10MHZ = 10000000,
  DATA_RATE_20MHZ = 20000000,
  DATA_RATE_40MHZ = 40000000,
  DATA_RATE_80MHZ = 80000000,
};

/**
 * A pin to replace those that don't exist.
 */
class NullPin : public GPIOPin {
  friend class SPIComponent;

  friend class SPIDelegate;

  friend class Utility;

 public:
  void setup() override {}

  void pin_mode(gpio::Flags flags) override {}

  bool digital_read() override { return false; }

  void digital_write(bool value) override {}

  std::string dump_summary() const override { return std::string(); }

 protected:
  static GPIOPin *const NULL_PIN;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  // https://bugs.llvm.org/show_bug.cgi?id=48040
};

class Utility {
 public:
  static int get_pin_no(GPIOPin *pin) {
    if (pin == nullptr || !pin->is_internal())
      return -1;
    if (((InternalGPIOPin *) pin)->is_inverted())
      return -1;
    return ((InternalGPIOPin *) pin)->get_pin();
  }

  static SPIMode get_mode(SPIClockPolarity polarity, SPIClockPhase phase) {
    if (polarity == CLOCK_POLARITY_HIGH) {
      return phase == CLOCK_PHASE_LEADING ? MODE2 : MODE3;
    }
    return phase == CLOCK_PHASE_LEADING ? MODE0 : MODE1;
  }

  static SPIClockPhase get_phase(SPIMode mode) {
    switch (mode) {
      case MODE0:
      case MODE2:
        return CLOCK_PHASE_LEADING;
      default:
        return CLOCK_PHASE_TRAILING;
    }
  }

  static SPIClockPolarity get_polarity(SPIMode mode) {
    switch (mode) {
      case MODE0:
      case MODE1:
        return CLOCK_POLARITY_LOW;
      default:
        return CLOCK_POLARITY_HIGH;
    }
  }
};

// represents a device attached to an SPI bus, with a defined clock rate, mode and bit order. On Arduino this is
// a thin wrapper over SPIClass.
class SPIDelegate {
  friend class SPIClient;

 public:
  SPIDelegate() = default;

  SPIDelegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin)
      : bit_order_(bit_order), data_rate_(data_rate), mode_(mode), cs_pin_(cs_pin) {
    if (this->cs_pin_ == nullptr)
      this->cs_pin_ = NullPin::NULL_PIN;
    this->cs_pin_->setup();
    this->cs_pin_->digital_write(true);
  }

  virtual ~SPIDelegate(){};

  // enable CS if configured.
  virtual void begin_transaction() { this->cs_pin_->digital_write(false); }

  // end the transaction
  virtual void end_transaction() { this->cs_pin_->digital_write(true); }

  // transfer one byte, return the byte that was read.
  virtual uint8_t transfer(uint8_t data) = 0;

  // transfer a buffer, replace the contents with read data
  virtual void transfer(uint8_t *ptr, size_t length) { this->transfer(ptr, ptr, length); }

  virtual void transfer(const uint8_t *txbuf, uint8_t *rxbuf, size_t length) {
    for (size_t i = 0; i != length; i++)
      rxbuf[i] = this->transfer(txbuf[i]);
  }

  /**
   * write a variable length data item, up to 16 bits.
   * @param data The data to send. Should be LSB-aligned (i.e. top bits will be discarded.)
   * @param num_bits The number of bits to send
   */
  virtual void write(uint16_t data, size_t num_bits) {
    esph_log_e("spi_device", "variable length write not implemented");
  }

  virtual void write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address,
                                   const uint8_t *data, size_t length, uint8_t bus_width) {
    esph_log_e("spi_device", "write_cmd_addr_data not implemented");
  }
  // write 16 bits
  virtual void write16(uint16_t data) {
    if (this->bit_order_ == BIT_ORDER_MSB_FIRST) {
      uint16_t buffer;
      buffer = (data >> 8) | (data << 8);
      this->write_array(reinterpret_cast<const uint8_t *>(&buffer), 2);
    } else {
      this->write_array(reinterpret_cast<const uint8_t *>(&data), 2);
    }
  }

  virtual void write_array16(const uint16_t *data, size_t length) {
    for (size_t i = 0; i != length; i++) {
      this->write16(data[i]);
    }
  }

  // write the contents of a buffer, ignore read data (buffer is unchanged.)
  virtual void write_array(const uint8_t *ptr, size_t length) {
    for (size_t i = 0; i != length; i++)
      this->transfer(ptr[i]);
  }

  // read into a buffer, write nulls
  virtual void read_array(uint8_t *ptr, size_t length) {
    for (size_t i = 0; i != length; i++)
      ptr[i] = this->transfer(0);
  }

  // check if device is ready
  virtual bool is_ready();

 protected:
  SPIBitOrder bit_order_{BIT_ORDER_MSB_FIRST};
  uint32_t data_rate_{1000000};
  SPIMode mode_{MODE0};
  GPIOPin *cs_pin_{NullPin::NULL_PIN};
};

/**
 * An implementation of SPI that relies only on software toggling of pins.
 *
 */
class SPIDelegateBitBash : public SPIDelegate {
 public:
  SPIDelegateBitBash(uint32_t clock, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin, GPIOPin *clk_pin,
                     GPIOPin *sdo_pin, GPIOPin *sdi_pin)
      : SPIDelegate(clock, bit_order, mode, cs_pin), clk_pin_(clk_pin), sdo_pin_(sdo_pin), sdi_pin_(sdi_pin) {
    // this calculation is pretty meaningless except at very low bit rates.
    this->wait_cycle_ = uint32_t(arch_get_cpu_freq_hz()) / this->data_rate_ / 2ULL;
    this->clock_polarity_ = Utility::get_polarity(this->mode_);
    this->clock_phase_ = Utility::get_phase(this->mode_);
  }

  uint8_t transfer(uint8_t data) override;

  void write(uint16_t data, size_t num_bits) override;

  void write16(uint16_t data) override { this->write(data, 16); };

 protected:
  GPIOPin *clk_pin_;
  GPIOPin *sdo_pin_;
  GPIOPin *sdi_pin_;
  uint32_t last_transition_{0};
  uint32_t wait_cycle_;
  SPIClockPolarity clock_polarity_;
  SPIClockPhase clock_phase_;

  void HOT cycle_clock_() {
    while (this->last_transition_ - arch_get_cpu_cycle_count() < this->wait_cycle_)
      continue;
    this->last_transition_ += this->wait_cycle_;
  }
  uint16_t transfer_(uint16_t data, size_t num_bits);
};

class SPIBus {
 public:
  SPIBus() = default;

  SPIBus(GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi) : clk_pin_(clk), sdo_pin_(sdo), sdi_pin_(sdi) {}

  virtual SPIDelegate *get_delegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin) {
    return new SPIDelegateBitBash(data_rate, bit_order, mode, cs_pin, this->clk_pin_, this->sdo_pin_, this->sdi_pin_);
  }

  virtual bool is_hw() { return false; }

 protected:
  GPIOPin *clk_pin_{};
  GPIOPin *sdo_pin_{};
  GPIOPin *sdi_pin_{};
};

class SPIClient;

class SPIComponent : public Component {
 public:
  SPIDelegate *register_device(SPIClient *device, SPIMode mode, SPIBitOrder bit_order, uint32_t data_rate,
                               GPIOPin *cs_pin);
  void unregister_device(SPIClient *device);

  void set_clk(GPIOPin *clk) { this->clk_pin_ = clk; }

  void set_miso(GPIOPin *sdi) { this->sdi_pin_ = sdi; }

  void set_mosi(GPIOPin *sdo) { this->sdo_pin_ = sdo; }
  void set_data_pins(std::vector<uint8_t> pins) { this->data_pins_ = std::move(pins); }

  void set_interface(SPIInterface interface) {
    this->interface_ = interface;
    this->using_hw_ = true;
  }

  void set_interface_name(const char *name) { this->interface_name_ = name; }

  float get_setup_priority() const override { return setup_priority::BUS; }

  void setup() override;
  void dump_config() override;

 protected:
  GPIOPin *clk_pin_{nullptr};
  GPIOPin *sdi_pin_{nullptr};
  GPIOPin *sdo_pin_{nullptr};
  std::vector<uint8_t> data_pins_{};

  SPIInterface interface_{};
  bool using_hw_{false};
  const char *interface_name_{nullptr};
  SPIBus *spi_bus_{};
  std::map<SPIClient *, SPIDelegate *> devices_;

  static SPIBus *get_bus(SPIInterface interface, GPIOPin *clk, GPIOPin *sdo, GPIOPin *sdi,
                         const std::vector<uint8_t> &data_pins);
};

using QuadSPIComponent = SPIComponent;
/**
 * Base class for SPIDevice, un-templated.
 */
class SPIClient {
 public:
  SPIClient(SPIBitOrder bit_order, SPIMode mode, uint32_t data_rate)
      : bit_order_(bit_order), mode_(mode), data_rate_(data_rate) {}

  virtual void spi_setup() {
    esph_log_d("spi_device", "mode %u, data_rate %ukHz", (unsigned) this->mode_, (unsigned) (this->data_rate_ / 1000));
    this->delegate_ = this->parent_->register_device(this, this->mode_, this->bit_order_, this->data_rate_, this->cs_);
  }

  virtual void spi_teardown() {
    this->parent_->unregister_device(this);
    this->delegate_ = nullptr;
  }

  bool spi_is_ready() { return this->delegate_->is_ready(); }

 protected:
  SPIBitOrder bit_order_{BIT_ORDER_MSB_FIRST};
  SPIMode mode_{MODE0};
  uint32_t data_rate_{1000000};
  SPIComponent *parent_{nullptr};
  GPIOPin *cs_{nullptr};
  SPIDelegate *delegate_{nullptr};
};

/**
 * The SPIDevice is what components using the SPI will create.
 *
 * @tparam BIT_ORDER
 * @tparam CLOCK_POLARITY
 * @tparam CLOCK_PHASE
 * @tparam DATA_RATE
 */
template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE, SPIDataRate DATA_RATE>
class SPIDevice : public SPIClient {
 public:
  SPIDevice() : SPIClient(BIT_ORDER, Utility::get_mode(CLOCK_POLARITY, CLOCK_PHASE), DATA_RATE) {}

  SPIDevice(SPIComponent *parent, GPIOPin *cs_pin) {
    this->set_spi_parent(parent);
    this->set_cs_pin(cs_pin);
  }

  void spi_setup() override { SPIClient::spi_setup(); }

  void spi_teardown() override { SPIClient::spi_teardown(); }

  void set_spi_parent(SPIComponent *parent) { this->parent_ = parent; }

  void set_cs_pin(GPIOPin *cs) { this->cs_ = cs; }

  void set_data_rate(uint32_t data_rate) { this->data_rate_ = data_rate; }

  void set_bit_order(SPIBitOrder order) { this->bit_order_ = order; }

  void set_mode(SPIMode mode) { this->mode_ = mode; }

  uint8_t read_byte() { return this->delegate_->transfer(0); }

  void read_array(uint8_t *data, size_t length) { return this->delegate_->read_array(data, length); }

  /**
   * Write a single data item, up to 32 bits.
   * @param data    The data
   * @param num_bits The number of bits to write. The lower num_bits of data will be sent.
   */
  void write(uint16_t data, size_t num_bits) { this->delegate_->write(data, num_bits); };

  /* Write command, address and data. Command and address will be written as single-bit SPI,
   * data phase can be multiple bit (currently only 1 or 4)
   * @param cmd_bits Number of bits to write in the command phase
   * @param cmd The command value to write
   * @param addr_bits Number of bits to write in addr phase
   * @param address Address data
   * @param data Plain data bytes
   * @param length Number of data bytes
   * @param bus_width The number of data lines to use for the data phase.
   */
  void write_cmd_addr_data(size_t cmd_bits, uint32_t cmd, size_t addr_bits, uint32_t address, const uint8_t *data,
                           size_t length, uint8_t bus_width = 1) {
    this->delegate_->write_cmd_addr_data(cmd_bits, cmd, addr_bits, address, data, length, bus_width);
  }

  void write_byte(uint8_t data) { this->delegate_->write_array(&data, 1); }

  /**
   * Write the array data, replace with received data.
   * @param data
   * @param length
   */
  void transfer_array(uint8_t *data, size_t length) { this->delegate_->transfer(data, length); }

  uint8_t transfer_byte(uint8_t data) { return this->delegate_->transfer(data); }

  /** Write 16 bit data. The driver will byte-swap if required.
   */
  void write_byte16(uint16_t data) { this->delegate_->write16(data); }

  /**
   * Write an array of data as 16 bit values, byte-swapping if required. Use of this should be avoided as
   * it is horribly slow.
   * @param data
   * @param length
   */
  void write_array16(const uint16_t *data, size_t length) { this->delegate_->write_array16(data, length); }

  void enable() { this->delegate_->begin_transaction(); }

  void disable() { this->delegate_->end_transaction(); }

  void write_array(const uint8_t *data, size_t length) { this->delegate_->write_array(data, length); }

  template<size_t N> void write_array(const std::array<uint8_t, N> &data) { this->write_array(data.data(), N); }

  void write_array(const std::vector<uint8_t> &data) { this->write_array(data.data(), data.size()); }

  template<size_t N> void transfer_array(std::array<uint8_t, N> &data) { this->transfer_array(data.data(), N); }
};

}  // namespace spi
}  // namespace esphome
