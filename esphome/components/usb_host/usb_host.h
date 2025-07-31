#pragma once

// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "esphome/core/component.h"
#include <vector>
#include "usb/usb_host.h"

#include <list>

namespace esphome {
namespace usb_host {

static const char *const TAG = "usb_host";

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

static const size_t MAX_REQUESTS = 16;  // maximum number of outstanding requests possible.

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
  USBClient(uint16_t vid, uint16_t pid) : vid_(vid), pid_(pid) { init_pool(); }

  void init_pool() {
    this->trq_pool_.clear();
    for (size_t i = 0; i != MAX_REQUESTS; i++)
      this->trq_pool_.push_back(&this->requests_[i]);
  }
  void setup() override;
  void loop() override;
  // setup must happen after the host bus has been setup
  float get_setup_priority() const override { return setup_priority::IO; }
  void on_opened(uint8_t addr);
  void on_removed(usb_device_handle_t handle);
  void control_transfer_callback(const usb_transfer_t *xfer) const;
  void transfer_in(uint8_t ep_address, const transfer_cb_t &callback, uint16_t length);
  void transfer_out(uint8_t ep_address, const transfer_cb_t &callback, const uint8_t *data, uint16_t length);
  void dump_config() override;
  void release_trq(TransferRequest *trq);
  bool control_transfer(uint8_t type, uint8_t request, uint16_t value, uint16_t index, const transfer_cb_t &callback,
                        const std::vector<uint8_t> &data = {});

 protected:
  bool register_();
  TransferRequest *get_trq_();
  virtual void disconnect();
  virtual void on_connected() {}
  virtual void on_disconnected() { this->init_pool(); }

  usb_host_client_handle_t handle_{};
  usb_device_handle_t device_handle_{};
  int device_addr_{-1};
  int state_{USB_CLIENT_INIT};
  uint16_t vid_{};
  uint16_t pid_{};
  std::list<TransferRequest *> trq_pool_{};
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

#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
