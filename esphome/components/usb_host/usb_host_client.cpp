// Should not be needed, but it's required to pass CI clang-tidy checks
#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_host.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/application.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#include <cinttypes>
#include <cstring>
#include <atomic>
#include <span>

namespace esphome::usb_host {

#pragma GCC diagnostic ignored "-Wparentheses"

using namespace bytebuffer;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
static void print_ep_desc(const usb_ep_desc_t *ep_desc) {
  const char *ep_type_str;
  int type = ep_desc->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK;
  switch (type) {
    case USB_BM_ATTRIBUTES_XFER_CONTROL: ep_type_str = "CTRL"; break;
    case USB_BM_ATTRIBUTES_XFER_ISOC:   ep_type_str = "ISOC"; break;
    case USB_BM_ATTRIBUTES_XFER_BULK:   ep_type_str = "BULK"; break;
    case USB_BM_ATTRIBUTES_XFER_INT:    ep_type_str = "INT";  break;
    default:                             ep_type_str = NULL;   break;
  }
  ESP_LOGV(TAG,
           "\t\t*** Endpoint descriptor ***\n"
           "\t\tbLength %d\n"
           "\t\tbDescriptorType %d\n"
           "\t\tbEndpointAddress 0x%x\tEP %d %s\n"
           "\t\tbmAttributes 0x%x\t%s\n"
           "\t\twMaxPacketSize %d\n"
           "\t\tbInterval %d",
           ep_desc->bLength, ep_desc->bDescriptorType, ep_desc->bEndpointAddress,
           USB_EP_DESC_GET_EP_NUM(ep_desc), USB_EP_DESC_GET_EP_DIR(ep_desc) ? "IN" : "OUT",
           ep_desc->bmAttributes, ep_type_str, ep_desc->wMaxPacketSize, ep_desc->bInterval);
}

static void usbh_print_intf_desc(const usb_intf_desc_t *intf_desc) {
  ESP_LOGV(TAG,
           "\t*** Interface descriptor ***\n"
           "\tbLength %d\n"
           "\tbDescriptorType %d\n"
           "\tbInterfaceNumber %d\n"
           "\tbAlternateSetting %d\n"
           "\tbNumEndpoints %d\n"
           "\tbInterfaceClass 0x%x\n"
           "\tiInterface %d",
           intf_desc->bLength, intf_desc->bDescriptorType, intf_desc->bInterfaceNumber,
           intf_desc->bAlternateSetting, intf_desc->bNumEndpoints, intf_desc->bInterfaceProtocol,
           intf_desc->iInterface);
}

static void usbh_print_cfg_desc(const usb_config_desc_t *cfg_desc) {
  ESP_LOGV(TAG,
           "*** Configuration descriptor ***\n"
           "  bLength %d\n"
           "  bDescriptorType %d\n"
           "  wTotalLength %d\n"
           "  bNumInterfaces %d\n"
           "  bConfigurationValue %d\n"
           "  iConfiguration %d\n"
           "  bmAttributes 0x%x\n"
           "  bMaxPower %dmA",
           cfg_desc->bLength, cfg_desc->bDescriptorType, cfg_desc->wTotalLength,
           cfg_desc->bNumInterfaces, cfg_desc->bConfigurationValue, cfg_desc->iConfiguration,
           cfg_desc->bmAttributes, cfg_desc->bMaxPower * 2);
}

static void usb_client_print_device_descriptor(const usb_device_desc_t *devc_desc) {
  if (devc_desc == nullptr)
    return;
  ESP_LOGV(TAG,
           "*** Device descriptor ***\n"
           "  bLength %d\n"
           "  bDescriptorType %d\n"
           "  bcdUSB %d.%d0\n"
           "  bDeviceClass 0x%x\n"
           "  bDeviceSubClass 0x%x\n"
           "  bDeviceProtocol 0x%x\n"
           "  bMaxPacketSize0 %d\n"
           "  idVendor 0x%x\n"
           "  idProduct 0x%x\n"
           "  bcdDevice %d.%d0\n"
           "  iManufacturer %d\n"
           "  iProduct %d\n"
           "  iSerialNumber %d\n"
           "  bNumConfigurations %d",
           devc_desc->bLength, devc_desc->bDescriptorType,
           ((devc_desc->bcdUSB >> 8) & 0xF), ((devc_desc->bcdUSB >> 4) & 0xF),
           devc_desc->bDeviceClass, devc_desc->bDeviceSubClass, devc_desc->bDeviceProtocol,
           devc_desc->bMaxPacketSize0, devc_desc->idVendor, devc_desc->idProduct,
           ((devc_desc->bcdDevice >> 8) & 0xF), ((devc_desc->bcdDevice >> 4) & 0xF),
           devc_desc->iManufacturer, devc_desc->iProduct,
           devc_desc->iSerialNumber, devc_desc->bNumConfigurations);
}

static void usb_client_print_config_descriptor(const usb_config_desc_t *cfg_desc,
                                               print_class_descriptor_cb class_specific_cb) {
  if (cfg_desc == nullptr)
    return;
  int offset = 0;
  uint16_t w_total_length = cfg_desc->wTotalLength;
  const usb_standard_desc_t *next_desc = reinterpret_cast<const usb_standard_desc_t *>(cfg_desc);
  do {
    switch (next_desc->bDescriptorType) {
      case USB_W_VALUE_DT_CONFIG:
        usbh_print_cfg_desc(reinterpret_cast<const usb_config_desc_t *>(next_desc));
        break;
      case USB_W_VALUE_DT_INTERFACE:
        usbh_print_intf_desc(reinterpret_cast<const usb_intf_desc_t *>(next_desc));
        break;
      case USB_W_VALUE_DT_ENDPOINT:
        print_ep_desc(reinterpret_cast<const usb_ep_desc_t *>(next_desc));
        break;
      default:
        if (class_specific_cb)
          class_specific_cb(next_desc);
        break;
    }
    next_desc = usb_parse_next_descriptor(next_desc, w_total_length, &offset);
  } while (next_desc != nullptr);
}
#endif  // ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE

static constexpr size_t DESC_STRING_BUF_SIZE = 128;

static const char *get_descriptor_string(const usb_str_desc_t *desc,
                                          std::span<char, DESC_STRING_BUF_SIZE> buffer) {
  if (desc == nullptr || desc->bLength < 2)
    return "(unspecified)";
  int char_count = (desc->bLength - 2) / 2;
  char *p = buffer.data();
  char *end = p + buffer.size() - 1;
  for (int i = 0; i != char_count && p < end; i++) {
    auto c = desc->wData[i];
    if (c < 0x100)
      *p++ = static_cast<char>(c);
  }
  *p = '\0';
  return buffer.data();
}

// ── Static transfer callbacks (USB task context) ──────────────────────────────

// Shared completion logic: fill status, fire callback, release slot.
static void complete_trq_(TransferRequest *trq, const usb_transfer_t *xfer) {
  trq->status.error_code = xfer->status;
  trq->status.success = xfer->status == USB_TRANSFER_STATUS_COMPLETED;
  trq->status.endpoint = xfer->bEndpointAddress;
  trq->status.data = xfer->data_buffer;
  trq->status.data_len = xfer->actual_num_bytes;
  if (trq->callback != nullptr)
    trq->callback(trq->status);
  trq->client->release_trq(trq);
}

#ifdef USE_USB_CONTROL_TRANSFERS
static void control_callback(const usb_transfer_t *xfer) {
  complete_trq_(static_cast<TransferRequest *>(xfer->context), xfer);
}
#endif

#if defined(USE_USB_BULK_TRANSFERS) || defined(USE_USB_CONTROL_TRANSFERS)
static void transfer_callback(usb_transfer_t *xfer) {
  complete_trq_(static_cast<TransferRequest *>(xfer->context), xfer);
}
#endif

// ── USBClient event callback (USB task context) ───────────────────────────────

void USBClient::client_event_cb(const usb_host_client_event_msg_t *event_msg, void *ptr) {
  auto *client = static_cast<USBClient *>(ptr);
  UsbEvent *event = client->event_pool.allocate();
  if (event == nullptr) {
    client->event_queue.increment_dropped_count();
    return;
  }
  switch (event_msg->event) {
    case USB_HOST_CLIENT_EVENT_NEW_DEV:
      ESP_LOGD(TAG, "New device %d", event_msg->new_dev.address);
      event->type = EVENT_DEVICE_NEW;
      event->data.device_new.address = event_msg->new_dev.address;
      break;
    case USB_HOST_CLIENT_EVENT_DEV_GONE:
      ESP_LOGD(TAG, "Device gone");
      event->type = EVENT_DEVICE_GONE;
      event->data.device_gone.handle = event_msg->dev_gone.dev_hdl;
      break;
    default:
      ESP_LOGD(TAG, "Unknown event %d", event_msg->event);
      client->event_pool.release(event);
      return;
  }
  client->event_queue.push(event);
  client->enable_loop_soon_any_context();
  App.wake_loop_threadsafe();
}

// ── USBClient lifecycle ───────────────────────────────────────────────────────

void USBClient::setup() {
  usb_host_client_config_t config{
      .is_synchronous = false,
      .max_num_event_msg = 5,
      .async = {.client_event_callback = USBClient::client_event_cb, .callback_arg = this}};
  auto err = usb_host_client_register(&config, &this->handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "client register failed: %s", esp_err_to_name(err));
    this->status_set_error(LOG_STR("Client register failed"));
    this->mark_failed();
    return;
  }
  // Pre-allocate transfer buffers for all pool slots at setup — no runtime allocation
  for (auto &request : this->requests_) {
    usb_host_transfer_alloc(USB_MAX_PACKET_SIZE, 0, &request.transfer);
    request.client = this;
  }
  xTaskCreate(usb_task_fn, "usb_task", USB_TASK_STACK_SIZE, this, USB_TASK_PRIORITY,
              &this->usb_task_handle_);
  if (this->usb_task_handle_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create USB task");
    this->mark_failed();
  }
}

