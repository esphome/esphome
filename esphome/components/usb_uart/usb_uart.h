#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
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
class USBUartTypePL2303;

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

class USBUartChannel final : public uart::UARTComponent, public Parented<USBUartComponent> {
  friend class USBUartComponent;
  friend class USBUartTypeCdcAcm;
  friend class USBUartTypeCP210X;
  friend class USBUartTypeCH34X;
  friend class USBUartTypeFT23XX;
  friend class USBUartTypePL2303;

 public:
  // Number of output chunk slots per channel, derived from buffer_size config.
  // Computed as ceil(buffer_size / 64) + 1 in Python codegen; defaults to 5 (256 / 64 + 1).
  static constexpr uint8_t USB_OUTPUT_CHUNK_COUNT = USB_UART_OUTPUT_CHUNK_COUNT;

  USBUartChannel(uint8_t index, uint16_t buffer_size) : input_buffer_(RingBuffer(buffer_size)), index_(index) {}
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  size_t available() override { return this->input_buffer_.get_available(); }
  bool is_connected() override { return this->initialised_.load(); }
  uart::UARTFlushResult flush() override;
  // Re-apply the current line settings (baud, parity, etc) to this already-open channel.
  void load_settings(bool dump_config) override;
  using UARTComponent::load_settings;  // also bring in the no-arg overload for convenience
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
  void check_logger_conflict() override {}
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

  virtual void start_input(USBUartChannel *channel);
  void start_output(USBUartChannel *channel);

  // Begin configuring all channels (full initialisation). Called from on_connected().
  void enable_channels();
  // Re-apply line settings to a single, already-open channel (used by
  // USBUartChannel::load_settings()).
  void apply_channel_settings(USBUartChannel *channel);

  // Called from loop() when input_buffer_ has insufficient space for the incoming chunk.
  // Default is a no-op; override in device-specific subclasses that need resync on overflow.
  virtual void on_rx_overflow(USBUartChannel *channel) {}

  // Lock-free data transfer from USB task to main loop
  static constexpr int USB_DATA_QUEUE_SIZE = 32;
  LockFreeQueue<UsbDataChunk, USB_DATA_QUEUE_SIZE> usb_data_queue_;
  // Pool sized to queue capacity (SIZE-1) — see USBUartChannel::output_pool_ comment.
  EventPool<UsbDataChunk, USB_DATA_QUEUE_SIZE - 1> chunk_pool_;

 protected:
  // Issue one control transfer as part of the setup state machine. The completion
  // callback (USB-task context) records the result/IN data, marks the step done and
  // wakes the loop so run_config_machine_() advances on the loop thread. Call exactly
  // once from config_step_()/config_device_step_() when issuing a step.
  void config_transfer_(uint8_t type, uint8_t request, uint16_t value, uint16_t index,
                        const std::vector<uint8_t> &data = {});
  // (Re)start the config state machine. reload=false runs full init over all channels;
  // reload=true re-applies settings to cfg_single_ only.
  void start_config_(bool reload);
  // Advance the config state machine; called from loop(). Returns true if it did work.
  bool run_config_machine_();

  // Per-subclass per-channel settings sequence. For the given zero-based step, issue the
  // next control transfer via config_transfer_() and return true, or return false when the
  // channel has no more steps. reload=true ⇒ apply only baud/parity/stop/data (skip
  // enable/reset/DTR-RTS). ok/response carry the previous step's result and IN data.
  virtual bool config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok, const uint8_t *response) = 0;
  // Optional one-time device-level setup run before the per-channel phase on init only
  // (e.g. CH34x chip detection). Same contract as config_step_(). Default: no steps.
  virtual bool config_device_step(uint8_t step, bool ok, const uint8_t *response) { return false; }

  std::vector<USBUartChannel *> channels_{};

  // Config state machine
  USBUartChannel *cfg_single_{nullptr};          // non-null: reload of a single channel
  USBUartChannel *cfg_pending_reload_{nullptr};  // reload requested while the machine was busy
  std::atomic<bool> cfg_done_{false};            // synchronizes cfg_ok_/cfg_response_ across threads
  uint8_t cfg_response_[8]{};                    // last IN transfer payload (for detection reads)
  uint8_t cfg_channel_idx_{0};
  uint8_t cfg_step_{0};
  bool cfg_active_{false};
  bool cfg_reload_{false};
  bool cfg_device_phase_{false};
  bool cfg_in_flight_{false};
  bool cfg_ok_{true};
};

class USBUartTypeCdcAcm : public USBUartComponent {
 public:
  USBUartTypeCdcAcm(uint16_t vid, uint16_t pid) : USBUartComponent(vid, pid) {}

 protected:
  virtual std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl);
  void on_connected() override;
  void on_disconnected() override;
  bool config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok, const uint8_t *response) override;
};

class USBUartTypeCP210X : public USBUartTypeCdcAcm {
 public:
  USBUartTypeCP210X(uint16_t vid, uint16_t pid) : USBUartTypeCdcAcm(vid, pid) {}

 protected:
  std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl) override;
  bool config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok, const uint8_t *response) override;
};
class USBUartTypeCH34X : public USBUartTypeCdcAcm {
 public:
  USBUartTypeCH34X(uint16_t vid, uint16_t pid) : USBUartTypeCdcAcm(vid, pid) {}
  void dump_config() override;

 protected:
  bool config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok, const uint8_t *response) override;
  bool config_device_step(uint8_t step, bool ok, const uint8_t *response) override;
  std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl) override;

 private:
  CH34xChipType chiptype_{CHIP_UNKNOWN};
  const char *chip_name_{"unknown"};
  uint8_t num_ports_{1};
};

class USBUartTypeFT23XX : public USBUartTypeCdcAcm {
 public:
  USBUartTypeFT23XX(uint16_t vid, uint16_t pid) : USBUartTypeCdcAcm(vid, pid) {}

  void start_input(USBUartChannel *channel) override;
  void on_rx_overflow(USBUartChannel *channel) override;

 protected:
  std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl) override;
  bool config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok, const uint8_t *response) override;

  uint8_t chip_type_{255};
};

enum Pl2303ChipType : uint8_t {
  PL2303_TYPE_H = 0,  // Legacy, max 1.2Mbaud
  PL2303_TYPE_HX,     // max 6Mbaud, divisor encoding
  PL2303_TYPE_TA,     // max 6Mbaud, alt divisor encoding
  PL2303_TYPE_TB,     // max 12Mbaud, alt divisor encoding
  PL2303_TYPE_HXD,    // max 12Mbaud, divisor encoding
  PL2303_TYPE_HXN,    // G-series, max 12Mbaud, direct encoding only
  PL2303_TYPE_UNKNOWN = 0xFF,
};

class USBUartTypePL2303 : public USBUartTypeCdcAcm {
  friend class USBUartChannel;

 public:
  USBUartTypePL2303(uint16_t vid, uint16_t pid) : USBUartTypeCdcAcm(vid, pid) {}

 protected:
  std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl) override;
  bool config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok, const uint8_t *response) override;

  Pl2303ChipType chip_type_{PL2303_TYPE_UNKNOWN};
};

}  // namespace esphome::usb_uart

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
