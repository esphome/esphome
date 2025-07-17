// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_host.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#include <cinttypes>
#include <cstring>
namespace esphome {
namespace usb_host {

#pragma GCC diagnostic ignored "-Wparentheses"

using namespace bytebuffer;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
static void print_ep_desc(const usb_ep_desc_t *ep_desc) {
  const char *ep_type_str;
  int type = ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;

  switch (type) {
    case USB_BM_ATTRIBUTES_XFER_CONTROL:
      ep_type_str = "CTRL";
      break;
    case USB_BM_ATTRIBUTES_XFER_ISOC:
      ep_type_str = "ISOC";
      break;
    case USB_BM_ATTRIBUTES_XFER_BULK:
      ep_type_str = "BULK";
      break;
    case USB_BM_ATTRIBUTES_XFER_INT:
      ep_type_str = "INT";
      break;
    default:
      ep_type_str = NULL;
      break;
  }

  ESP_LOGV(TAG, "\t\t*** Endpoint descriptor ***");
  ESP_LOGV(TAG, "\t\tbLength %d", ep_desc->bLength);
  ESP_LOGV(TAG, "\t\tbDescriptorType %d", ep_desc->bDescriptorType);
  ESP_LOGV(TAG, "\t\tbEndpointAddress 0x%x\tEP %d %s", ep_desc->bEndpointAddress, USB_EP_DESC_GET_EP_NUM(ep_desc),
           USB_EP_DESC_GET_EP_DIR(ep_desc) ? "IN" : "OUT");
  ESP_LOGV(TAG, "\t\tbmAttributes 0x%x\t%s", ep_desc->bmAttributes, ep_type_str);
  ESP_LOGV(TAG, "\t\twMaxPacketSize %d", ep_desc->wMaxPacketSize);
  ESP_LOGV(TAG, "\t\tbInterval %d", ep_desc->bInterval);
}

static void usbh_print_intf_desc(const usb_intf_desc_t *intf_desc) {
  ESP_LOGV(TAG, "\t*** Interface descriptor ***");
  ESP_LOGV(TAG, "\tbLength %d", intf_desc->bLength);
  ESP_LOGV(TAG, "\tbDescriptorType %d", intf_desc->bDescriptorType);
  ESP_LOGV(TAG, "\tbInterfaceNumber %d", intf_desc->bInterfaceNumber);
  ESP_LOGV(TAG, "\tbAlternateSetting %d", intf_desc->bAlternateSetting);
  ESP_LOGV(TAG, "\tbNumEndpoints %d", intf_desc->bNumEndpoints);
  ESP_LOGV(TAG, "\tbInterfaceClass 0x%x", intf_desc->bInterfaceProtocol);
  ESP_LOGV(TAG, "\tiInterface %d", intf_desc->iInterface);
}

static void usbh_print_cfg_desc(const usb_config_desc_t *cfg_desc) {
  ESP_LOGV(TAG, "*** Configuration descriptor ***");
  ESP_LOGV(TAG, "bLength %d", cfg_desc->bLength);
  ESP_LOGV(TAG, "bDescriptorType %d", cfg_desc->bDescriptorType);
  ESP_LOGV(TAG, "wTotalLength %d", cfg_desc->wTotalLength);
  ESP_LOGV(TAG, "bNumInterfaces %d", cfg_desc->bNumInterfaces);
  ESP_LOGV(TAG, "bConfigurationValue %d", cfg_desc->bConfigurationValue);
  ESP_LOGV(TAG, "iConfiguration %d", cfg_desc->iConfiguration);
  ESP_LOGV(TAG, "bmAttributes 0x%x", cfg_desc->bmAttributes);
  ESP_LOGV(TAG, "bMaxPower %dmA", cfg_desc->bMaxPower * 2);
}

static void usb_client_print_device_descriptor(const usb_device_desc_t *devc_desc) {
  if (devc_desc == NULL) {
    return;
  }

  ESP_LOGV(TAG, "*** Device descriptor ***");
  ESP_LOGV(TAG, "bLength %d", devc_desc->bLength);
  ESP_LOGV(TAG, "bDescriptorType %d", devc_desc->bDescriptorType);
  ESP_LOGV(TAG, "bcdUSB %d.%d0", ((devc_desc->bcdUSB >> 8) & 0xF), ((devc_desc->bcdUSB >> 4) & 0xF));
  ESP_LOGV(TAG, "bDeviceClass 0x%x", devc_desc->bDeviceClass);
  ESP_LOGV(TAG, "bDeviceSubClass 0x%x", devc_desc->bDeviceSubClass);
  ESP_LOGV(TAG, "bDeviceProtocol 0x%x", devc_desc->bDeviceProtocol);
  ESP_LOGV(TAG, "bMaxPacketSize0 %d", devc_desc->bMaxPacketSize0);
  ESP_LOGV(TAG, "idVendor 0x%x", devc_desc->idVendor);
  ESP_LOGV(TAG, "idProduct 0x%x", devc_desc->idProduct);
  ESP_LOGV(TAG, "bcdDevice %d.%d0", ((devc_desc->bcdDevice >> 8) & 0xF), ((devc_desc->bcdDevice >> 4) & 0xF));
  ESP_LOGV(TAG, "iManufacturer %d", devc_desc->iManufacturer);
  ESP_LOGV(TAG, "iProduct %d", devc_desc->iProduct);
  ESP_LOGV(TAG, "iSerialNumber %d", devc_desc->iSerialNumber);
  ESP_LOGV(TAG, "bNumConfigurations %d", devc_desc->bNumConfigurations);
}

static void usb_client_print_config_descriptor(const usb_config_desc_t *cfg_desc,
                                               print_class_descriptor_cb class_specific_cb) {
  if (cfg_desc == nullptr) {
    return;
  }

  int offset = 0;
  uint16_t w_total_length = cfg_desc->wTotalLength;
  const usb_standard_desc_t *next_desc = (const usb_standard_desc_t *) cfg_desc;

  do {
    switch (next_desc->bDescriptorType) {
      case USB_W_VALUE_DT_CONFIG:
        usbh_print_cfg_desc((const usb_config_desc_t *) next_desc);
        break;
      case USB_W_VALUE_DT_INTERFACE:
        usbh_print_intf_desc((const usb_intf_desc_t *) next_desc);
        break;
      case USB_W_VALUE_DT_ENDPOINT:
        print_ep_desc((const usb_ep_desc_t *) next_desc);
        break;
      default:
        if (class_specific_cb) {
          class_specific_cb(next_desc);
        }
        break;
    }

    next_desc = usb_parse_next_descriptor(next_desc, w_total_length, &offset);

  } while (next_desc != NULL);
}
#endif
static std::string get_descriptor_string(const usb_str_desc_t *desc) {
  char buffer[256];
  if (desc == nullptr)
    return "(unspecified)";
  char *p = buffer;
  for (int i = 0; i != desc->bLength / 2; i++) {
    auto c = desc->wData[i];
    if (c < 0x100)
      *p++ = static_cast<char>(c);
  }
  *p = '\0';
  return {buffer};
}

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *ptr) {
  auto *client = static_cast<USBClient *>(ptr);
  switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV: {
      auto addr = event_msg->new_dev.address;
      ESP_LOGD(TAG, "New device %d", event_msg->new_dev.address);
      client->on_opened(addr);
      break;
    }
    case USB_HOST_CLIENT_EVENT_DEV_GONE: {
      client->on_removed(event_msg->dev_gone.dev_hdl);
      ESP_LOGD(TAG, "Device gone %d", event_msg->new_dev.address);
      break;
    }
    default:
      ESP_LOGD(TAG, "Unknown event %d", event_msg->event);
      break;
  }
}
void USBClient::setup() {
  usb_host_client_config_t config{.is_synchronous = false,
                                  .max_num_event_msg = 5,
                                  .async = {.client_event_callback = client_event_cb, .callback_arg = this}};
  auto err = usb_host_client_register(&config, &this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "client register failed: %s", esp_err_to_name(err));
    this->status_set_error("Client register failed");
    this->mark_failed();
    return;
  }
  for (auto *trq : this->trq_pool_) {
    usb_host_transfer_alloc(64, 0, &trq->transfer);
    trq->client = this;
  }
  ESP_LOGCONFIG(TAG, "client setup complete");
}