void USBClient::usb_task_fn(void *arg) {
  static_cast<USBClient *>(arg)->usb_task_loop_();
}

void USBClient::usb_task_loop_() const {
  while (true)
    usb_host_client_handle_events(this->handle_, portMAX_DELAY);
}

bool USBClient::process_usb_events_() {
  bool had_work = false;
  UsbEvent *event;
  while ((event = this->event_queue.pop()) != nullptr) {
    had_work = true;
    switch (event->type) {
      case EVENT_DEVICE_NEW:
        this->on_opened(event->data.device_new.address);
        break;
      case EVENT_DEVICE_GONE:
        this->on_removed(event->data.device_gone.handle);
        break;
    }
    this->event_pool.release(event);
  }
  uint16_t dropped = this->event_queue.get_and_reset_dropped_count();
  if (dropped > 0)
    ESP_LOGW(TAG, "Dropped %u USB events due to queue overflow", dropped);
  if (this->state_ == USB_CLIENT_OPEN) {
    had_work = true;
    this->handle_open_state_();
  }
  return had_work;
}

void USBClient::loop() {
  if (!this->process_usb_events_())
    this->disable_loop();
}

void USBClient::handle_open_state_() {
  int err;
  ESP_LOGD(TAG, "Open device %d", this->device_addr_);
  err = usb_host_device_open(this->handle_, this->device_addr_, &this->device_handle_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Device open failed: %s", esp_err_to_name(err));
    this->state_ = USB_CLIENT_INIT;
    return;
  }
  const usb_device_desc_t *desc;
  err = usb_host_get_device_descriptor(this->device_handle_, &desc);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Device get_desc failed: %s", esp_err_to_name(err));
    this->disconnect();
    return;
  }
  this->device_desc_ = desc;
  ESP_LOGD(TAG, "Device descriptor: vid %X pid %X", desc->idVendor, desc->idProduct);
  if (desc->idVendor != this->vid_ || desc->idProduct != this->pid_) {
    if (this->vid_ != 0 || this->pid_ != 0) {
      ESP_LOGD(TAG, "Not our device, closing");
      this->disconnect();
      return;
    }
  }
  {
    const usb_config_desc_t *cfg_desc;
    if (usb_host_get_active_config_descriptor(this->device_handle_, &cfg_desc) != ESP_OK) {
      this->disconnect();
      return;
    }
    this->config_desc_ = cfg_desc;
  }
  uint8_t required_class = this->get_interface_class();
  if (required_class != USB_INTERFACE_CLASS_ANY) {
    bool found = false;
    int offset = 0;
    const usb_standard_desc_t *next = reinterpret_cast<const usb_standard_desc_t *>(this->config_desc_);
    while ((next = usb_parse_next_descriptor_of_type(next, this->config_desc_->wTotalLength,
                                                     USB_W_VALUE_DT_INTERFACE, &offset)) != nullptr) {
      if (reinterpret_cast<const usb_intf_desc_t *>(next)->bInterfaceClass == required_class) {
        found = true;
        break;
      }
    }
    if (!found) {
      ESP_LOGD(TAG, "Device has no interface class 0x%02X, closing", required_class);
      this->disconnect();
      return;
    }
  }
  usb_device_info_t dev_info;
  err = usb_host_device_info(this->device_handle_, &dev_info);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Device info failed: %s", esp_err_to_name(err));
    this->disconnect();
    return;
  }
  this->state_ = USB_CLIENT_CONNECTED;
  char buf_manuf[DESC_STRING_BUF_SIZE];
  char buf_product[DESC_STRING_BUF_SIZE];
  char buf_serial[DESC_STRING_BUF_SIZE];
  ESP_LOGD(TAG, "Device connected: Manuf: %s; Prod: %s; Serial: %s",
           get_descriptor_string(dev_info.str_desc_manufacturer, buf_manuf),
           get_descriptor_string(dev_info.str_desc_product, buf_product),
           get_descriptor_string(dev_info.str_desc_serial_num, buf_serial));
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  usb_client_print_device_descriptor(this->device_desc_);
  usb_client_print_config_descriptor(this->config_desc_, nullptr);
