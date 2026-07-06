#pragma once

// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include <memory>
#include <vector>
#include "usb/usb_host.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esphome/core/lock_free_queue.h"
#include "esphome/core/event_pool.h"
#include <atomic>

namespace esphome::usb_host {

// THREADING MODEL:
// This component uses a dedicated USB task for event processing to prevent data loss.
// - USB Task (high priority): Handles USB events, executes transfer callbacks, releases transfer slots
// - Main Loop Task: Initiates transfers, processes device connect/disconnect events
//
// Thread-safe communication:
// - Lock-free queues for USB task -> main loop events (SPSC pattern)
// - Lock-free TransferRequest pool using atomic bitmask (MCMP pattern)
//
// Transfer submission engine (USBHost) is stateless w.r.t. client memory — the
// Linux URB model: client owns the pool, fills a slot, hands it to USBHost.

static const char *const TAG = "usb_host";

// Forward declarations
struct TransferRequest;
class USBClient;
class USBHost;

// constants for setup packet type
static constexpr uint8_t USB_RECIP_DEVICE = 0;
static constexpr uint8_t USB_RECIP_INTERFACE = 1;
static constexpr uint8_t USB_RECIP_ENDPOINT = 2;
static constexpr uint8_t USB_TYPE_STANDARD = 0 << 5;
static constexpr uint8_t USB_TYPE_CLASS = 1 << 5;
static constexpr uint8_t USB_TYPE_VENDOR = 2 << 5;
static constexpr uint8_t USB_DIR_MASK = 1 << 7;
static constexpr uint8_t USB_DIR_IN = 1 << 7;
static constexpr uint8_t USB_DIR_OUT = 0;
static constexpr size_t SETUP_PACKET_SIZE = 8;

static constexpr size_t MAX_REQUESTS = USB_HOST_MAX_REQUESTS;
static_assert(MAX_REQUESTS >= 1 && MAX_REQUESTS <= 32, "MAX_REQUESTS must be between 1 and 32");

using trq_bitmask_t = std::conditional<(MAX_REQUESTS <= 16), uint16_t, uint32_t>::type;
static constexpr trq_bitmask_t ALL_REQUESTS_IN_USE = MAX_REQUESTS == 32 ? ~0 : (1 << MAX_REQUESTS) - 1;

static constexpr size_t USB_MAX_PACKET_SIZE = USB_HOST_MAX_PACKET_SIZE;
static constexpr size_t USB_EVENT_QUEUE_SIZE = 32;
static constexpr size_t USB_TASK_STACK_SIZE = 4096;
static constexpr UBaseType_t USB_TASK_PRIORITY = 5;

// Transfer status reported to callback
struct TransferStatus {
  uint8_t *data;
  size_t data_len;
  void *user_data;
  uint16_t error_code;
  uint8_t endpoint;
  bool success;
};

using transfer_cb_t = std::function<void(const TransferStatus &)>;

// TransferRequest — our URB equivalent.
// Client owns the pool, fills a slot, passes it to USBHost::submit_*().
// USBHost never allocates or frees these.
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
  void release() {}
};

enum ClientState {
  USB_CLIENT_INIT = 0,
  USB_CLIENT_OPEN,
  USB_CLIENT_CLOSE,
  USB_CLIENT_GET_DESC,
  USB_CLIENT_GET_INFO,
  USB_CLIENT_CONNECTED,
};

// ─────────────────────────────────────────────────────────────────────────────
// Isochronous stream types — compiled only when USE_USB_ISOC_TRANSFERS is set.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef USE_USB_ISOC_TRANSFERS

struct IsocCbCtx {
  USBClient *client;  // vtable dispatch target for on_isoc_packet()
  struct IsocStream *stream;
};

struct IsocStream {
  std::unique_ptr<usb_transfer_t *[]> xfers {};
  std::unique_ptr<IsocCbCtx[]> ctxs{};
  std::atomic<uint8_t> pending_urbs{0};
  uint8_t num_urbs{0};
  uint8_t ep_addr{0};
  uint16_t mps{0};
  uint8_t packets_per_urb{0};
  uint8_t interface_num{0};
  uint8_t alt_setting{0};
  bool streaming{false};
};

#endif  // USE_USB_ISOC_TRANSFERS

// ─────────────────────────────────────────────────────────────────────────────
// USBClient — device state machine + thin forwarding layer to USBHost.
// ─────────────────────────────────────────────────────────────────────────────
class USBClient : public Component {
  friend class USBHost;

 public:
  USBClient(uint16_t vid, uint16_t pid) : trq_in_use_(0), vid_(vid), pid_(pid) {}
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void on_opened(uint8_t addr);
  virtual void on_removed(usb_device_handle_t handle);
  void dump_config() override;
  void release_trq(TransferRequest *trq);
  trq_bitmask_t get_trq_in_use() const { return trq_in_use_; }

  void set_required_interface_class(uint8_t cls) {
    this->match_any_interface_class_ = false;
    this->required_interface_class_ = cls;
  }

  // Lock-free event queue and pool — public for static callbacks
  LockFreeQueue<UsbEvent, USB_EVENT_QUEUE_SIZE> event_queue;
  EventPool<UsbEvent, USB_EVENT_QUEUE_SIZE - 1> event_pool;

