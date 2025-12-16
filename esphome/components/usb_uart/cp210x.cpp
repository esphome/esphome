#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"

#include "esphome/components/bytebuffer/bytebuffer.h"

namespace esphome {
namespace usb_uart {

using namespace bytebuffer;
/**
 * Silabs CP210x Commands
 */

static constexpr uint8_t IFC_ENABLE = 0x00;       // Enable or disable the interface.
static constexpr uint8_t SET_BAUDDIV = 0x01;      // Set the baud rate divisor.
static constexpr uint8_t GET_BAUDDIV = 0x02;      // Get the baud rate divisor.
static constexpr uint8_t SET_LINE_CTL = 0x03;     // Set the line control.
static constexpr uint8_t GET_LINE_CTL = 0x04;     // Get the line control.
static constexpr uint8_t SET_BREAK = 0x05;        // Set a BREAK.
static constexpr uint8_t IMM_CHAR = 0x06;         // Send character out of order.
static constexpr uint8_t SET_MHS = 0x07;          // Set modem handshaking.
static constexpr uint8_t GET_MDMSTS = 0x08;       // Get modem status.
static constexpr uint8_t SET_XON = 0x09;          // Emulate XON.
static constexpr uint8_t SET_XOFF = 0x0A;         // Emulate XOFF.
static constexpr uint8_t SET_EVENTMASK = 0x0B;    // Set the event mask.
static constexpr uint8_t GET_EVENTMASK = 0x0C;    // Get the event mask.
static constexpr uint8_t GET_EVENTSTATE = 0x16;   // Get the event state.
static constexpr uint8_t SET_RECEIVE = 0x17;      // Set receiver max timeout.
static constexpr uint8_t GET_RECEIVE = 0x18;      // Get receiver max timeout.
static constexpr uint8_t SET_CHAR = 0x0D;         // Set special character individually.
static constexpr uint8_t GET_CHARS = 0x0E;        // Get special characters.
static constexpr uint8_t GET_PROPS = 0x0F;        // Get properties.
static constexpr uint8_t GET_COMM_STATUS = 0x10;  // Get the serial status.
static constexpr uint8_t RESET = 0x11;            // Reset.
static constexpr uint8_t PURGE = 0x12;            // Purge.
static constexpr uint8_t SET_FLOW = 0x13;         // Set flow control.
static constexpr uint8_t GET_FLOW = 0x14;         // Get flow control.
static constexpr uint8_t EMBED_EVENTS = 0x15;     // Control embedding of events in the data stream.
static constexpr uint8_t GET_BAUDRATE = 0x1D;     // Get the baud rate.
static constexpr uint8_t SET_BAUDRATE = 0x1E;     // Set the baud rate.
static constexpr uint8_t SET_CHARS = 0x19;        // Set special characters.
static constexpr uint8_t VENDOR_SPECIFIC = 0xFF;  // Vendor specific command.

std::vector<CdcEps> USBUartTypeCP210X::parse_descriptors(usb_device_handle_t dev_hdl) {
  const usb_config_desc_t *config_desc;
  const usb_device_desc_t *device_desc;
  int conf_offset = 0, ep_offset;
  std::vector<CdcEps> cdc_devs{};

  // Get required descriptors
  if (usb_host_get_device_descriptor(dev_hdl, &device_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_device_descriptor failed");
    return {};
  }
  if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_active_config_descriptor failed");
    return {};
  }
  ESP_LOGD(TAG, "bDeviceClass: %u, bDeviceSubClass: %u", device_desc->bDeviceClass, device_desc->bDeviceSubClass);
  ESP_LOGD(TAG, "bNumInterfaces: %u", config_desc->bNumInterfaces);
  if (device_desc->bDeviceClass != 0) {
    ESP_LOGE(TAG, "bDeviceClass != 0");
    return {};
  }

  for (uint8_t i = 0; i != config_desc->bNumInterfaces; i++) {
    auto data_desc = usb_parse_interface_descriptor(config_desc, 0, 0, &conf_offset);
    if (!data_desc) {
      ESP_LOGE(TAG, "data_desc: usb_parse_interface_descriptor failed");
      break;
    }
    if (data_desc->bNumEndpoints != 2 || data_desc->bInterfaceClass != USB_CLASS_VENDOR_SPEC) {
      ESP_LOGE(TAG, "data_desc: bInterfaceClass == %u, bInterfaceSubClass == %u, bNumEndpoints == %u",
               data_desc->bInterfaceClass, data_desc->bInterfaceSubClass, data_desc->bNumEndpoints);
      continue;
    }
    ep_offset = conf_offset;
    auto out_ep = usb_parse_endpoint_descriptor_by_index(data_desc, 0, config_desc->wTotalLength, &ep_offset);
    if (!out_ep) {
      ESP_LOGE(TAG, "out_ep: usb_parse_endpoint_descriptor_by_index failed");
      continue;
    }
    ep_offset = conf_offset;
    auto in_ep = usb_parse_endpoint_descriptor_by_index(data_desc, 1, config_desc->wTotalLength, &ep_offset);
    if (!in_ep) {
      ESP_LOGE(TAG, "in_ep: usb_parse_endpoint_descriptor_by_index failed");
      continue;
    }
    if (in_ep->bEndpointAddress & usb_host::USB_DIR_IN) {
      cdc_devs.push_back({CdcEps{nullptr, in_ep, out_ep, data_desc->bInterfaceNumber}});
    } else {
      cdc_devs.push_back({CdcEps{nullptr, out_ep, in_ep, data_desc->bInterfaceNumber}});
    }
  }
  return cdc_devs;
}

void USBUartTypeCP210X::enable_channels() {
  // enable the channels
  for (auto channel : this->channels_) {
    if (!channel->initialised_.load())
      continue;
    usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
      if (!status.success) {
        ESP_LOGE(TAG, "Control transfer failed, status=%s", esp_err_to_name(status.error_code));
        channel->initialised_.store(false);
      }
    };
    this->control_transfer(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, IFC_ENABLE, 1, channel->index_, callback);
    uint16_t line_control = channel->stop_bits_;
    line_control |= static_cast<uint8_t>(channel->parity_) << 4;
    line_control |= channel->data_bits_ << 8;
    ESP_LOGD(TAG, "Line control value 0x%X", line_control);
    this->control_transfer(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, SET_LINE_CTL, line_control, channel->index_,
                           callback);
    auto baud = ByteBuffer::wrap(channel->baud_rate_, LITTLE);
    this->control_transfer(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, SET_BAUDRATE, 0, channel->index_, callback,
                           baud.get_data());
  }
  USBUartTypeCdcAcm::enable_channels();
}
}  // namespace usb_uart
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