#endif
  this->on_connected();
}

void USBClient::on_opened(uint8_t addr) {
  if (this->state_ == USB_CLIENT_INIT) {
    this->device_addr_ = addr;
    this->state_ = USB_CLIENT_OPEN;
  }
}

void USBClient::on_removed(usb_device_handle_t handle) {
  if (this->device_handle_ == handle)
    this->disconnect();
}

void USBClient::disconnect() {
  this->on_disconnected();
  auto err = usb_host_device_close(this->handle_, this->device_handle_);
  if (err != ESP_OK)
    ESP_LOGE(TAG, "Device close failed: %s", esp_err_to_name(err));
  this->state_ = USB_CLIENT_INIT;
  this->device_handle_ = nullptr;
  this->device_addr_ = -1;
  this->device_desc_ = nullptr;
  this->config_desc_ = nullptr;
}

void USBClient::dump_config() {
  ESP_LOGCONFIG(TAG, "USBClient\n  Vendor id %04X\n  Product id %04X", this->vid_, this->pid_);
}

// ── TransferRequest pool management (lock-free, thread-safe) ─────────────────

TransferRequest *USBClient::get_trq_() {
  trq_bitmask_t mask = this->trq_in_use_.load(std::memory_order_acquire);
  for (;;) {
    if (mask == ALL_REQUESTS_IN_USE) {
      ESP_LOGE(TAG, "All %zu transfer slots in use", MAX_REQUESTS);
      return nullptr;
    }
    trq_bitmask_t lsb = ~mask & (mask + 1);
    trq_bitmask_t desired = mask | lsb;
    if (this->trq_in_use_.compare_exchange_weak(mask, desired, std::memory_order::acquire)) {
      auto i = __builtin_ctz(lsb);
      auto *trq = &this->requests_[i];
      trq->transfer->context = trq;
      trq->transfer->device_handle = this->device_handle_;
      return trq;
    }
  }
}

