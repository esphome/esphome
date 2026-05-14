#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/usb_host/usb_host.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"
#include <atomic>
#include <functional>

namespace esphome::usb_uart {

class USBUartTypeCdcAcm;
class USBUartComponent;
class USBUartChannel;

static const char *const TAG = "usb_uart";

static constexpr uint8_t USB_CDC_SUBCLASS_ACM = 0x02;
static constexpr uint8_t USB_SUBCLASS_COMMON = 0x02;
static constexpr uint8_t USB_SUBCLASS_NULL = 0x00;
static constexpr uint8_t USB_PROTOCOL_NULL = 0x00;
static constexpr uint8_t USB_DEVICE_PROTOCOL_IAD = 0x01;
static constexpr uint8_t USB_VENDOR_IFC = usb_host::USB_TYPE_VENDOR | usb_host::USB_RECIP_INTERFACE;
static constexpr uint8_t USB_VENDOR_DEV = usb_host::USB_TYPE_VENDOR | usb_host::USB_RECIP_DEVICE;

struct CdcEps {
  const usb_ep_desc_t *notify_ep;
  const usb_ep_desc_t *in_ep;
  const usb_ep_desc_t *out_ep;
  uint8_t bulk_interface_number;
  uint8_t interrupt_interface_number;
};

enum CH34xChipType : uint8_t {
  CHIP_CH342F = 0,
  CHIP_CH342K,
  CHIP_CH343GP,
  CHIP_CH343G_AUTOBAUD,
  CHIP_CH343K,
  CHIP_CH343J,
  CHIP_CH344L,
  CHIP_CH344L_V2,
  CHIP_CH344Q,
  CHIP_CH347TF,
  CHIP_CH9101UH,
  CHIP_CH9101RY,
  CHIP_CH9102F,
  CHIP_CH9102X,
  CHIP_CH9103M,
  CHIP_CH9104L,
  CHIP_CH340B,
  CHIP_CH339W,
  CHIP_CH9111L_M0,
  CHIP_CH9111L_M1,
  CHIP_CH9114L,
  CHIP_CH9114W,
  CHIP_CH9114F,
  CHIP_CH346C_M0,
  CHIP_CH346C_M1,
  CHIP_CH346C_M2,
  CHIP_UNKNOWN = 0xFF,
};

enum UARTParityOptions {
  UART_CONFIG_PARITY_NONE = 0,
  UART_CONFIG_PARITY_ODD,
  UART_CONFIG_PARITY_EVEN,
  UART_CONFIG_PARITY_MARK,
  UART_CONFIG_PARITY_SPACE,
};

enum UARTStopBitsOptions {
  UART_CONFIG_STOP_BITS_1 = 0,
  UART_CONFIG_STOP_BITS_1_5,
  UART_CONFIG_STOP_BITS_2,
};

static const char *const PARITY_NAMES[] = {"NONE", "ODD", "EVEN", "MARK", "SPACE"};
static const char *const STOP_BITS_NAMES[] = {"1", "1.5", "2"};

class RingBuffer {
 public:
  RingBuffer(uint16_t buffer_size) : buffer_size_(buffer_size), buffer_(new uint8_t[buffer_size]) {}
  bool is_empty() const { return this->read_pos_ == this->insert_pos_; }
  size_t get_available() const {
    return (this->insert_pos_ + this->buffer_size_ - this->read_pos_) % this->buffer_size_;
  };
  size_t get_free_space() const { return this->buffer_size_ - 1 - this->get_available(); }
  uint8_t peek() const { return this->buffer_[this->read_pos_]; }
  void push(uint8_t item);
  void push(const uint8_t *data, size_t len);
  uint8_t pop();
  size_t pop(uint8_t *data, size_t len);
  void clear() { this->read_pos_ = this->insert_pos_ = 0; }

 protected:
  uint16_t insert_pos_ = 0;
  uint16_t read_pos_ = 0;
  uint16_t buffer_size_;
  uint8_t *buffer_;
};

// Structure for queuing received USB data chunks
struct UsbDataChunk {
  uint8_t data[usb_host::USB_MAX_PACKET_SIZE];
  uint16_t length;
  USBUartChannel *channel;

  // Required for EventPool - no cleanup needed for POD types
  void release() {}
};

// Structure for queuing outgoing USB data chunks (one per USB packet)
struct UsbOutputChunk {
  static constexpr size_t MAX_CHUNK_SIZE = usb_host::USB_MAX_PACKET_SIZE;
  uint8_t data[MAX_CHUNK_SIZE];
  uint16_t length;

  // Required for EventPool - no cleanup needed for POD types
  void release() {}
};

class USBUartChannel : public uart::UARTComponent, public Parented<USBUartComponent> {
  friend class USBUartComponent;
  friend class USBUartTypeCdcAcm;
  friend class USBUartTypeCP210X;
  friend class USBUartTypeCH34X;

