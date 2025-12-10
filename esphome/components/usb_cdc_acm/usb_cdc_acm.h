#pragma once
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)

#include "esphome/core/component.h"
#include "esphome/core/event_pool.h"
#include "esphome/core/lock_free_queue.h"
#include "esphome/components/uart/uart_component.h"

#include <functional>
#include "freertos/ringbuf.h"
#include "tusb_cdc_acm.h"

namespace esphome::usb_cdc_acm {

static const uint8_t EVENT_QUEUE_SIZE = 12;
static const uint8_t MAX_USB_CDC_INSTANCES = 2;

// Callback types for line coding and line state changes
using LineCodingCallback = std::function<void(uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits)>;
using LineStateCallback = std::function<void(bool dtr, bool rts)>;

// Event types
enum CDCEventType : uint8_t {
  CDC_EVENT_LINE_STATE_CHANGED,
  CDC_EVENT_LINE_CODING_CHANGED,
};

// Event structure for the queue
struct CDCEvent {
  CDCEventType type;
  union {
    struct {
      bool dtr;
      bool rts;
    } line_state;
    struct {
      uint32_t bit_rate;
      uint8_t stop_bits;
      uint8_t parity;
      uint8_t data_bits;
    } line_coding;
  } data;

  // Required by EventPool - called before returning to pool
  void release() {
    // No dynamic memory to clean up, data is stored inline
  }
};

// Forward declaration
class USBCDCACMComponent;

/// Represents a single CDC ACM interface instance
class USBCDCACMInstance : public uart::UARTComponent, public Parented<USBCDCACMComponent> {
 public:
  void set_interface_number(uint8_t itf) { this->itf_ = static_cast<tinyusb_cdcacm_itf_t>(itf); }

  void setup();
  void loop();

  // Get the CDC port number for this instance
  tinyusb_cdcacm_itf_t get_itf() const { return this->itf_; }

  // Ring buffer accessors for bridge components
  RingbufHandle_t get_tx_ringbuf() const { return this->usb_tx_ringbuf_; }
  RingbufHandle_t get_rx_ringbuf() const { return this->usb_rx_ringbuf_; }

  // Task handle accessor for notifying TX task
  TaskHandle_t get_tx_task_handle() const { return this->usb_tx_task_handle_; }

  // Callback registration for line coding and line state changes
  void set_line_coding_callback(LineCodingCallback callback) { this->line_coding_callback_ = std::move(callback); }
  void set_line_state_callback(LineStateCallback callback) { this->line_state_callback_ = std::move(callback); }

  // Called from TinyUSB task context (SPSC producer) - queues event for processing in main loop
  void queue_line_coding_event(uint32_t bit_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits);
  void queue_line_state_event(bool dtr, bool rts);

  static void usb_tx_task_fn(void *arg);
  void usb_tx_task();

  // UARTComponent interface implementation
  void write_array(const uint8_t *data, size_t len) override;
  bool peek_byte(uint8_t *data) override;
  bool read_array(uint8_t *data, size_t len) override;
  int available() override;
  void flush() override;

 protected:
  void check_logger_conflict() override {}

  // Process queued events and invoke callbacks (called from main loop)
  void process_events_();

  TaskHandle_t usb_tx_task_handle_{nullptr};
  tinyusb_cdcacm_itf_t itf_{TINYUSB_CDC_ACM_0};

  RingbufHandle_t usb_tx_ringbuf_{nullptr};
  RingbufHandle_t usb_rx_ringbuf_{nullptr};

  // User-registered callbacks (called from main loop)
  LineCodingCallback line_coding_callback_{nullptr};
  LineStateCallback line_state_callback_{nullptr};

  // Lock-free queue and event pool for cross-task event passing
  EventPool<CDCEvent, EVENT_QUEUE_SIZE> event_pool_;
  LockFreeQueue<CDCEvent, EVENT_QUEUE_SIZE> event_queue_;

  // RX buffer for peek functionality
  uint8_t peek_buffer_{0};
  bool has_peek_{false};
};

/// Main USB CDC ACM component that manages the USB device and all CDC interfaces
class USBCDCACMComponent : public Component {
 public:
  USBCDCACMComponent();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  // Interface management
  void add_interface(USBCDCACMInstance *interface);
  USBCDCACMInstance *get_interface_by_number(uint8_t itf);

 protected:
  std::array<USBCDCACMInstance *, MAX_USB_CDC_INSTANCES> interfaces_{nullptr, nullptr};
};

extern USBCDCACMComponent *global_usb_cdc_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::usb_cdc_acm
#endif