void USBClient::loop() {
  switch (this->state_) {
    case USB_CLIENT_OPEN: {
      int err;
      ESP_LOGD(TAG, "Open device %d", this->device_addr_);
      err = usb_host_device_open(this->handle_, this->device_addr_, &this->device_handle_);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Device open failed: %s", esp_err_to_name(err));
        this->state_ = USB_CLIENT_INIT;
        break;
      }
      ESP_LOGD(TAG, "Get descriptor device %d", this->device_addr_);
      const usb_device_desc_t *desc;
      err = usb_host_get_device_descriptor(this->device_handle_, &desc);
      if (err != ESP_OK) {
        ESP_LOGW(TAG, "Device get_desc failed: %s", esp_err_to_name(err));
        this->disconnect();
      } else {
        ESP_LOGD(TAG, "Device descriptor: vid %X pid %X", desc->idVendor, desc->idProduct);
        if (desc->idVendor == this->vid_ && desc->idProduct == this->pid_ || this->vid_ == 0 && this->pid_ == 0) {
          usb_device_info_t dev_info;
          err = usb_host_device_info(this->device_handle_, &dev_info);
          if (err != ESP_OK) {
            ESP_LOGW(TAG, "Device info failed: %s", esp_err_to_name(err));
            this->disconnect();
            break;
          }
          this->state_ = USB_CLIENT_CONNECTED;
          ESP_LOGD(TAG, "Device connected: Manuf: %s; Prod: %s; Serial: %s",
                   get_descriptor_string(dev_info.str_desc_manufacturer).c_str(),
                   get_descriptor_string(dev_info.str_desc_product).c_str(),
                   get_descriptor_string(dev_info.str_desc_serial_num).c_str());

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          const usb_device_desc_t *device_desc;
          err = usb_host_get_device_descriptor(this->device_handle_, &device_desc);
          if (err == ESP_OK)
            usb_client_print_device_descriptor(device_desc);
          const usb_config_desc_t *config_desc;
          err = usb_host_get_active_config_descriptor(this->device_handle_, &config_desc);
          if (err == ESP_OK)
            usb_client_print_config_descriptor(config_desc, nullptr);
#endif
          this->on_connected();
        } else {
          ESP_LOGD(TAG, "Not our device, closing");
          this->disconnect();
        }
      }
      break;
    }

    default:
      usb_host_client_handle_events(this->handle_, 0);
      break;
  }
}

