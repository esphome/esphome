#pragma once

#ifdef USE_ESP32

#include <array>
#include <atomic>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/static_task.h"

namespace esphome::ads131m0x {

static constexpr uint8_t MAX_CHANNELS = 8;

enum ADS131M0xPowerMode : uint8_t {
  ADS131M0X_POWER_MODE_VERY_LOW_POWER = 0,
  ADS131M0X_POWER_MODE_LOW_POWER = 1,
  ADS131M0X_POWER_MODE_HIGH_RESOLUTION = 2,
};

/// Result of one measurement window, in volts at the ADC input.
struct Snapshot {
  std::array<float, MAX_CHANNELS> rms{};   ///< RMS of the AC component (DC offset removed)
  std::array<float, MAX_CHANNELS> mean{};  ///< Mean (DC) value
  uint32_t sequence{0};                    ///< Incremented with every completed window; 0 = no valid data
  uint32_t timestamp_ms{0};                ///< millis() at the end of the window
};

/// TI ADS131M02/04/08 simultaneous-sampling 24-bit delta-sigma ADC (https://www.ti.com/lit/ds/symlink/ads131m08.pdf).
/// A FreeRTOS task woken by DRDY reads every conversion, checks CRC and STATUS, reconfigures after a device reset,
/// and publishes mean and AC RMS per window as a Snapshot.
class ADS131M0x final : public Component,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_8MHZ> {
 public:
  void setup() override;
  void dump_config() override;

  void set_num_channels(uint8_t num_channels) { this->num_channels_ = num_channels; }
  void set_drdy_pin(InternalGPIOPin *pin) { this->drdy_pin_ = pin; }
  void set_reset_pin(InternalGPIOPin *pin) { this->reset_pin_ = pin; }
  void set_power_mode(ADS131M0xPowerMode power_mode) { this->power_mode_ = power_mode; }
  void set_osr_code(uint8_t code) { this->osr_code_ = code; }
  void set_sample_rate(float sample_rate) { this->sample_rate_ = sample_rate; }
  void set_window_ms(uint32_t window_ms) { this->window_ms_ = window_ms; }
  void set_gain(uint8_t channel, uint8_t gain_code) { this->gain_codes_[channel] = gain_code; }

  /// Copy of the latest completed window; sequence is 0 until the first window after (re)configuration.
  /// Safe to call from any task.
  Snapshot get_snapshot();
  /// True while the device is configured and delivers samples at the expected rate.
  bool is_healthy() const { return this->healthy_.load() != 0; }
  uint32_t get_window_ms() const { return this->window_ms_; }
  /// Input full scale of a channel in volts (the input range is +/- this value).
  float get_full_scale(uint8_t channel) const;

 protected:
  static void task_entry(void *arg);
  static void drdy_isr(ADS131M0x *self);
  void task_loop_();
  void check_health_();
  /// Averages the sample rate over several health checks and warns when conversions are being lost.
  void check_rate_(uint32_t elapsed_ms, uint32_t frames);

  /// Resets (if a reset pin is set) and configures the device. Returns nullptr on success, else the reason.
  const LogString *configure_();
  /// Clocks one full frame. Returns false on an output CRC mismatch. `response` receives the first word.
  bool transfer_frame_(uint16_t command, uint16_t data, uint16_t *response, int32_t *samples);
  bool read_register_(uint8_t address, uint16_t *value);
  bool write_register_(uint8_t address, uint16_t value);
  size_t frame_bytes_() const;
  /// Reads both FIFO slots back to back, as the datasheet requires after a gap in reading.
  void drain_fifo_();
  /// Marks the snapshot as invalid (sequence 0) until a full window has been sampled again.
  void invalidate_snapshot_();

  InternalGPIOPin *drdy_pin_{nullptr};
  InternalGPIOPin *reset_pin_{nullptr};
  uint8_t num_channels_{MAX_CHANNELS};
  ADS131M0xPowerMode power_mode_{ADS131M0X_POWER_MODE_HIGH_RESOLUTION};
  uint8_t osr_code_{3};
  float sample_rate_{4000.0f};
  uint32_t window_ms_{100};
  std::array<uint8_t, MAX_CHANNELS> gain_codes_{};

  StaticTask task_;
  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot snapshot_{};
  uint8_t *frame_buf_{nullptr};  ///< DMA-capable, allocated once in setup()
  size_t transfer_bytes_{0};     ///< Frame length, padded where the DMA requires it

  // Sampling task only
  uint32_t window_sequence_{0};

  // Written by the sampling task, read by the main loop
  std::atomic<uint8_t> healthy_{0};
  std::atomic<uint8_t> configured_{0};
  std::atomic<uint32_t> frames_{0};
  std::atomic<uint32_t> crc_errors_{0};
  std::atomic<uint32_t> device_resets_{0};

  // Main loop only
  uint32_t last_health_check_{0};
  uint32_t last_frames_{0};
  uint32_t last_crc_errors_{0};
  uint32_t last_device_resets_{0};
  uint32_t rate_start_frames_{0};
  uint32_t rate_elapsed_ms_{0};
  uint8_t rate_checks_{0};
  bool rate_degraded_{false};
};

}  // namespace esphome::ads131m0x

#endif  // USE_ESP32