void USBClient::release_trq(TransferRequest *trq) {
  if (trq == nullptr)
    return;
  size_t index = trq - this->requests_;
  if (index >= MAX_REQUESTS) {
    ESP_LOGE(TAG, "Invalid TransferRequest pointer");
    return;
  }
  trq_bitmask_t mask = ~(static_cast<trq_bitmask_t>(1) << index);
  this->trq_in_use_.fetch_and(mask, std::memory_order_release);
}

// ── Thin forwarders — allocate trq, fill fields, delegate to USBHost ──────────

#ifdef USE_USB_BULK_TRANSFERS

bool USBClient::transfer_in(uint8_t ep_address, const transfer_cb_t &callback, uint16_t length) {
  auto *trq = this->get_trq_();
  if (trq == nullptr) {
    ESP_LOGE(TAG, "transfer_in: no free slots");
    return false;
  }
  if (length > trq->transfer->data_buffer_size) {
    ESP_LOGE(TAG, "transfer_in: length %u > buffer %u", length, trq->transfer->data_buffer_size);
    this->release_trq(trq);
    return false;
  }
  trq->callback = callback;
  trq->transfer->callback = transfer_callback;
  trq->transfer->bEndpointAddress = ep_address | USB_DIR_IN;
  // IN transfers: num_bytes must be an integer multiple of MPS
  trq->transfer->num_bytes = ((length + USB_MAX_PACKET_SIZE - 1) / USB_MAX_PACKET_SIZE) * USB_MAX_PACKET_SIZE;
  if (!global_usb_host->submit_transfer_(trq)) {
    this->release_trq(trq);
    return false;
  }
  return true;
}

