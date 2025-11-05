#pragma once

#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart_component.h"
#include "esphome/components/usb_host/usb_host.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"
#include <atomic>

namespace esphome {
namespace usb_uart {
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

static constexpr uint16_t CH9344_TTY_MINORS = 256;

struct CdcEps {
  const usb_ep_desc_t *notify_ep;
  const usb_ep_desc_t *in_ep;
  const usb_ep_desc_t *out_ep;
  uint8_t bulk_interface_number;
  uint8_t interrupt_interface_number;
};

struct Ch934xEps {
  const usb_ep_desc_t *in_ep;
  const usb_ep_desc_t *out_ep;
  const usb_ep_desc_t *ep_cmd_read;
  const usb_ep_desc_t *ep_cmd_write;
  uint8_t data_interface = 0;
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

#if defined(USE_UART_DEBUGGER)
enum CH934X_CHIPTYPE {
  CHIP_CH9344L = 0,
  CHIP_CH9344Q,
  CHIP_CH348L,
  CHIP_CH348Q,
};

enum CH34X_CHIPTYPE {
  CHIP_CH342F = 0x00,
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
};
#endif

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
  uint8_t data[esphome::usb_host::USB_MAX_PACKET_SIZE];
#if defined(USE_ESP32_VARIANT_ESP32P4)
  uint16_t length;
#else
  uint8_t length;
#endif
  USBUartChannel *channel;

  // Required for EventPool - no cleanup needed for POD types
  void release() {}
};

class USBUartChannel : public uart::UARTComponent, public Parented<USBUartComponent> {
  friend class USBUartComponent;
  friend class USBUartTypeCdcAcm;
  friend class USBUartTypeCP210X;
  friend class USBUartTypeCH34X;
  friend class USBUartTypeCH934X;
  friend class USBUartTypeFT23XX;

 public:
  USBUartChannel(uint8_t index, uint16_t buffer_size)
      : index_(index), input_buffer_(RingBuffer(buffer_size)), output_buffer_(RingBuffer(buffer_size)) {}
  void write_array(const uint8_t *data, size_t len) override;
  ;
  bool peek_byte(uint8_t *data) override;
  ;
  bool read_array(uint8_t *data, size_t len) override;
  int available() override { return static_cast<int>(this->input_buffer_.get_available()); }
  void flush() override {}
  void check_logger_conflict() override {}
  void set_parity(UARTParityOptions parity) { this->parity_ = parity; }
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_dummy_receiver(bool dummy_receiver) { this->dummy_receiver_ = dummy_receiver; }
#ifdef USE_UART_DEBUGGER
  void set_debug_prefix(const std::string &prefix) { this->debug_prefix_ = prefix; }
  void set_debug_add_settings(bool add) { this->debug_add_settings_ = add; }
#endif

 protected:
  // Larger structures first for better alignment
  RingBuffer input_buffer_;
  RingBuffer output_buffer_;
  CdcEps cdc_dev_{};
  // Enum (likely 4 bytes)
  UARTParityOptions parity_{UART_CONFIG_PARITY_NONE};
  // Group atomics together (each 1 byte)
  std::atomic<bool> input_started_{false};
  std::atomic<bool> output_started_{false};
  std::atomic<bool> initialised_{false};  // Set to true only after configuration complete and RX started
  // Group regular bytes together to minimize padding
  const uint8_t index_;
  bool debug_{};
  bool dummy_receiver_{};
#ifdef USE_UART_DEBUGGER
  std::string get_debug_prefix();
  std::string debug_prefix_;
  bool debug_add_settings_{true};
#endif
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
  virtual void start_output(USBUartChannel *channel);

  // Lock-free data transfer from USB task to main loop
  static constexpr int USB_DATA_QUEUE_SIZE = 32;
  LockFreeQueue<UsbDataChunk, USB_DATA_QUEUE_SIZE> usb_data_queue_;
  EventPool<UsbDataChunk, USB_DATA_QUEUE_SIZE> chunk_pool_;

 protected:
  std::vector<USBUartChannel *> channels_{};
};

class USBUartTypeCdcAcm : public USBUartComponent {
 public:
  USBUartTypeCdcAcm(uint16_t vid, uint16_t pid) : USBUartComponent(vid, pid) {}

 protected:
  virtual std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl);
  void on_connected() override;
  virtual void enable_channels();
  void on_disconnected() override;
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

 protected:
  void enable_channels() override;
};

class USBUartTypeFT23XX : public USBUartTypeCdcAcm {
 public:
  USBUartTypeFT23XX(uint16_t vid, uint16_t pid) : USBUartTypeCdcAcm(vid, pid) {}

  virtual void start_input(USBUartChannel *channel);

 protected:
  std::vector<CdcEps> parse_descriptors(usb_device_handle_t dev_hdl) override;
  void enable_channels() override;

  int reset(USBUartChannel *channel);
  int set_baudrate(USBUartChannel *channel, uint32_t baudrate = 0);
  int set_line_properties(USBUartChannel *channel);
  int set_dtr_rts(USBUartChannel *channel);

  uint8_t chip_type_{255};
  bool device_init_complete_{false};
};

class USBUartTypeCH934X : public USBUartComponent {
 public:
  USBUartTypeCH934X(uint16_t vid, uint16_t pid) : USBUartComponent(vid, pid) {
    ESP_LOGI("usb_uart", "=== CH934X CONSTRUCTOR CALLED! VID=%04X PID=%04X ===", vid, pid);
  }

  // Overridden methods from USBUartComponent
  void start_input(USBUartChannel *channel) override;
  void start_output(USBUartChannel *channel) override;
  bool parse_descriptors(usb_device_handle_t dev_hdl);
  void enable_channels();

  // ESPHome component loop - handles RX queue processing and TX multiplexing
  void loop() override;

 protected:
  void on_connected() override;
  void on_disconnected() override;
  // Device-level configuration
  void configure_device_();
  void configure_channels_after_detection_();  // Called via defer after chip detection

  // Channel-level configuration
  bool configure_channel_(USBUartChannel *channel);
  bool set_uart_mode(USBUartChannel *channel);
  bool configure_uart_parameters_(USBUartChannel *channel);

  // Multiplexed RX handling (demultiplexes to channels)
  void start_rx_reader_();
  void handle_rx_data_(const uint8_t *data, size_t len);

  // Multiplexed TX handling (sends data from channels)
  void handle_tx_multiplexing_();
  void send_next_channel_data_(USBUartChannel *channel);

  // Command channel for status updates
  void start_command_reader_();
  void handle_command_data_(const uint8_t *data, size_t len);

  // Helper functions
  uint8_t get_reg_address_(uint8_t portnum);

 private:
  // Device information
  Ch934xEps uart_host_dev_{};
  uint16_t pid_ = 0;
  uint8_t chiptype_ = 0;
  uint8_t chipversion_ = 0;
  uint8_t num_ports_ = 0;
  uint8_t port_offset_ = 0;
  bool bpackload_ = false;

  // RX/CMD reader state
  std::atomic<bool> rx_running_{false};
  std::atomic<bool> cmd_running_{false};

  // TX multiplexing state
  size_t tx_current_channel_index_ = 0;
  std::atomic<bool> tx_in_progress_{false};
};

}  // namespace usb_uart
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
