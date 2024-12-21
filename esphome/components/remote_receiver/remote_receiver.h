#pragma once

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"

#include <cinttypes>

#if defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR >= 5
#include <driver/rmt_rx.h>
#endif

namespace esphome {
namespace remote_receiver {

#if defined(USE_ESP8266) || defined(USE_LIBRETINY)
struct RemoteReceiverComponentStore {
  static void gpio_intr(RemoteReceiverComponentStore *arg);

  /// Stores the time (in micros) that the leading/falling edge happened at
  ///  * An even index means a falling edge appeared at the time stored at the index
  ///  * An uneven index means a rising edge appeared at the time stored at the index
  volatile uint32_t *buffer{nullptr};
  /// The position last written to
  volatile uint32_t buffer_write_at;
  /// The position last read from
  uint32_t buffer_read_at{0};
  bool overflow{false};
  uint32_t buffer_size{1000};
  uint32_t filter_us{10};
  ISRInternalGPIOPin pin;
};
#elif defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR >= 5
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
#if defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR < 5
  RemoteReceiverComponent(InternalGPIOPin *pin, uint8_t mem_block_num = 1)
      : RemoteReceiverBase(pin), remote_base::RemoteRMTChannel(mem_block_num) {}

  RemoteReceiverComponent(InternalGPIOPin *pin, rmt_channel_t channel, uint8_t mem_block_num = 1)
      : RemoteReceiverBase(pin), remote_base::RemoteRMTChannel(channel, mem_block_num) {}
#else
  RemoteReceiverComponent(InternalGPIOPin *pin) : RemoteReceiverBase(pin) {}
#endif
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

#if defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR >= 5
  void set_filter_symbols(uint32_t filter_symbols) { this->filter_symbols_ = filter_symbols; }
  void set_receive_symbols(uint32_t receive_symbols) { this->receive_symbols_ = receive_symbols; }
  void set_with_dma(bool with_dma) { this->with_dma_ = with_dma; }
#endif
  void set_buffer_size(uint32_t buffer_size) { this->buffer_size_ = buffer_size; }
  void set_filter_us(uint32_t filter_us) { this->filter_us_ = filter_us; }
  void set_idle_us(uint32_t idle_us) { this->idle_us_ = idle_us; }

 protected:
#ifdef USE_ESP32
#if ESP_IDF_VERSION_MAJOR >= 5
  void decode_rmt_(rmt_symbol_word_t *item, size_t item_count);
  rmt_channel_handle_t channel_{NULL};
  uint32_t filter_symbols_{0};
  uint32_t receive_symbols_{0};
  bool with_dma_{false};
#else
  void decode_rmt_(rmt_item32_t *item, size_t item_count);
  RingbufHandle_t ringbuf_;
#endif
  esp_err_t error_code_{ESP_OK};
  std::string error_string_{""};
#endif

#if defined(USE_ESP8266) || defined(USE_LIBRETINY) || (defined(USE_ESP32) && ESP_IDF_VERSION_MAJOR >= 5)
  RemoteReceiverComponentStore store_;
  HighFrequencyLoopRequester high_freq_;
#endif

  uint32_t buffer_size_{};
  uint32_t filter_us_{10};
  uint32_t idle_us_{10000};
};

}  // namespace remote_receiver
}  // namespace esphome
