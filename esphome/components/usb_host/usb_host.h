#pragma once

// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include <vector>
#include "usb/usb_host.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"
#include <atomic>

namespace esphome {
namespace usb_host {

// THREADING MODEL:
// This component uses a dedicated USB task for event processing to prevent data loss.
// - USB Task (high priority): Handles USB events, executes transfer callbacks, releases transfer slots
// - Main Loop Task: Initiates transfers, processes device connect/disconnect events
//
// Thread-safe communication:
// - Lock-free queues for USB task -> main loop events (SPSC pattern)
// - Lock-free TransferRequest pool using atomic bitmask (MCMP pattern - multi-consumer, multi-producer)
//
// TransferRequest pool access pattern:
// - get_trq_() [allocate]: Called from BOTH USB task and main loop threads
//   * USB task: via USB UART input callbacks that restart transfers immediately
//   * Main loop: for output transfers and flow-controlled input restarts
// - release_trq() [deallocate]: Called from BOTH USB task and main loop threads
//   * USB task: immediately after transfer callback completes (critical for preventing slot exhaustion)
//   * Main loop: when transfer submission fails
//
// The multi-threaded allocation/deallocation is intentional for performance:
// - USB task can immediately restart input transfers and release slots without context switching
// - Main loop controls backpressure by deciding when to restart after consuming data
// The atomic bitmask ensures thread-safe allocation/deallocation without mutex blocking.

static const char *const TAG = "usb_host";

// Forward declarations
struct TransferRequest;
class USBClient;

// constants for setup packet type
static const uint8_t USB_RECIP_DEVICE = 0;
static const uint8_t USB_RECIP_INTERFACE = 1;
static const uint8_t USB_RECIP_ENDPOINT = 2;
static const uint8_t USB_TYPE_STANDARD = 0 << 5;
static const uint8_t USB_TYPE_CLASS = 1 << 5;
static const uint8_t USB_TYPE_VENDOR = 2 << 5;
static const uint8_t USB_DIR_MASK = 1 << 7;
static const uint8_t USB_DIR_IN = 1 << 7;
static const uint8_t USB_DIR_OUT = 0;
static const size_t SETUP_PACKET_SIZE = 8;

static constexpr size_t MAX_REQUESTS = USB_HOST_MAX_REQUESTS;  // maximum number of outstanding requests possible.
static_assert(MAX_REQUESTS >= 1 && MAX_REQUESTS <= 32, "MAX_REQUESTS must be between 1 and 32");

// Select appropriate bitmask type for tracking allocation of TransferRequest slots.
// The bitmask must have at least as many bits as MAX_REQUESTS, so:
// - Use uint16_t for up to 16 requests (MAX_REQUESTS <= 16)
// - Use uint32_t for 17-32 requests (MAX_REQUESTS > 16)
// This is tied to the static_assert above, which enforces MAX_REQUESTS is between 1 and 32.
// If MAX_REQUESTS is increased above 32, this logic and the static_assert must be updated.
using trq_bitmask_t = std::conditional<(MAX_REQUESTS <= 16), uint16_t, uint32_t>::type;
static constexpr trq_bitmask_t ALL_REQUESTS_IN_USE = MAX_REQUESTS == 32 ? ~0 : (1 << MAX_REQUESTS) - 1;

static constexpr size_t USB_EVENT_QUEUE_SIZE = 32;   // Size of event queue between USB task and main loop
static constexpr size_t USB_TASK_STACK_SIZE = 4096;  // Stack size for USB task (same as ESP-IDF USB examples)
static constexpr UBaseType_t USB_TASK_PRIORITY = 5;  // Higher priority than main loop (tskIDLE_PRIORITY + 5)

// used to report a transfer status
struct TransferStatus {
  bool success;
  uint16_t error_code;
  uint8_t *data;
  size_t data_len;
  uint8_t endpoint;
  void *user_data;
};

using transfer_cb_t = std::function<void(const TransferStatus &)>;

class USBClient;

// struct used to capture all data needed for a transfer
struct TransferRequest {
  usb_transfer_t *transfer;
  transfer_cb_t callback;
  TransferStatus status;
  USBClient *client;
};

enum EventType : uint8_t {
  EVENT_DEVICE_NEW,
  EVENT_DEVICE_GONE,
};

struct UsbEvent {
  EventType type;
  union {
    struct {
      uint8_t address;
    } device_new;
    struct {
      usb_device_handle_t handle;
    } device_gone;
  } data;

  // Required for EventPool - no cleanup needed for POD types
  void release() {}
};

// callback function type.

enum ClientState {
  USB_CLIENT_INIT = 0,
  USB_CLIENT_OPEN,
  USB_CLIENT_CLOSE,
  USB_CLIENT_GET_DESC,
  USB_CLIENT_GET_INFO,
  USB_CLIENT_CONNECTED,
};
class USBClient : public Component {
  friend class USBHost;

 public:
  USBClient(uint16_t vid, uint16_t pid) : vid_(vid), pid_(pid), trq_in_use_(0) {}
  void setup() override;
  void loop() override;
  // setup must happen after the host bus has been setup
  float get_setup_priority() const override { return setup_priority::IO; }
  void on_opened(uint8_t addr);
  void on_removed(usb_device_handle_t handle);
  bool transfer_in(uint8_t ep_address, const transfer_cb_t &callback, uint16_t length);
  bool transfer_out(uint8_t ep_address, const transfer_cb_t &callback, const uint8_t *data, uint16_t length);
  void dump_config() override;
  void release_trq(TransferRequest *trq);
  trq_bitmask_t get_trq_in_use() const { return trq_in_use_; }
  bool control_transfer(uint8_t type, uint8_t request, uint16_t value, uint16_t index, const transfer_cb_t &callback,
                        const std::vector<uint8_t> &data = {});

  // Lock-free event queue and pool for USB task to main loop communication
  // Must be public for access from static callbacks
  LockFreeQueue<UsbEvent, USB_EVENT_QUEUE_SIZE> event_queue;
  EventPool<UsbEvent, USB_EVENT_QUEUE_SIZE> event_pool;

 protected:
  TransferRequest *get_trq_();  // Lock-free allocation using atomic bitmask (multi-consumer safe)
  virtual void disconnect();
  virtual void on_connected() {}
  virtual void on_disconnected() {
    // Reset all requests to available (all bits to 0)
    this->trq_in_use_.store(0);
  }

  // USB task management
  static void usb_task_fn(void *arg);
  [[noreturn]] void usb_task_loop() const;

  TaskHandle_t usb_task_handle_{nullptr};

  usb_host_client_handle_t handle_{};
  usb_device_handle_t device_handle_{};
  int device_addr_{-1};
  int state_{USB_CLIENT_INIT};
  uint16_t vid_{};
  uint16_t pid_{};
  // Lock-free pool management using atomic bitmask (no dynamic allocation)
  // Bit i = 1: requests_[i] is in use, Bit i = 0: requests_[i] is available
  // Supports multiple concurrent consumers and producers (both threads can allocate/deallocate)
  // Bitmask type automatically selected: uint16_t for <= 16 slots, uint32_t for 17-32 slots
  std::atomic<trq_bitmask_t> trq_in_use_;
  TransferRequest requests_[MAX_REQUESTS]{};
};
class USBHost : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void loop() override;
  void setup() override;

 protected:
  std::vector<USBClient *> clients_{};
};

}  // namespace usb_host
}  // namespace esphome

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
