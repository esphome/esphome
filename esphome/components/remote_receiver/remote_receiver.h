#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

#include <cinttypes>

#if defined(USE_ESP32)
#include <driver/rmt_rx.h>
#endif

namespace esphome::remote_receiver {

#if defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2040)
struct RemoteReceiverComponentStore {
  static void gpio_intr(RemoteReceiverComponentStore *arg);

  /// Stores pulse durations in microseconds as signed integers
  ///  * Positive values indicate high pulses (marks)
  ///  * Negative values indicate low pulses (spaces)
  volatile int32_t *buffer{nullptr};
  /// The position last written to
  volatile uint32_t buffer_write{0};
  /// The start position of the last sequence
  volatile uint32_t buffer_start{0};
  /// The position last read from
  uint32_t buffer_read{0};
  volatile uint32_t commit_micros{0};
  volatile uint32_t prev_micros{0};
  uint32_t buffer_size{1000};
  uint32_t filter_us{10};
  uint32_t idle_us{10000};
  ISRInternalGPIOPin pin;
  volatile bool commit_level{false};
  volatile bool prev_level{false};
  volatile bool overflow{false};
};
#elif defined(USE_ESP32)
struct RemoteReceiverComponentStore {
  /// Stores RMT symbols and rx done event data
  volatile uint8_t *buffer{nullptr};
  /// The position last written to
  volatile uint32_t buffer_write{0};
  /// The position last read from
  volatile uint32_t buffer_read{0};
  bool overflow{false};
  uint32_t buffer_size{1000};
  uint32_t receive_size{0};
  uint32_t filter_symbols{0};
  esp_err_t error{ESP_OK};
  rmt_receive_config_t config;
};
#endif

class RemoteReceiverComponent : public remote_base::RemoteReceiverBase,
                                public Component
#ifdef USE_ESP32
    ,
                                public remote_base::RemoteRMTChannel
#endif

{
 public:
  RemoteReceiverComponent(InternalGPIOPin *pin) : RemoteReceiverBase(pin) {}
  void setup() override;
  void dump_config() override;
  void loop() override;

#ifdef USE_ESP32
  void set_filter_symbols(uint32_t filter_symbols) { this->filter_symbols_ = filter_symbols; }
  void set_receive_symbols(uint32_t receive_symbols) { this->receive_symbols_ = receive_symbols; }
  void set_with_dma(bool with_dma) { this->with_dma_ = with_dma; }
  void set_carrier_duty_percent(uint8_t carrier_duty_percent) { this->carrier_duty_percent_ = carrier_duty_percent; }
  void set_carrier_frequency(uint32_t carrier_frequency) { this->carrier_frequency_ = carrier_frequency; }
#endif
  void set_buffer_size(uint32_t buffer_size) { this->buffer_size_ = buffer_size; }
  void set_filter_us(uint32_t filter_us) { this->filter_us_ = filter_us; }
  void set_idle_us(uint32_t idle_us) { this->idle_us_ = idle_us; }

 protected:
#ifdef USE_ESP32
  void decode_rmt_(rmt_symbol_word_t *item, size_t item_count);
  rmt_channel_handle_t channel_{NULL};
  uint32_t filter_symbols_{0};
  uint32_t receive_symbols_{0};
  bool with_dma_{false};
  uint32_t carrier_frequency_{0};
  uint8_t carrier_duty_percent_{100};
  esp_err_t error_code_{ESP_OK};
  std::string error_string_{""};
#endif

#if defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2040) || defined(USE_ESP32)
  RemoteReceiverComponentStore store_;
#endif

#if defined(USE_ESP8266) || defined(USE_LIBRETINY) || defined(USE_RP2040)
  HighFrequencyLoopRequester high_freq_;
#endif

  uint32_t buffer_size_{};
  uint32_t filter_us_{10};
  uint32_t idle_us_{10000};
};

}  // namespace esphome::remote_receiver
