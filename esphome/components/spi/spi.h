#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <vector>
#include <map>

#ifdef USE_ARDUINO

#include <SPI.h>

#endif
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
  MODE0 = SPI_MODE0,
  MODE1 = SPI_MODE1,
  MODE2 = SPI_MODE2,
  MODE3 = SPI_MODE3,
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

class Modes {
 public:
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

/**
 * A pin to replace those that don't exist.
 */
class NullPin : public GPIOPin {
 public:
  void setup() override {}

  void pin_mode(gpio::Flags flags) override {}

  bool digital_read() override { return false; }

  void digital_write(bool value) override {}

  std::string dump_summary() const override { return std::string(); }

  static GPIOPin *null_pin;
};

class SPIDelegateDummy;

// represents a device attached to an SPI bus, with a defined clock rate, mode and bit order. On Arduino this is
// a thin wrapper over SPIClass.
class SPIDelegate {
 public:

  SPIDelegate() = default;

  SPIDelegate(uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode, GPIOPin *cs_pin)
    : bit_order_(bit_order), data_rate_(data_rate), mode_(mode), cs_pin_(cs_pin) {
    if (this->cs_pin_ == nullptr)
      this->cs_pin_ = NullPin::null_pin;
    this->cs_pin_->setup();
    this->cs_pin_->digital_write(true);
  }

  virtual ~SPIDelegate() {};

  // enable CS if configured.
  virtual void begin_transaction() {
    this->cs_pin_->digital_write(false);
  }

  // end the transaction
  virtual void end_transaction() {
    this->cs_pin_->digital_write(true);
  }

  // transfer one byte, return the byte that was read.
  virtual uint8_t transfer(uint8_t data) = 0;

  // transfer a buffer, replace the contents with read data
  virtual void transfer(uint8_t *ptr, size_t length) {
    for (size_t i = 0; i != length; i++)
      ptr[i] = this->transfer(ptr[i]);
  }

  // write 16 bits, return read data
  // Question, what is the expected byte order? Implemented as little-endian.
  virtual uint16_t transfer16(uint16_t data) {
    uint16_t const out_data = this->transfer((uint8_t) data);
    return out_data | this->transfer((uint8_t) (data >> 8)) << 8;
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

  static SPIDelegateDummy null_delegate;

 protected:
  SPIBitOrder bit_order_{BIT_ORDER_MSB_FIRST};
  uint32_t data_rate_{1000000};
  SPIMode mode_{MODE0};
  GPIOPin *cs_pin_{NullPin::null_pin};

};

/**
 * A null_delegate SPIDelegate that complains if it's used.
 */

class SPIDelegateDummy : public SPIDelegate {
 public:
  SPIDelegateDummy() = default;

  uint8_t transfer(uint8_t data) override { return 0; }

  void begin_transaction() override;

};

/**
 * An implementation of SPI that relies only on software toggling of pins.
 *
 */
class SPIDelegateBitBash : public SPIDelegate {
 public:
  SPIDelegateBitBash(uint32_t clock, SPIBitOrder bit_order, SPIMode mode,
                     GPIOPin *cs_pin, GPIOPin *clk_pin, GPIOPin *sdo_pin, GPIOPin *sdi_pin)
    : SPIDelegate(clock, bit_order, mode, cs_pin), clk_pin_(clk_pin), sdo_pin_(sdo_pin),
      sdi_pin_(sdi_pin) {
    this->wait_cycle_ = uint32_t(arch_get_cpu_freq_hz()) / this->data_rate_ / 2ULL;
    if (this->sdo_pin_ == nullptr)
      this->sdo_pin_ = NullPin::null_pin;
    if (this->sdi_pin_ == nullptr)
      this->sdi_pin_ = NullPin::null_pin;
    this->clock_polarity_ = Modes::get_polarity(this->mode_);
    this->clock_phase_ = Modes::get_phase(this->mode_);
  }

  uint8_t transfer(uint8_t data) override {
    // Clock starts out at idle level
    this->clk_pin_->digital_write(clock_polarity_);
    uint8_t out_data = 0;

    for (uint8_t i = 0; i < 8; i++) {
      uint8_t shift;
      if (bit_order_ == BIT_ORDER_MSB_FIRST) {
        shift = 7 - i;
      } else {
        shift = i;
      }

      if (clock_phase_ == CLOCK_PHASE_LEADING) {
        // sampling on leading edge
        this->sdo_pin_->digital_write(data & (1 << shift));
        this->cycle_clock_(!clock_polarity_);
        out_data |= uint8_t(this->sdi_pin_->digital_read()) << shift;
        this->cycle_clock_(clock_polarity_);
      } else {
        // sampling on trailing edge
        this->cycle_clock_(!clock_polarity_);
        this->sdo_pin_->digital_write(data & (1 << shift));
        this->cycle_clock_(clock_polarity_);
        out_data |= uint8_t(this->sdi_pin_->digital_read()) << shift;
      }
    }
    App.feed_wdt();
    return out_data;
  }

 protected:
  GPIOPin *clk_pin_;
  GPIOPin *sdo_pin_;
  GPIOPin *sdi_pin_;
  uint32_t last_transition_{0};
  uint32_t wait_cycle_;
  SPIClockPolarity clock_polarity_;
  SPIClockPhase clock_phase_;

