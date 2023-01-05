#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include <vector>

#ifdef USE_ARDUINO
#define USE_SPI_ARDUINO_BACKEND
#endif

#ifdef USE_SPI_ARDUINO_BACKEND
#include <SPI.h>
#endif

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
/** The SPI clock signal data rate. This defines for what duration the clock signal is HIGH/LOW.
 * So effectively the rate of bytes can be calculated using
 *
 * effective_byte_rate = spi_data_rate / 16
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
  DATA_RATE_8MHZ = 8000000,
  DATA_RATE_10MHZ = 10000000,
  DATA_RATE_20MHZ = 20000000,
  DATA_RATE_40MHZ = 40000000,
};

class SPIComponent : public Component {
 public:
  void set_clk(GPIOPin *clk) { clk_ = clk; }
  void set_miso(GPIOPin *miso) { miso_ = miso; }
  void set_mosi(GPIOPin *mosi) { mosi_ = mosi; }

  void setup() override;

  void dump_config() override;

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE> uint8_t read_byte() {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
      return this->hw_spi_->transfer(0x00);
    }
#endif  // USE_SPI_ARDUINO_BACKEND
    return this->transfer_<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE, true, false>(0x00);
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void read_array(uint8_t *data, size_t length) {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
      this->hw_spi_->transfer(data, length);
      return;
    }
#endif  // USE_SPI_ARDUINO_BACKEND
    for (size_t i = 0; i < length; i++) {
      data[i] = this->read_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>();
    }
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void write_byte(uint8_t data) {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
#ifdef USE_RP2040
      this->hw_spi_->transfer(data);
#else
      this->hw_spi_->write(data);
#endif
      return;
    }
#endif  // USE_SPI_ARDUINO_BACKEND
    this->transfer_<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE, false, true>(data);
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void write_byte16(const uint16_t data) {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
#ifdef USE_RP2040
      this->hw_spi_->transfer16(data);
#else
      this->hw_spi_->write16(data);
#endif
      return;
    }
#endif  // USE_SPI_ARDUINO_BACKEND

    this->write_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data >> 8);
    this->write_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data);
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void write_array16(const uint16_t *data, size_t length) {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
      for (size_t i = 0; i < length; i++) {
#ifdef USE_RP2040
        this->hw_spi_->transfer16(data[i]);
#else
        this->hw_spi_->write16(data[i]);
#endif
      }
      return;
    }
#endif  // USE_SPI_ARDUINO_BACKEND
    for (size_t i = 0; i < length; i++) {
      this->write_byte16<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data[i]);
    }
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void write_array(const uint8_t *data, size_t length) {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
      auto *data_c = const_cast<uint8_t *>(data);
#ifdef USE_RP2040
      this->hw_spi_->transfer(data_c, length);
#else
      this->hw_spi_->writeBytes(data_c, length);
#endif
      return;
    }
#endif  // USE_SPI_ARDUINO_BACKEND
    for (size_t i = 0; i < length; i++) {
      this->write_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data[i]);
    }
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  uint8_t transfer_byte(uint8_t data) {
    if (this->miso_ != nullptr) {
#ifdef USE_SPI_ARDUINO_BACKEND
      if (this->hw_spi_ != nullptr) {
        return this->hw_spi_->transfer(data);
      } else {
#endif  // USE_SPI_ARDUINO_BACKEND
        return this->transfer_<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE, true, true>(data);
#ifdef USE_SPI_ARDUINO_BACKEND
      }
#endif  // USE_SPI_ARDUINO_BACKEND
    }
    this->write_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data);
    return 0;
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE>
  void transfer_array(uint8_t *data, size_t length) {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
      if (this->miso_ != nullptr) {
        this->hw_spi_->transfer(data, length);
      } else {
#ifdef USE_RP2040
        this->hw_spi_->transfer(data, length);
#else
        this->hw_spi_->writeBytes(data, length);
#endif
      }
      return;
    }
#endif  // USE_SPI_ARDUINO_BACKEND

    if (this->miso_ != nullptr) {
      for (size_t i = 0; i < length; i++) {
        data[i] = this->transfer_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data[i]);
      }
    } else {
      this->write_array<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
    }
  }

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE, uint32_t DATA_RATE>
  void enable(GPIOPin *cs) {
#ifdef USE_SPI_ARDUINO_BACKEND
    if (this->hw_spi_ != nullptr) {
      uint8_t data_mode = SPI_MODE0;
      if (!CLOCK_POLARITY && CLOCK_PHASE) {
        data_mode = SPI_MODE1;
      } else if (CLOCK_POLARITY && !CLOCK_PHASE) {
        data_mode = SPI_MODE2;
      } else if (CLOCK_POLARITY && CLOCK_PHASE) {
        data_mode = SPI_MODE3;
      }
#ifdef USE_RP2040
      SPISettings settings(DATA_RATE, static_cast<BitOrder>(BIT_ORDER), data_mode);
#else
      SPISettings settings(DATA_RATE, BIT_ORDER, data_mode);
#endif
      this->hw_spi_->beginTransaction(settings);
    } else {
#endif  // USE_SPI_ARDUINO_BACKEND
      this->clk_->digital_write(CLOCK_POLARITY);
      uint32_t cpu_freq_hz = arch_get_cpu_freq_hz();
      this->wait_cycle_ = uint32_t(cpu_freq_hz) / DATA_RATE / 2ULL;
#ifdef USE_SPI_ARDUINO_BACKEND
    }
#endif  // USE_SPI_ARDUINO_BACKEND

    if (cs != nullptr) {
      this->active_cs_ = cs;
      this->active_cs_->digital_write(false);
    }
  }

  void disable();

  float get_setup_priority() const override;

 protected:
  inline void cycle_clock_(bool value);

  template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE, bool READ, bool WRITE>
  uint8_t transfer_(uint8_t data);

  GPIOPin *clk_;
  GPIOPin *miso_{nullptr};
  GPIOPin *mosi_{nullptr};
  GPIOPin *active_cs_{nullptr};
#ifdef USE_SPI_ARDUINO_BACKEND
  SPIClass *hw_spi_{nullptr};
#endif  // USE_SPI_ARDUINO_BACKEND
  uint32_t wait_cycle_;
};

template<SPIBitOrder BIT_ORDER, SPIClockPolarity CLOCK_POLARITY, SPIClockPhase CLOCK_PHASE, SPIDataRate DATA_RATE>
class SPIDevice {
 public:
  SPIDevice() = default;
  SPIDevice(SPIComponent *parent, GPIOPin *cs) : parent_(parent), cs_(cs) {}

  void set_spi_parent(SPIComponent *parent) { parent_ = parent; }
  void set_cs_pin(GPIOPin *cs) { cs_ = cs; }

  void spi_setup() {
    if (this->cs_) {
      this->cs_->setup();
      this->cs_->digital_write(true);
    }
  }

  void enable() { this->parent_->template enable<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE, DATA_RATE>(this->cs_); }

  void disable() { this->parent_->disable(); }

  uint8_t read_byte() { return this->parent_->template read_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(); }

  void read_array(uint8_t *data, size_t length) {
    return this->parent_->template read_array<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
  }

  template<size_t N> std::array<uint8_t, N> read_array() {
    std::array<uint8_t, N> data;
    this->read_array(data.data(), N);
    return data;
  }

  void write_byte(uint8_t data) {
    return this->parent_->template write_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data);
  }

  void write_byte16(uint16_t data) {
    return this->parent_->template write_byte16<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data);
  }

  void write_array16(const uint16_t *data, size_t length) {
    this->parent_->template write_array16<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
  }

  void write_array(const uint8_t *data, size_t length) {
    this->parent_->template write_array<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
  }

  template<size_t N> void write_array(const std::array<uint8_t, N> &data) { this->write_array(data.data(), N); }

  void write_array(const std::vector<uint8_t> &data) { this->write_array(data.data(), data.size()); }

  uint8_t transfer_byte(uint8_t data) {
    return this->parent_->template transfer_byte<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data);
  }

  void transfer_array(uint8_t *data, size_t length) {
    this->parent_->template transfer_array<BIT_ORDER, CLOCK_POLARITY, CLOCK_PHASE>(data, length);
  }

  template<size_t N> void transfer_array(std::array<uint8_t, N> &data) { this->transfer_array(data.data(), N); }

 protected:
  SPIComponent *parent_{nullptr};
  GPIOPin *cs_{nullptr};
};

}  // namespace spi
}  // namespace esphome