 public:
  // Number of output chunk slots per channel, derived from buffer_size config.
  // Computed as ceil(buffer_size / 64) + 1 in Python codegen; defaults to 5 (256 / 64 + 1).
  static constexpr uint8_t USB_OUTPUT_CHUNK_COUNT = USB_UART_OUTPUT_CHUNK_COUNT;

  USBUartChannel(uint8_t index, uint16_t buffer_size) : index_(index), input_buffer_(RingBuffer(buffer_size)) {}
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return this->input_buffer_.get_available(); }
  bool is_connected() override { return this->initialised_.load(); }
  uart::UARTFlushResult flush() override;
  void check_logger_conflict() override {}
  void set_parity(UARTParityOptions parity) { this->parity_ = parity; }
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_dummy_receiver(bool dummy_receiver) { this->dummy_receiver_ = dummy_receiver; }
  void set_debug_prefix(const char *prefix) { this->debug_prefix_ = StringRef(prefix); }
  void set_flush_timeout(uint32_t flush_timeout_ms) override { this->flush_timeout_ms_ = flush_timeout_ms; }

  /// Register a callback invoked immediately after data is pushed to the input ring buffer.
  /// Called from USBUartComponent::loop() in the main loop context.
  /// Allows consumers (e.g. ZigbeeProxy) to process bytes in the same loop iteration
  /// they arrive, eliminating one full main-loop-wakeup cycle of latency.
  void set_rx_callback(std::function<void()> cb) { this->rx_callback_ = std::move(cb); }

 protected:
  // Larger structures first (8+ bytes)
  RingBuffer input_buffer_;
  LockFreeQueue<UsbOutputChunk, USB_OUTPUT_CHUNK_COUNT> output_queue_;
  // Pool sized to queue capacity (SIZE-1) because LockFreeQueue<T,N> is a ring
  // buffer that holds N-1 elements. This guarantees allocate() returns nullptr
  // before push() can fail, preventing a pool slot leak.
  EventPool<UsbOutputChunk, USB_OUTPUT_CHUNK_COUNT - 1> output_pool_;
  std::function<void()> rx_callback_{};
  CdcEps cdc_dev_{};
  StringRef debug_prefix_{};
  // 4-byte fields
  UARTParityOptions parity_{UART_CONFIG_PARITY_NONE};
  uint32_t flush_timeout_ms_{100};
  // 1-byte fields (no padding between groups)
  std::atomic<bool> input_started_{true};
  std::atomic<bool> output_started_{true};
  std::atomic<bool> initialised_{false};
  const uint8_t index_;
  bool debug_{};
  bool dummy_receiver_{};
};

class USBUartComponent : public usb_host::USBClient {
 public:
  USBUartComponent(uint16_t vid, uint16_t pid) : usb_host::USBClient(vid, pid) {}
  void setup() override;
  void loop() override;
  void dump_config() override;
  std::vector<USBUartChannel *> get_channels() { return this->channels_; }

  void add_channel(USBUartChannel *channel) { this->channels_.push_back(channel); }

  void start_input(USBUartChannel *channel);
  void start_output(USBUartChannel *channel);

  // Lock-free data transfer from USB task to main loop
  static constexpr int USB_DATA_QUEUE_SIZE = 32;
  LockFreeQueue<UsbDataChunk, USB_DATA_QUEUE_SIZE> usb_data_queue_;
  // Pool sized to queue capacity (SIZE-1) — see USBUartChannel::output_pool_ comment.
  EventPool<UsbDataChunk, USB_DATA_QUEUE_SIZE - 1> chunk_pool_;

 protected:
  std::vector<USBUartChannel *> channels_{};
};

class USBUartTypeCdcAcm : public USBUartComponent {
 public:
  USBUartTypeCdcAcm(uint16_t vid, uint16_t pid) : USBUartComponent(vid, pid) {}

 protected:
  virtual std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl);
  void on_connected() override;
  void on_disconnected() override;
  virtual void enable_channels();
  /// Resets per-channel transfer flags and posts the first bulk IN transfer.
  /// Called by enable_channels() and by vendor-specific subclass overrides that
  /// handle their own line-coding setup before starting data flow.
  void start_channels();
};

class USBUartTypeCP210X : public USBUartTypeCdcAcm {
 public:
  USBUartTypeCP210X(uint16_t vid, uint16_t pid) : USBUartTypeCdcAcm(vid, pid) {}

 protected:
  std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl) override;
  void enable_channels() override;
};
class USBUartTypeCH34X : public USBUartTypeCdcAcm {
 public:
  USBUartTypeCH34X(uint16_t vid, uint16_t pid) : USBUartTypeCdcAcm(vid, pid) {}
  void dump_config() override;

 protected:
  void enable_channels() override;
  std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl) override;

 private:
  void apply_line_settings_();
  CH34xChipType chiptype_{CHIP_UNKNOWN};
  const char *chip_name_{"unknown"};
  uint8_t num_ports_{1};
};

}  // namespace esphome::usb_uart

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