bool USBClient::transfer_out(uint8_t ep_address, const transfer_cb_t &callback,
                              const uint8_t *data, uint16_t length) {
  auto *trq = this->get_trq_();
  if (trq == nullptr) {
    ESP_LOGE(TAG, "transfer_out: no free slots");
    return false;
  }
  if (length > trq->transfer->data_buffer_size) {
    ESP_LOGE(TAG, "transfer_out: length %u > buffer %u", length, trq->transfer->data_buffer_size);
    this->release_trq(trq);
    return false;
  }
  trq->callback = callback;
  trq->transfer->callback = transfer_callback;
  trq->transfer->bEndpointAddress = ep_address | USB_DIR_OUT;
  trq->transfer->num_bytes = length;
  memcpy(trq->transfer->data_buffer, data, length);
  if (!global_usb_host->submit_transfer_(trq)) {
    this->release_trq(trq);
    return false;
  }
  return true;
}

#endif  // USE_USB_BULK_TRANSFERS

#ifdef USE_USB_CONTROL_TRANSFERS

bool USBClient::control_transfer(uint8_t type, uint8_t request, uint16_t value, uint16_t index,
                                  const transfer_cb_t &callback, const std::vector<uint8_t> &data) {
  auto *trq = this->get_trq_();
  if (trq == nullptr)
    return false;
  auto length = data.size();
  if (length > trq->transfer->data_buffer_size - SETUP_PACKET_SIZE) {
    ESP_LOGE(TAG, "control_transfer: data too large: %u", length);
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
  if (length != 0 && !(type & USB_DIR_IN))
    memcpy(trq->transfer->data_buffer + SETUP_PACKET_SIZE, data.data(), length);
  trq->callback = callback;
  trq->transfer->bEndpointAddress = type & USB_DIR_MASK;
  trq->transfer->num_bytes = static_cast<int>(length + SETUP_PACKET_SIZE);
  trq->transfer->callback = reinterpret_cast<usb_transfer_cb_t>(control_callback);
  if (!global_usb_host->submit_control_(this->handle_, trq)) {
    this->release_trq(trq);
    return false;
  }
  return true;
}

bool USBClient::set_interface(uint8_t interface_num, uint8_t alt_setting) {
  return global_usb_host->do_set_interface_(this->handle_, this->device_handle_,
                                             interface_num, alt_setting);
}

#endif  // USE_USB_CONTROL_TRANSFERS

bool USBClient::claim_interface(uint8_t interface_num, uint8_t alt_setting) {
  return global_usb_host->do_claim_interface_(this->handle_, this->device_handle_,
                                               interface_num, alt_setting);
}

bool USBClient::release_interface(uint8_t interface_num) {
  return global_usb_host->do_release_interface_(this->handle_, this->device_handle_,
                                                 interface_num);
}

// ── Isochronous thin forwarders ───────────────────────────────────────────────
#ifdef USE_USB_ISOC_TRANSFERS

usb_transfer_t *USBClient::isoc_alloc(uint8_t ep_addr, uint16_t mps, uint8_t num_packets,
                                       usb_transfer_cb_t callback, void *context) {
  return global_usb_host->do_isoc_alloc_(ep_addr, this->device_handle_, mps, num_packets,
                                          callback, context);
}

bool USBClient::isoc_submit(usb_transfer_t *xfer) {
  return global_usb_host->do_isoc_submit_(xfer);
}

void USBClient::isoc_free(usb_transfer_t *xfer) {
  global_usb_host->do_isoc_free_(xfer);
}

bool USBClient::stream_open_(IsocStream &stream, USBClient *cb) {
  return global_usb_host->stream_open_(stream, cb, this->handle_, this->device_handle_);
}

void USBClient::stream_close_(IsocStream &stream) {
  global_usb_host->stream_close_(stream, this->handle_, this->device_handle_);
}

#endif  // USE_USB_ISOC_TRANSFERS

}  // namespace esphome::usb_host
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