void USBClient::on_opened(uint8_t addr) {
  if (this->state_ == USB_CLIENT_INIT) {
    this->device_addr_ = addr;
    this->state_ = USB_CLIENT_OPEN;
  }
}
void USBClient::on_removed(usb_device_handle_t handle) {
  if (this->device_handle_ == handle) {
    this->disconnect();
  }
}

static void control_callback(const usb_transfer_t *xfer) {
  auto *trq = static_cast<TransferRequest *>(xfer->context);
  trq->status.error_code = xfer->status;
  trq->status.success = xfer->status == USB_TRANSFER_STATUS_COMPLETED;
  trq->status.endpoint = xfer->bEndpointAddress;
  trq->status.data = xfer->data_buffer;
  trq->status.data_len = xfer->actual_num_bytes;
  if (trq->callback != nullptr)
    trq->callback(trq->status);
  trq->client->release_trq(trq);
}

TransferRequest *USBClient::get_trq_() {
  if (this->trq_pool_.empty()) {
    ESP_LOGE(TAG, "Too many requests queued");
    return nullptr;
  }
  auto *trq = this->trq_pool_.front();
  this->trq_pool_.pop_front();
  trq->client = this;
  trq->transfer->context = trq;
  trq->transfer->device_handle = this->device_handle_;
  return trq;
}
void USBClient::disconnect() {
  this->on_disconnected();
  auto err = usb_host_device_close(this->handle_, this->device_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Device close failed: %s", esp_err_to_name(err));
  }
  this->state_ = USB_CLIENT_INIT;
  this->device_handle_ = nullptr;
  this->device_addr_ = -1;
}

