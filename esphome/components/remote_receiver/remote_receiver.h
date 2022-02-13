#pragma once

#include "esphome/core/component.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace remote_receiver {

#ifdef USE_ESP8266
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
  uint8_t filter_us{10};
  ISRInternalGPIOPin pin;
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
#ifdef USE_ESP32
  RemoteReceiverComponent(InternalGPIOPin *pin, uint8_t mem_block_num = 1)
      : RemoteReceiverBase(pin), remote_base::RemoteRMTChannel(mem_block_num) {}
#else
  RemoteReceiverComponent(InternalGPIOPin *pin) : RemoteReceiverBase(pin) {}
#endif
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_buffer_size(uint32_t buffer_size) { this->buffer_size_ = buffer_size; }
  void set_filter_us(uint8_t filter_us) { this->filter_us_ = filter_us; }
  void set_idle_us(uint32_t idle_us) { this->idle_us_ = idle_us; }

 protected:
#ifdef USE_ESP32
  void decode_rmt_(rmt_item32_t *item, size_t len);
  RingbufHandle_t ringbuf_;
  esp_err_t error_code_{ESP_OK};
#endif

#ifdef USE_ESP8266
  RemoteReceiverComponentStore store_;
  HighFrequencyLoopRequester high_freq_;
#endif

  uint32_t buffer_size_{};
  uint8_t filter_us_{10};
  uint32_t idle_us_{10000};
};

}  // namespace remote_receiver
}  // namespace esphome
