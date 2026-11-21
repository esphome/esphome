#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "usb_bluetooth.h"
#include "esphome/core/log.h"
#include "usb/usb_helpers.h"

#include <algorithm>
#include <cstring>

#ifdef USE_USB_BLUETOOTH_BLUEDROID
#include "esphome/components/esp32_ble/ble.h"
#include "esp_bluedroid_hci.h"
#endif

namespace esphome::usb_bluetooth {

ESPHOME_LOG_TAG(TAG, "usb_bluetooth");

static constexpr uint32_t COMMAND_TIMEOUT_ID = 0;
static constexpr uint32_t COMMAND_TIMEOUT_MS = 3000;

static constexpr uint8_t HCI_CLASS = 0xE0;
// An HCI command is a class request to the device on endpoint 0
static constexpr uint8_t HCI_COMMAND_REQUEST_TYPE =
    usb_host::USB_DIR_OUT | usb_host::USB_TYPE_CLASS | usb_host::USB_RECIP_DEVICE;
static constexpr uint8_t HCI_EVENT_COMMAND_COMPLETE = 0x0E;

static constexpr uint16_t OP_RESET = 0x0C03;
static constexpr uint16_t OP_READ_LOCAL_NAME = 0x0C14;
static constexpr uint16_t OP_READ_LOCAL_VERSION = 0x1001;
static constexpr uint16_t OP_READ_BD_ADDR = 0x1009;
// Realtek vendor commands, as used by Linux drivers/bluetooth/btrtl.c
static constexpr uint16_t OP_RTL_DOWNLOAD = 0xFC20;
static constexpr uint16_t OP_RTL_READ_ROM_VERSION = 0xFC6D;
static constexpr size_t RTL_FRAGMENT = 252;
static constexpr uint16_t MANUFACTURER_REALTEK = 93;

#ifdef USE_USB_BLUETOOTH_BLUEDROID
// H4 packet types, the byte Bluedroid's transport puts in front of every packet
static constexpr uint8_t H4_COMMAND = 0x01;
static constexpr uint8_t H4_ACL = 0x02;
static constexpr uint8_t H4_EVENT = 0x04;
// Transfer slots the two readers hold all the time, plus one kept for the next command
static constexpr size_t RESERVED_SLOTS = 3;

// The transport operations are plain C functions without a user argument
static USBBluetooth *g_bluedroid_dongle = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static void hci_drv_send(uint8_t *data, uint16_t len) { g_bluedroid_dongle->hci_send(data, len); }
static bool hci_drv_check_send_available() { return g_bluedroid_dongle->hci_send_available(); }
static esp_err_t hci_drv_register_host_callback(const esp_bluedroid_hci_driver_callbacks_t *callbacks) {
  g_bluedroid_dongle->hci_attach(callbacks);
  return ESP_OK;
}
static constexpr esp_bluedroid_hci_driver_operations_t HCI_OPS = {
    .send = hci_drv_send,
    .check_send_available = hci_drv_check_send_available,
    .register_host_callback = hci_drv_register_host_callback,
};

bool attach_bluedroid() {
  if (!bluedroid_controller_ready())
    return false;
  return esp_bluedroid_attach_hci_driver(&HCI_OPS) == ESP_OK;
}

void detach_bluedroid() {
  if (g_bluedroid_dongle != nullptr)
    g_bluedroid_dongle->hci_attach(nullptr);
  esp_bluedroid_detach_hci_driver();
}

bool bluedroid_controller_ready() { return g_bluedroid_dongle != nullptr && g_bluedroid_dongle->is_ready(); }

void USBBluetooth::set_bluedroid() {
  g_bluedroid_dongle = this;
  this->hci_command_.reserve(3 + 255);
}

// Bluedroid's tasks. Commands go to endpoint 0, ACL data to the bulk OUT endpoint; the H4 type byte is not sent.
void USBBluetooth::hci_send(const uint8_t *h4, uint16_t len) {
  if (len < 2)
    return;
  if (!this->is_ready()) {
    // The dongle is gone. Bluedroid still sends commands while esp32_ble takes it down and waits for
    // their answers, so they are answered here and it can finish
    if (h4[0] == H4_COMMAND && len >= 3)
      this->answer_without_controller_(encode_uint16(h4[2], h4[1]));
    return;
  }
  bool sent = false;
  auto done = [this](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGW(TAG, "Send to the dongle failed (status %u)", status.error_code);
    this->send_done_();
  };
  if (h4[0] == H4_COMMAND) {
    // Bluedroid sends one command at a time from its HCI task: one buffer, reserved once, holds it
    this->hci_command_.assign(h4 + 1, h4 + len);
    sent = this->control_transfer(HCI_COMMAND_REQUEST_TYPE, 0, 0, this->intf_, done, this->hci_command_);
  } else if (h4[0] == H4_ACL) {
    sent = this->transfer_out(this->acl_out_ep_, done, h4 + 1, len - 1);
  }
  if (!sent) {
    ESP_LOGW(TAG, "HCI packet type %u dropped", h4[0]);
    this->send_blocked_.store(true, std::memory_order_release);
  }
}

// Commands the controller answers with a Command Status event; it answers all others with Command Complete
static constexpr uint16_t STATUS_OPCODES[] = {
    0x0406,  // Disconnect
    0x041D,  // Read Remote Version Information
    0x200D,  // LE Create Connection
    0x2013,  // LE Connection Update
    0x2016,  // LE Read Remote Features
    0x2019,  // LE Enable Encryption
    0x2025,  // LE Read Local P-256 Public Key
    0x2026,  // LE Generate DHKey
    0x2032,  // LE Set PHY
    0x2043,  // LE Extended Create Connection
};

// Bluedroid's HCI task. A Command Complete with status success, or a Command Status with Command Disallowed for
// commands that would otherwise start something on the air, so no completion event is waited for.
void USBBluetooth::answer_without_controller_(uint16_t opcode) {
  static constexpr uint8_t STATUS_COMMAND_DISALLOWED = 0x0C;
  bool status_event = false;
  for (uint16_t op : STATUS_OPCODES)
    status_event |= op == opcode;
  // H4 type, event code, parameter length, then the parameters; Command Complete gets zeroed return
  // parameters, so a reader of a few more bytes finds them
  uint8_t event[3 + 4 + 8]{};
  event[0] = H4_EVENT;
  size_t len;
  if (status_event) {
    event[1] = 0x0F;
    event[2] = 4;
    event[3] = STATUS_COMMAND_DISALLOWED;
    event[4] = 1;
    event[5] = static_cast<uint8_t>(opcode & 0xFF);
    event[6] = static_cast<uint8_t>(opcode >> 8);
    len = 7;
  } else {
    event[1] = HCI_EVENT_COMMAND_COMPLETE;
    event[2] = 4 + 8;
    event[3] = 1;
    event[4] = static_cast<uint8_t>(opcode & 0xFF);
    event[5] = static_cast<uint8_t>(opcode >> 8);
    len = sizeof(event);
  }
  this->to_host_(event, len);
}

bool USBBluetooth::hci_send_available() const {
  const size_t used = __builtin_popcount(this->trq_in_use_.load(std::memory_order_acquire));
  if (used + RESERVED_SLOTS <= usb_host::MAX_REQUESTS)
    return true;
  this->send_blocked_.store(true, std::memory_order_release);
  return false;
}

void USBBluetooth::hci_attach(const esp_bluedroid_hci_driver_callbacks_t *callbacks) {
  this->host_ = callbacks;
  this->host_attached_.store(callbacks != nullptr, std::memory_order_release);
  if (callbacks != nullptr)
    ESP_LOGI(TAG, "Bluedroid attached to the dongle");
}

// USB task
void USBBluetooth::send_done_() {
  if (this->send_blocked_.exchange(false, std::memory_order_acq_rel) && this->host_ != nullptr)
    this->host_->notify_host_send_available();
}

// USB task. Bluedroid copies the packet before returning.
void USBBluetooth::to_host_(uint8_t *h4, size_t len) {
  const auto *host = this->host_;
  if (host != nullptr && this->host_attached_.load(std::memory_order_acquire))
    host->notify_host_recv(h4, static_cast<uint16_t>(len));
}

// USB task. ACL packets are put back together by their own length field, like events.
void USBBluetooth::read_acl_(uint32_t generation) {
  const bool submitted = this->transfer_in(
      this->acl_in_ep_,
      [this, generation](const usb_host::TransferStatus &status) {
        if (generation != this->generation_.load(std::memory_order_acquire))
          return;
        if (!status.success) {
          ESP_LOGW(TAG, "ACL endpoint stopped (status %u)", status.error_code);
          return;
        }
        uint8_t *acl = this->acl_h4_ + 1;
        const size_t take = std::min(status.data_len, ACL_MAX - this->acl_len_);
        memcpy(acl + this->acl_len_, status.data, take);
        this->acl_len_ += take;
        if (this->acl_len_ >= 4) {
          const size_t len = 4u + encode_uint16(acl[3], acl[2]);
          if (this->acl_len_ >= len || this->acl_len_ == ACL_MAX) {
            this->acl_h4_[0] = H4_ACL;
            this->to_host_(this->acl_h4_, 1 + std::min(len, this->acl_len_));
            this->acl_len_ = 0;
          }
        }
        this->read_acl_(generation);
      },
      this->acl_in_mps_);
  if (!submitted)
    ESP_LOGE(TAG, "ACL read not submitted");
}
#endif

// Bluetooth SIG assigned numbers for the HCI version
static const char *core_version(uint8_t version) {
  static constexpr const char *NAMES[] = {"1.0b", "1.1", "1.2", "2.0", "2.1", "3.0", "4.0", "4.1",
                                          "4.2",  "5.0", "5.1", "5.2", "5.3", "5.4", "6.0", "6.1"};
  return version < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[version] : "?";
}

bool USBBluetooth::find_interface_() {
  const usb_config_desc_t *cfg;
  if (usb_host_get_active_config_descriptor(this->device_handle_, &cfg) != ESP_OK)
    return false;
  this->intf_ = 0xFF;
  this->evt_ep_ = this->acl_in_ep_ = this->acl_out_ep_ = 0;
  uint8_t found = 0xFF;
  bool in_hci = false;
  int offset = 0;
  const auto *desc = reinterpret_cast<const usb_standard_desc_t *>(cfg);
  while ((desc = usb_parse_next_descriptor(desc, cfg->wTotalLength, &offset)) != nullptr) {
    if (desc->bDescriptorType == USB_W_VALUE_DT_INTERFACE) {
      const auto *intf = reinterpret_cast<const usb_intf_desc_t *>(desc);
      in_hci = found == 0xFF && intf->bAlternateSetting == 0 && intf->bInterfaceClass == HCI_CLASS &&
               intf->bInterfaceSubClass == 0x01 && intf->bInterfaceProtocol == 0x01;
      if (in_hci)
        found = intf->bInterfaceNumber;
      continue;
    }
    if (desc->bDescriptorType != USB_W_VALUE_DT_ENDPOINT || !in_hci)
      continue;
    const auto *ep = reinterpret_cast<const usb_ep_desc_t *>(desc);
    const uint8_t type = USB_EP_DESC_GET_XFERTYPE(ep);
    const bool in = USB_EP_DESC_GET_EP_DIR(ep);
    if (type == USB_BM_ATTRIBUTES_XFER_INT && in) {
      this->evt_ep_ = ep->bEndpointAddress;
      this->evt_mps_ = USB_EP_DESC_GET_MPS(ep);
    } else if (type == USB_BM_ATTRIBUTES_XFER_BULK && in) {
      this->acl_in_ep_ = ep->bEndpointAddress;
      this->acl_in_mps_ = USB_EP_DESC_GET_MPS(ep);
    } else if (type == USB_BM_ATTRIBUTES_XFER_BULK) {
      this->acl_out_ep_ = ep->bEndpointAddress;
      this->acl_out_mps_ = USB_EP_DESC_GET_MPS(ep);
    }
  }
  if (found == 0xFF)
    return false;
  if (this->evt_ep_ == 0 || this->acl_in_ep_ == 0 || this->acl_out_ep_ == 0 || this->evt_mps_ == 0 ||
      this->evt_mps_ > usb_host::USB_MAX_PACKET_SIZE) {
    ESP_LOGE(TAG, "Bluetooth interface %u lacks usable event or ACL endpoints", found);
    return false;
  }
  this->intf_ = found;
  return true;
}

void USBBluetooth::on_connected() {
  const uint32_t generation = this->generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
  if (!this->find_interface_()) {
    // Not a Bluetooth dongle: leave it to the other clients
    ESP_LOGD(TAG, "No Bluetooth controller interface, closing");
    this->disconnect();
    return;
  }
  const esp_err_t err = usb_host_interface_claim(this->handle_, this->device_handle_, this->intf_, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Claiming interface %u failed: %s", this->intf_, esp_err_to_name(err));
    this->intf_ = 0xFF;
    this->disconnect();
    return;
  }
  ESP_LOGI(TAG, "Bluetooth controller on interface %u", this->intf_);
  this->event_len_ = 0;
  this->complete_ready_.store(false, std::memory_order_release);
  this->read_event_(generation);
#ifdef USE_USB_BLUETOOTH_BLUEDROID
  if (g_bluedroid_dongle == this) {
    this->acl_len_ = 0;
    this->read_acl_(generation);
  }
#endif
  this->step_ = ProbeStep::PROBE_STEP_RESET;
  this->send_command_(OP_RESET, nullptr, 0);
}

void USBBluetooth::on_disconnected() {
  this->generation_.fetch_add(1, std::memory_order_acq_rel);
  this->cancel_timeout(COMMAND_TIMEOUT_ID);
  this->pending_opcode_.store(0, std::memory_order_release);
  this->complete_ready_.store(false, std::memory_order_release);
  this->step_ = ProbeStep::PROBE_STEP_IDLE;
  this->download_ = nullptr;
  memset(this->address_, 0, sizeof(this->address_));
  if (this->intf_ != 0xFF) {
    usb_host_interface_release(this->handle_, this->device_handle_, this->intf_);
    this->intf_ = 0xFF;
    ESP_LOGI(TAG, "Bluetooth controller removed");
  }
#ifdef USE_USB_BLUETOOTH_BLUEDROID
  // Bluedroid reads its controller once, when it starts: take it down now and bring it back on the next dongle
  // Bluedroid stays attached until esp32_ble has taken it down; answer_without_controller_() answers it meanwhile
  if (this->host_attached_.load(std::memory_order_acquire) && !this->restart_host_) {
    this->restart_host_ = true;
    esp32_ble::global_ble->disable();
  }
#endif
  usb_host::USBClient::on_disconnected();
}

// USB task. One read is one wMaxPacketSize packet; events are put back together by their own length
// field, because an event that is a whole number of packets long ends without a short packet.
void USBBluetooth::read_event_(uint32_t generation) {
  const bool submitted = this->transfer_in(
      this->evt_ep_,
      [this, generation](const usb_host::TransferStatus &status) {
        if (generation != this->generation_.load(std::memory_order_acquire))
          return;
        if (!status.success) {
          ESP_LOGW(TAG, "Event endpoint stopped (status %u)", status.error_code);
          return;
        }
        const size_t take = std::min(status.data_len, EVENT_MAX - this->event_len_);
        memcpy(this->event_ + this->event_len_, status.data, take);
        this->event_len_ += take;
        if (this->event_len_ >= 2 && this->event_len_ >= 2u + this->event_[1]) {
          const size_t len = 2u + this->event_[1];
          this->event_len_ = 0;
#ifdef USE_USB_BLUETOOTH_BLUEDROID
          if (this->host_attached_.load(std::memory_order_acquire)) {
            this->event_h4_[0] = H4_EVENT;
            this->to_host_(this->event_h4_, len + 1);
            this->read_event_(generation);
            return;
          }
#endif
          // Command Complete: code, length, number of packets, opcode, status, return parameters
          const uint16_t pending = this->pending_opcode_.load(std::memory_order_acquire);
          if (this->event_[0] == HCI_EVENT_COMMAND_COMPLETE && len >= 6 && pending != 0 &&
              encode_uint16(this->event_[4], this->event_[3]) == pending &&
              !this->complete_ready_.load(std::memory_order_acquire)) {
            memcpy(this->complete_, this->event_, len);
            this->complete_len_ = len;
            this->complete_ready_.store(true, std::memory_order_release);
            this->enable_loop_soon_any_context();
          }
        }
        this->read_event_(generation);
      },
      this->evt_mps_);
  if (!submitted)
    ESP_LOGE(TAG, "Event read not submitted");
}

void USBBluetooth::send_command_(uint16_t opcode, const uint8_t *params, size_t len) {
  std::vector<uint8_t> command(3 + len);
  command[0] = static_cast<uint8_t>(opcode & 0xFF);
  command[1] = static_cast<uint8_t>(opcode >> 8);
  command[2] = static_cast<uint8_t>(len);
  if (len != 0)
    memcpy(command.data() + 3, params, len);
  this->pending_opcode_.store(opcode, std::memory_order_release);
  const bool sent = this->control_transfer(
      HCI_COMMAND_REQUEST_TYPE, 0, 0, this->intf_,
      [opcode](const usb_host::TransferStatus &status) {
        if (!status.success)
          ESP_LOGW(TAG, "HCI command 0x%04X not taken (status %u)", opcode, status.error_code);
      },
      command);
  if (!sent) {
    ESP_LOGE(TAG, "HCI command 0x%04X not submitted", opcode);
    this->finish_probe_(false);
    return;
  }
  this->set_timeout(COMMAND_TIMEOUT_ID, COMMAND_TIMEOUT_MS, [this]() {
    ESP_LOGE(TAG, "No answer to HCI command 0x%04X", this->pending_opcode_.load(std::memory_order_acquire));
    this->finish_probe_(false);
  });
}

void USBBluetooth::loop() {
  bool work = this->process_usb_events_();
  if (this->complete_ready_.load(std::memory_order_acquire)) {
    work = true;
    this->cancel_timeout(COMMAND_TIMEOUT_ID);
    this->pending_opcode_.store(0, std::memory_order_release);
    const uint8_t status = this->complete_[5];
    const uint16_t opcode = encode_uint16(this->complete_[4], this->complete_[3]);
    // Copy out before the flag lets the USB task write the buffer again
    uint8_t ret[EVENT_MAX];
    const size_t len = this->complete_len_ - 6;
    memcpy(ret, this->complete_ + 6, len);
    this->complete_ready_.store(false, std::memory_order_release);
    if (status != 0) {
      ESP_LOGE(TAG, "HCI command 0x%04X failed, status 0x%02X", opcode, status);
      this->finish_probe_(false);
    } else {
      this->handle_command_complete_(ret, len);
    }
  }
  if (!work)
    this->disable_loop();
}

void USBBluetooth::handle_command_complete_(const uint8_t *ret, size_t len) {
  switch (this->step_) {
    case ProbeStep::PROBE_STEP_RESET:
      this->step_ = ProbeStep::PROBE_STEP_READ_VERSION;
      this->send_command_(OP_READ_LOCAL_VERSION, nullptr, 0);
      return;

    case ProbeStep::PROBE_STEP_READ_VERSION:
    case ProbeStep::PROBE_STEP_READ_VERSION_PATCHED: {
      // HCI version, HCI revision, LMP version, manufacturer, LMP subversion
      if (len < 8) {
        this->finish_probe_(false);
        return;
      }
      this->hci_version_ = ret[0];
      this->hci_revision_ = encode_uint16(ret[2], ret[1]);
      this->manufacturer_ = encode_uint16(ret[5], ret[4]);
      this->lmp_subver_ = encode_uint16(ret[7], ret[6]);
      if (this->step_ == ProbeStep::PROBE_STEP_READ_VERSION_PATCHED) {
        ESP_LOGI(TAG, "Firmware loaded: revision 0x%04X, LMP subversion 0x%04X", this->hci_revision_,
                 this->lmp_subver_);
      } else if (this->manufacturer_ == MANUFACTURER_REALTEK) {
        // A controller on its ROM reports the lmp_subver of its chip, a patched one that of its firmware
        bool for_chip = false;
        for (const auto &image : this->realtek_images_)
          for_chip |= image.lmp_subver == this->lmp_subver_;
        if (for_chip) {
          this->step_ = ProbeStep::PROBE_STEP_RTL_READ_ROM_VERSION;
          this->send_command_(OP_RTL_READ_ROM_VERSION, nullptr, 0);
          return;
        }
        if (this->realtek_images_.empty()) {
          ESP_LOGW(TAG, "Realtek controller without firmware configured: on its ROM it does not work on the air");
        }
      }
      this->step_ = ProbeStep::PROBE_STEP_READ_ADDRESS;
      this->send_command_(OP_READ_BD_ADDR, nullptr, 0);
      return;
    }

    case ProbeStep::PROBE_STEP_RTL_READ_ROM_VERSION: {
      if (len < 1) {
        this->finish_probe_(false);
        return;
      }
      this->download_ = nullptr;
      for (const auto &image : this->realtek_images_) {
        if (image.lmp_subver == this->lmp_subver_ && image.rom_version == ret[0])
          this->download_ = &image;
      }
      if (this->download_ == nullptr) {
        ESP_LOGE(TAG, "The firmware has no patch for ROM version %u", ret[0]);
        this->step_ = ProbeStep::PROBE_STEP_READ_ADDRESS;
        this->send_command_(OP_READ_BD_ADDR, nullptr, 0);
        return;
      }
      ESP_LOGD(TAG, "Realtek ROM version %u: downloading %u bytes", ret[0],
               static_cast<unsigned>(this->download_->length));
      this->step_ = ProbeStep::PROBE_STEP_RTL_DOWNLOAD;
      this->fragment_ = 0;
      this->fragment_count_ = this->download_->length / RTL_FRAGMENT + 1;
      this->next_fragment_();
      return;
    }

    case ProbeStep::PROBE_STEP_RTL_DOWNLOAD:
      if (++this->fragment_ < this->fragment_count_) {
        this->next_fragment_();
        return;
      }
      this->download_ = nullptr;
      this->step_ = ProbeStep::PROBE_STEP_READ_VERSION_PATCHED;
      this->send_command_(OP_READ_LOCAL_VERSION, nullptr, 0);
      return;

    case ProbeStep::PROBE_STEP_READ_ADDRESS:
      if (len >= 6) {
        for (size_t i = 0; i < 6; i++)
          this->address_[i] = ret[5 - i];
      }
      this->step_ = ProbeStep::PROBE_STEP_READ_NAME;
      this->send_command_(OP_READ_LOCAL_NAME, nullptr, 0);
      return;

    case ProbeStep::PROBE_STEP_READ_NAME: {
      // The name is NUL terminated, but some controllers (Realtek) pad it with 0xFF instead
      size_t name_len = 0;
      while (name_len < len && ret[name_len] != 0x00 && ret[name_len] != 0xFF)
        name_len++;
      ESP_LOGI(TAG,
               "Controller ready: Bluetooth %s, manufacturer %u, address %02X:%02X:%02X:%02X:%02X:%02X, name '%.*s'",
               core_version(this->hci_version_), this->manufacturer_, this->address_[0], this->address_[1],
               this->address_[2], this->address_[3], this->address_[4], this->address_[5], static_cast<int>(name_len),
               reinterpret_cast<const char *>(ret));
      this->finish_probe_(true);
      return;
    }

    case ProbeStep::PROBE_STEP_IDLE:
    case ProbeStep::PROBE_STEP_READY:
    case ProbeStep::PROBE_STEP_FAILED:
      return;
  }
}

// Fragments of RTL_FRAGMENT bytes, the last one flagged 0x80 and carrying the remainder; the
// index runs 0..0x7F and then wraps to 1, as rtl_download_firmware() in btrtl.c does.
void USBBluetooth::next_fragment_() {
  const uint32_t i = this->fragment_;
  uint8_t index = i <= 0x7F ? static_cast<uint8_t>(i) : static_cast<uint8_t>((i - 0x80) % 0x7F + 1);
  size_t take = RTL_FRAGMENT;
  if (i + 1 == this->fragment_count_) {
    index |= 0x80;
    take = this->download_->length % RTL_FRAGMENT;
  }
  uint8_t params[1 + RTL_FRAGMENT];
  params[0] = index;
  memcpy(params + 1, this->download_->data + i * RTL_FRAGMENT, take);
  this->send_command_(OP_RTL_DOWNLOAD, params, 1 + take);
}

void USBBluetooth::finish_probe_(bool ok) {
  this->cancel_timeout(COMMAND_TIMEOUT_ID);
  this->pending_opcode_.store(0, std::memory_order_release);
  this->download_ = nullptr;
  this->step_ = ok ? ProbeStep::PROBE_STEP_READY : ProbeStep::PROBE_STEP_FAILED;
  if (ok) {
    this->status_clear_warning();
#ifdef USE_USB_BLUETOOTH_BLUEDROID
    if (this->restart_host_) {
      this->restart_host_ = false;
      esp32_ble::global_ble->enable();
    }
#endif
  } else {
    this->status_set_warning(LOG_STR("Bluetooth controller did not start"));
  }
  this->report_connected_();
}

void USBBluetooth::dump_config() {
  usb_host::USBClient::dump_config();
  ESP_LOGCONFIG(TAG, "  Realtek firmware images: %u", static_cast<unsigned>(this->realtek_images_.size()));
}

}  // namespace esphome::usb_bluetooth

#endif