bool USBClient::control_transfer(uint8_t type, uint8_t request, uint16_t value, uint16_t index,
                                 const transfer_cb_t &callback, const std::vector<uint8_t> &data) {
  auto *trq = this->get_trq_();
  if (trq == nullptr)
    return false;
  auto length = data.size();
  if (length > sizeof(trq->transfer->data_buffer_size) - SETUP_PACKET_SIZE) {
    ESP_LOGE(TAG, "Control transfer data size too large: %u > %u", length,
             sizeof(trq->transfer->data_buffer_size) - sizeof(usb_setup_packet_t));
    this->release_trq(trq);
    return false;
  }
  auto control_packet = ByteBuffer(SETUP_PACKET_SIZE, LITTLE);
  control_packet.put_uint8(type);
  control_packet.put_uint8(request);
  control_packet.put_uint16(value);
  control_packet.put_uint16(index);
  control_packet.put_uint16(length);
  memcpy(trq->transfer->data_buffer, control_packet.get_data().data(), SETUP_PACKET_SIZE);
  if (length != 0 && !(type & USB_DIR_IN)) {
    memcpy(trq->transfer->data_buffer + SETUP_PACKET_SIZE, data.data(), length);
  }
  trq->callback = callback;
  trq->transfer->bEndpointAddress = type & USB_DIR_MASK;
  trq->transfer->num_bytes = static_cast<int>(length + SETUP_PACKET_SIZE);
  trq->transfer->callback = reinterpret_cast<usb_transfer_cb_t>(control_callback);
  auto err = usb_host_transfer_submit_control(this->handle_, trq->transfer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to submit control transfer, err=%s", esp_err_to_name(err));
    this->release_trq(trq);
    return false;
  }
  return true;
}

static void transfer_callback(usb_transfer_t *xfer) {
  auto *trq = static_cast<TransferRequest *>(xfer->context);
  trq->status.error_code = xfer->status;
  trq->status.success = xfer->status == USB_TRANSFER_STATUS_COMPLETED;
  trq->status.endpoint = xfer->bEndpointAddress;
  trq->status.data = xfer->data_buffer;
  trq->status.data_len = xfer->actual_num_bytes;
  if (trq->callback != nullptr)
    trq->callback(trq->status);
  trq->client->release_trq(trq);
}
/**
 * Performs a transfer input operation.
 *
 * @param ep_address The endpoint address.
 * @param callback The callback function to be called when the transfer is complete.
 * @param length The length of the data to be transferred.
 *
 * @throws None.
 */
void USBClient::transfer_in(uint8_t ep_address, const transfer_cb_t &callback, uint16_t length) {
  auto *trq = this->get_trq_();
  if (trq == nullptr) {
    ESP_LOGE(TAG, "Too many requests queued");
    return;
  }
  trq->callback = callback;
  trq->transfer->callback = transfer_callback;
  trq->transfer->bEndpointAddress = ep_address | USB_DIR_IN;
  trq->transfer->num_bytes = length;
  auto err = usb_host_transfer_submit(trq->transfer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to submit transfer, address=%x, length=%d, err=%x", ep_address, length, err);
    this->release_trq(trq);
  }
}

/**
 * Performs an output transfer operation.
 *
 * @param ep_address The endpoint address.
 * @param callback The callback function to be called when the transfer is complete.
 * @param data The data to be transferred.
 * @param length The length of the data to be transferred.
 *
 * @throws None.
 */
void USBClient::transfer_out(uint8_t ep_address, const transfer_cb_t &callback, const uint8_t *data, uint16_t length) {
  auto *trq = this->get_trq_();
  if (trq == nullptr) {
    ESP_LOGE(TAG, "Too many requests queued");
    return;
  }
  trq->callback = callback;
  trq->transfer->callback = transfer_callback;
  trq->transfer->bEndpointAddress = ep_address | USB_DIR_OUT;
  trq->transfer->num_bytes = length;
  memcpy(trq->transfer->data_buffer, data, length);
  auto err = usb_host_transfer_submit(trq->transfer);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to submit transfer, address=%x, length=%d, err=%x", ep_address, length, err);
    this->release_trq(trq);
  }
}
void USBClient::dump_config() {
  ESP_LOGCONFIG(TAG,
                "USBClient\n"
                "  Vendor id %04X\n"
                "  Product id %04X",
                this->vid_, this->pid_);
}
void USBClient::release_trq(TransferRequest *trq) { this->trq_pool_.push_back(trq); }

}  // namespace usb_host
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
