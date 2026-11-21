#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "usb_bluetooth.h"
#include "esphome/core/log.h"
#include "usb/usb_helpers.h"

#include <algorithm>
#include <cstring>

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

void USBBluetooth::setup() {
  usb_host::USBClient::setup();
  // The longest command is a Realtek download fragment: opcode, length, index and RTL_FRAGMENT bytes
  this->command_.reserve(3 + 1 + RTL_FRAGMENT);
}

void USBBluetooth::on_connected() {
  const uint32_t generation = this->generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
  if (!this->find_interface_()) {
    // Not a Bluetooth dongle: leave it to the other clients, and look at the next device. The host
    // library announces a device once, so one plugged in together with this one (behind a hub) was
    // not offered to this client while it was busy with this one.
    ESP_LOGD(TAG, "No Bluetooth controller interface, closing");
    const int addr = this->device_addr_;
    if (addr >= 0 && addr < 128)
      this->declined_[addr / 32] |= 1u << (addr % 32);
    this->disconnect();
    this->try_next_device_();
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
  if (!this->read_event_(generation)) {
    this->finish_probe_(false);
    return;
  }
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
    // Stop and cancel the transfers still queued on the interface's endpoints; it cannot be released before
    for (const uint8_t ep : {this->evt_ep_, this->acl_in_ep_, this->acl_out_ep_}) {
      if (ep == 0)
        continue;
      usb_host_endpoint_halt(this->device_handle_, ep);
      usb_host_endpoint_flush(this->device_handle_, ep);
    }
    const esp_err_t err = usb_host_interface_release(this->handle_, this->device_handle_, this->intf_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Releasing interface %u failed: %s", this->intf_, esp_err_to_name(err));
    }
    this->intf_ = 0xFF;
    ESP_LOGI(TAG, "Bluetooth controller removed");
  }
  usb_host::USBClient::on_disconnected();
}

// USB task. One read is one wMaxPacketSize packet; events are put back together by their own length
// field, because an event that is a whole number of packets long ends without a short packet.
bool USBBluetooth::read_event_(uint32_t generation) {
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
  if (!submitted) {
    ESP_LOGE(TAG, "Event read not submitted");
  }
  return submitted;
}

void USBBluetooth::try_next_device_() {
  uint8_t addrs[32];
  int count = 0;
  if (usb_host_device_addr_list_fill(sizeof(addrs), addrs, &count) != ESP_OK)
    return;
  // Addresses are reused: forget declined devices that are gone
  uint32_t present[4]{};
  for (int i = 0; i < count; i++) {
    if (addrs[i] < 128)
      present[addrs[i] / 32] |= 1u << (addrs[i] % 32);
  }
  for (size_t i = 0; i < 4; i++)
    this->declined_[i] &= present[i];
  for (int i = 0; i < count; i++) {
    const uint8_t addr = addrs[i];
    if (addr < 128 && (this->declined_[addr / 32] & (1u << (addr % 32))) == 0) {
      this->on_opened(addr);
      return;
    }
  }
}

void USBBluetooth::send_command_(uint16_t opcode, const uint8_t *params, size_t len) {
  // Reserved in setup(): assign() reuses the buffer
  this->command_.assign(3, 0);
  this->command_[0] = static_cast<uint8_t>(opcode & 0xFF);
  this->command_[1] = static_cast<uint8_t>(opcode >> 8);
  this->command_[2] = static_cast<uint8_t>(len);
  this->command_.insert(this->command_.end(), params, params + len);
  this->pending_opcode_.store(opcode, std::memory_order_release);
  // A device-recipient class request: wIndex is 0 (Bluetooth Core, USB transport)
  const bool sent = this->control_transfer(
      HCI_COMMAND_REQUEST_TYPE, 0, 0, 0,
      [opcode](const usb_host::TransferStatus &status) {
        if (!status.success) {
          ESP_LOGW(TAG, "HCI command 0x%04X not taken (status %u)", opcode, status.error_code);
        }
      },
      this->command_);
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
        // On its ROM the controller does not work on the air
        ESP_LOGE(TAG, "The firmware has no patch for ROM version %u", ret[0]);
        this->finish_probe_(false);
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
      // The return parameters carry the index of the fragment taken
      if (len < 1) {
        ESP_LOGE(TAG, "Realtek download fragment %u not acknowledged", static_cast<unsigned>(this->fragment_));
        this->finish_probe_(false);
        return;
      }
      if (++this->fragment_ < this->fragment_count_) {
        this->next_fragment_();
        return;
      }
      this->download_ = nullptr;
      this->step_ = ProbeStep::PROBE_STEP_READ_VERSION_PATCHED;
      this->send_command_(OP_READ_LOCAL_VERSION, nullptr, 0);
      return;

    case ProbeStep::PROBE_STEP_READ_ADDRESS:
      if (len < 6) {
        this->finish_probe_(false);
        return;
      }
      for (size_t i = 0; i < 6; i++)
        this->address_[i] = ret[5 - i];
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