  void cycle_clock_(bool value) {
    while (this->last_transition_ - arch_get_cpu_cycle_count() < this->wait_cycle_)
      continue;
    this->clk_pin_->digital_write(value);
    this->last_transition_ += this->wait_cycle_;
    while (this->last_transition_ - arch_get_cpu_cycle_count() < this->wait_cycle_)
      continue;
    this->last_transition_ += this->wait_cycle_;
  }
};

#ifdef  USE_ARDUINO

class SPIHWDelegate : public SPIDelegate {
 public:
  SPIHWDelegate(SPIClass *channel, uint32_t data_rate, SPIBitOrder bit_order, SPIMode mode,
                GPIOPin *cs_pin) : SPIDelegate(data_rate, bit_order, mode, cs_pin), channel_(channel) {}

  void begin_transaction() override {
    SPIDelegate::begin_transaction();
#ifdef USE_RP2040
    SPISettings settings(this->data_rate_, static_cast<BitOrder>(this->bit_order_), this->mode_);
#else
    SPISettings settings(this->data_rate_, this->bit_order_, this->mode_);
#endif
    this->channel_->beginTransaction(settings);
  }

  void end_transaction() override { this->channel_->endTransaction(); }

  uint8_t transfer(uint8_t data) override { return this->channel_->transfer(data); }

 protected:
  SPIClass *channel_{};
};

#endif

class SPIClient;

class SPIComponent : public Component {
 public:
  SPIDelegate *register_device(SPIClient *device,
                               SPIMode mode,
                               SPIBitOrder bit_order,
                               uint32_t data_rate,
                               GPIOPin *cs_pin);
  void unregister_device(SPIClient *device);

  void set_clk(GPIOPin *clk) { clk_pin_ = clk; }

  void set_miso(GPIOPin *miso) { sdi_pin_ = miso; }

  void set_mosi(GPIOPin *mosi) { sdo_pin_ = mosi; }

  void set_force_sw(bool force_sw) { force_sw_ = force_sw; }


  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  GPIOPin *clk_pin_{nullptr};
  GPIOPin *sdi_pin_{nullptr};
  GPIOPin *sdo_pin_{nullptr};
  bool force_sw_{false};
  SPIClass *spi_channel_{};
  std::map<SPIClient *, SPIDelegate *> devices_;
};

/**
 * Base class for SPIClient, un-templated.
 */
class SPIClient {
 public:
  SPIClient(SPIBitOrder bit_order, SPIMode mode, uint32_t data_rate) :
    bit_order_(bit_order), mode_(mode), data_rate_(data_rate) {}

  void set_spi_parent(SPIComponent *parent) { this->parent_ = parent; }

  void set_cs_pin(GPIOPin *cs) { this->cs_ = cs; }

  void set_data_rate(uint32_t data_rate) { this->data_rate_ = data_rate; }

  void set_mode(SPIMode mode) { this->mode_ = mode; }

  void spi_setup() {
    this->delegate_ = this->parent_->register_device(this, this->mode_, this->bit_order_, this->data_rate_, this->cs_);
  }

  void spi_teardown() {
    this->parent_->unregister_device(this);
    this->delegate_ = &SPIDelegate::null_delegate;
  }

  void enable();
  void disable();

  uint8_t read_byte() { return this->delegate_->transfer(0); }

  void read_array(uint8_t *data, size_t length) {
    return this->delegate_->read_array(data, length);
  }

  void write_byte(uint8_t data) { this->delegate_->transfer(data); }

  void transfer_array(uint8_t *data, size_t length) { this->delegate_->transfer(data, length); }

  uint8_t transfer_byte(uint8_t data) { return this->delegate_->transfer(data); }

  ESPDEPRECATED("The semantics of write_byte16() are not well-defined", "2023.8")
  void write_byte16(uint16_t data) { this->delegate_->transfer16(data); }

  ESPDEPRECATED("The semantics of write_array16() are not well-defined", "2023.8")
  void write_array16(const uint16_t *data, size_t length) {
    this->delegate_->write_array((uint8_t *) data, length * 2);
  }

 protected:
  SPIBitOrder bit_order_{BIT_ORDER_MSB_FIRST};
  SPIMode mode_{MODE0};
  uint32_t data_rate_{1000000};
  SPIComponent *parent_{nullptr};
  GPIOPin *cs_{nullptr};
  SPIDelegate *delegate_{&SPIDelegate::null_delegate};
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
  SPIDevice() : SPIClient(BIT_ORDER, Modes::get_mode(CLOCK_POLARITY, CLOCK_PHASE), DATA_RATE) {}

  SPIDevice(SPIComponent *parent, GPIOPin *cs_pin) {
    this->set_spi_parent(parent);
    this->set_cs_pin(cs_pin);
  }

  void write_array(const uint8_t *data, size_t length) { this->delegate_->write_array(data, length); }

  template<size_t N>
  std::array<uint8_t, N> read_array() {
    std::array<uint8_t, N> data;
    this->read_array(data.data(), N);
    return data;
  }

  template<size_t N>
  void write_array(const std::array<uint8_t, N> &data) { this->write_array(data.data(), N); }

  void write_array(const std::vector<uint8_t> &data) { this->write_array(data.data(), data.size()); }

  template<size_t N>
  void transfer_array(std::array<uint8_t, N> &data) { this->transfer_array(data.data(), N); }

};

}  // namespace spi
}  // namespace esphome