  // ── Bulk / interrupt transfers ──────────────────────────────────────────────
#ifdef USE_USB_BULK_TRANSFERS
  bool transfer_in(uint8_t ep_address, const transfer_cb_t &callback, uint16_t length);
  bool transfer_out(uint8_t ep_address, const transfer_cb_t &callback, const uint8_t *data, uint16_t length);
#endif

  // ── Control transfers ───────────────────────────────────────────────────────
#ifdef USE_USB_CONTROL_TRANSFERS
  bool control_transfer(uint8_t type, uint8_t request, uint16_t value, uint16_t index, const transfer_cb_t &callback,
                        const std::vector<uint8_t> &data = {});
#endif

  // ── Interface claim / release / alt-setting ─────────────────────────────────
  bool claim_interface(uint8_t interface_num, uint8_t alt_setting = 0);
  bool release_interface(uint8_t interface_num);

#ifdef USE_USB_CONTROL_TRANSFERS
  bool set_interface(uint8_t interface_num, uint8_t alt_setting);
#endif

  // ── Isochronous support ─────────────────────────────────────────────────────
#ifdef USE_USB_ISOC_TRANSFERS
  usb_transfer_t *isoc_alloc(uint8_t ep_addr, uint16_t mps, uint8_t num_packets, usb_transfer_cb_t callback,
                             void *context);
  bool isoc_submit(usb_transfer_t *xfer);
  void isoc_free(usb_transfer_t *xfer);

  bool stream_open(IsocStream &stream, USBClient *cb);
  void stream_close(IsocStream &stream);

  // Override in subclass to process one isochronous packet.
  // Called from USB-task context — must be fast and non-blocking.
  virtual void on_isoc_packet(uint8_t ep_addr, const uint8_t *data, size_t len, bool error) {}
#endif

 protected:
  bool process_usb_events_();
  void handle_open_state_();
  TransferRequest *get_trq_();
  virtual void disconnect();
  virtual void on_connected() {}
  virtual void on_disconnected() { this->trq_in_use_.store(0); }

  static void usb_task_fn(void *arg);
  [[noreturn]] void usb_task_loop_() const;
  static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg);

  TransferRequest requests_[MAX_REQUESTS]{};
  TaskHandle_t usb_task_handle_{nullptr};
  usb_host_client_handle_t handle_{};
  usb_device_handle_t device_handle_{};
  const usb_device_desc_t *device_desc_{nullptr};
  const usb_config_desc_t *config_desc_{nullptr};
  int device_addr_{-1};
  int state_{USB_CLIENT_INIT};
  std::atomic<trq_bitmask_t> trq_in_use_;
  uint16_t vid_{};
  uint16_t pid_{};
  bool match_any_interface_class_{true};
  uint8_t required_interface_class_{0};

  const usb_device_desc_t *get_device_desc_() const { return this->device_desc_; }
  const usb_config_desc_t *get_config_desc_() const { return this->config_desc_; }
};
// ─────────────────────────────────────────────────────────────────────────────
// USBHost — USB host stack + stateless transfer submission engine.
//
// Acts as the Linux host controller driver: owns usb_host_install(), the lib
// event loop, and all ESP-IDF transfer submission calls.  Never touches client
// memory — clients own their TransferRequest pools and hand filled slots here.
// ─────────────────────────────────────────────────────────────────────────────
class USBHost final : public Component {
 public:
  float get_setup_priority() const override { return setup_priority::BUS; }
  void loop() override;
  void setup() override;

  // ── Submission engine (called by USBClient thin forwarders) ────────────────

  // Bulk / interrupt IN and OUT — always compiled if any client uses them
  bool submit_transfer(TransferRequest *trq);

  // Control transfer submission — guarded
#ifdef USE_USB_CONTROL_TRANSFERS
  bool submit_control(usb_host_client_handle_t client_handle, TransferRequest *trq);
  bool do_set_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                        uint8_t interface_num, uint8_t alt_setting);
#endif

  // Interface claim / release — always needed
  bool do_claim_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                          uint8_t interface_num, uint8_t alt_setting);
  bool do_release_interface(usb_host_client_handle_t client_handle, usb_device_handle_t device_handle,
                            uint8_t interface_num);

  // ── Isochronous ─────────────────────────────────────────────────────────────
#ifdef USE_USB_ISOC_TRANSFERS
  usb_transfer_t *do_isoc_alloc(uint8_t ep_addr, usb_device_handle_t device_handle, uint16_t mps, uint8_t num_packets,
                                usb_transfer_cb_t callback, void *context);
  bool do_isoc_submit(usb_transfer_t *xfer);
  void do_isoc_free(usb_transfer_t *xfer);

  bool stream_open(IsocStream &stream, USBClient *cb, usb_host_client_handle_t client_handle,
                   usb_device_handle_t device_handle);
  void stream_close(IsocStream &stream, usb_host_client_handle_t client_handle, usb_device_handle_t device_handle);

  // Static trampoline stored as xfer->callback for every ISOC URB.
  // Iterates isoc_packet_desc[], calls client->on_isoc_packet() per packet, resubmits.
  static void isoc_cb(usb_transfer_t *xfer);
#endif

 protected:
  std::vector<USBClient *> clients_{};
};

// Returns the global USBHost singleton, set during USBHost::setup().
USBHost *get_usb_host();

}  // namespace esphome::usb_host

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
