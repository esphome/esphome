#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::usb_uart {

// Control request types
static constexpr uint8_t SET_LINE_REQUEST_TYPE = 0x21;
static constexpr uint8_t SET_LINE_REQUEST = 0x20;

static constexpr uint8_t SET_CONTROL_REQUEST_TYPE = 0x21;
static constexpr uint8_t SET_CONTROL_REQUEST = 0x22;
static constexpr uint8_t CONTROL_DTR = 0x01;
static constexpr uint8_t CONTROL_RTS = 0x02;

static constexpr uint8_t VENDOR_WRITE_REQUEST_TYPE = 0x40;
static constexpr uint8_t VENDOR_WRITE_REQUEST = 0x01;

static constexpr uint8_t VENDOR_READ_REQUEST_TYPE = 0xc0;
static constexpr uint8_t VENDOR_READ_REQUEST = 0x01;

// Supported standard baud rates for direct encoding (TYPE_H, TYPE_HX, TYPE_HXD, TYPE_HXN)
static const uint32_t SUPPORTED_BAUD_RATES[] = {
    75,    150,   300,   600,    1200,   1800,   2400,   3600,   4800,    7200,    9600,    14400,   19200,
    28800, 38400, 57600, 115200, 230400, 460800, 614400, 921600, 1228800, 2457600, 3000000, 6000000,
};

static const char *pl2303_type_name(Pl2303ChipType type) {
  switch (type) {
    case PL2303_TYPE_H:
      return "H (legacy)";
    case PL2303_TYPE_HX:
      return "HX";
    case PL2303_TYPE_TA:
      return "TA";
    case PL2303_TYPE_TB:
      return "TB";
    case PL2303_TYPE_HXD:
      return "HXD";
    case PL2303_TYPE_HXN:
      return "G/HXN (newer)";
    default:
      return "unknown";
  }
}

// Find nearest supported baud rate for direct encoding
static uint32_t nearest_supported_baud(uint32_t baud) {
  size_t n = sizeof(SUPPORTED_BAUD_RATES) / sizeof(SUPPORTED_BAUD_RATES[0]);
  for (size_t i = 0; i < n; i++) {
    if (SUPPORTED_BAUD_RATES[i] > baud) {
      if (i == 0)
        return SUPPORTED_BAUD_RATES[0];
      uint32_t lower = SUPPORTED_BAUD_RATES[i - 1];
      uint32_t upper = SUPPORTED_BAUD_RATES[i];
      return (upper - baud) > (baud - lower) ? lower : upper;
    }
  }
  return SUPPORTED_BAUD_RATES[n - 1];
}

// Direct encoding: little-endian 32-bit baud rate value
static void encode_baud_direct(uint8_t buf[4], uint32_t baud) {
  buf[0] = baud & 0xFF;
  buf[1] = (baud >> 8) & 0xFF;
  buf[2] = (baud >> 16) & 0xFF;
  buf[3] = (baud >> 24) & 0xFF;
}

// Divisor encoding for TYPE_HX, TYPE_HXD: baudrate = 12M*32 / (mantissa * 4^exponent)
static void encode_baud_divisor(uint8_t buf[4], uint32_t baud) {
  static constexpr uint32_t BASELINE = 12000000 * 32;
  uint32_t mantissa = BASELINE / baud;
  if (mantissa == 0)
    mantissa = 1;
  uint8_t exponent = 0;
  while (mantissa >= 512) {
    if (exponent < 7) {
      mantissa >>= 2;
      exponent++;
    } else {
      mantissa = 511;
      break;
    }
  }
  buf[3] = 0x80;
  buf[2] = 0;
  buf[1] = (exponent << 1) | (mantissa >> 8);
  buf[0] = mantissa & 0xFF;
}

// Alt divisor encoding for TYPE_TA, TYPE_TB: baudrate = 12M*32 / (mantissa * 2^exponent)
static void encode_baud_divisor_alt(uint8_t buf[4], uint32_t baud) {
  static constexpr uint32_t BASELINE = 12000000 * 32;
  uint32_t mantissa = BASELINE / baud;
  if (mantissa == 0)
    mantissa = 1;
  uint8_t exponent = 0;
  while (mantissa >= 2048) {
    if (exponent < 15) {
      mantissa >>= 1;
      exponent++;
    } else {
      mantissa = 2047;
      break;
    }
  }
  buf[3] = 0x80;
  buf[2] = exponent & 0x01;
  buf[1] = ((exponent & ~0x01) << 4) | (mantissa >> 8);
  buf[0] = mantissa & 0xFF;
}

std::vector<CdcEps> USBUartTypePL2303::parse_descriptors(usb_device_handle_t dev_hdl) {
  const usb_config_desc_t *config_desc;
  const usb_device_desc_t *device_desc;
  std::vector<CdcEps> cdc_devs{};

  if (usb_host_get_device_descriptor(dev_hdl, &device_desc) != ESP_OK) {
    ESP_LOGE(TAG, "PL2303: get_device_descriptor failed");
    return {};
  }
  if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) != ESP_OK) {
    ESP_LOGE(TAG, "PL2303: get_active_config_descriptor failed");
    return {};
  }

  // Detect chip type from USB descriptor fields (mirrors pl2303_detect_type in Linux driver)
  uint16_t bcd_device = device_desc->bcdDevice;
  uint16_t bcd_usb = device_desc->bcdUSB;
  uint8_t bmax_packet = device_desc->bMaxPacketSize0;
  uint8_t bdev_class = device_desc->bDeviceClass;

  if (bdev_class == 0x02 || bmax_packet != 0x40) {
    this->chip_type_ = PL2303_TYPE_H;
  } else {
    switch (bcd_usb) {
      case 0x0101:
      case 0x0110:
        this->chip_type_ = (bcd_device == 0x0400) ? PL2303_TYPE_HXD : PL2303_TYPE_HX;
        break;
      default:
        // TA and TB are distinguishable by bcdDevice without any USB probe.
        if (bcd_device == 0x0300) {
          this->chip_type_ = PL2303_TYPE_TA;
        } else if (bcd_device == 0x0500) {
          this->chip_type_ = PL2303_TYPE_TB;
        } else {
          this->chip_type_ = PL2303_TYPE_HXN;
        }
        break;
    }
  }

  ESP_LOGI(TAG, "PL2303 chip type: %s (bcdUSB=0x%04X bcdDevice=0x%04X bMaxPkt=%u)", pl2303_type_name(this->chip_type_),
           bcd_usb, bcd_device, bmax_packet);

  // PL2303 is single-port: find first interface with 2 bulk endpoints
  int conf_offset = 0;
  for (uint8_t i = 0; i < config_desc->bNumInterfaces; i++) {
    int ep_offset = conf_offset;
    const auto *intf = usb_parse_interface_descriptor(config_desc, i, 0, &conf_offset);
    if (!intf)
      break;
    if (intf->bNumEndpoints < 2)
      continue;

    const usb_ep_desc_t *in_ep = nullptr;
    const usb_ep_desc_t *out_ep = nullptr;
    const usb_ep_desc_t *notify_ep = nullptr;

    for (uint8_t e = 0; e < intf->bNumEndpoints; e++) {
      ep_offset = conf_offset;
      const auto *ep = usb_parse_endpoint_descriptor_by_index(intf, e, config_desc->wTotalLength, &ep_offset);
      if (!ep)
        break;
      if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK) {
        if (ep->bEndpointAddress & usb_host::USB_DIR_IN) {
          in_ep = ep;
        } else {
          out_ep = ep;
        }
      } else if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_INT) {
        notify_ep = ep;
      }
    }

    if (in_ep && out_ep) {
      cdc_devs.push_back(CdcEps{notify_ep, in_ep, out_ep, intf->bInterfaceNumber, intf->bInterfaceNumber});
      break;  // PL2303 is single-port
    }
  }

  if (cdc_devs.empty())
    ESP_LOGE(TAG, "PL2303: failed to find bulk IN+OUT endpoints");

  return cdc_devs;
}

void USBUartTypePL2303::enable_channels() {
  if (this->channels_.empty())
    return;

  auto *channel = this->channels_[0];
  bool is_legacy = (this->chip_type_ == PL2303_TYPE_H);
  bool is_hxn = (this->chip_type_ == PL2303_TYPE_HXN);

  usb_host::transfer_cb_t nop_cb = [](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGW(TAG, "PL2303: vendor init transfer failed");
  };

  // Init sequence for non-HXN chips (mirrors pl2303_startup in Linux driver):
  // Read 0x8484, write 0x0404=0, read 0x8484, read 0x8383, read 0x8484,
  // write 0x0404=1, read 0x8484, read 0x8383,
  // write 0=1, write 1=0, write 2=0x24 (legacy) or 0x44 (HX+)
  if (!is_hxn) {
    uint8_t req = VENDOR_READ_REQUEST;
    uint8_t wreq = VENDOR_WRITE_REQUEST;

    // Fire-and-forget vendor reads: result discarded, chip requires this sequence.
    // Pass a 1-byte buffer to set wLength=1 so the IN data stage is performed.
    this->control_transfer(VENDOR_READ_REQUEST_TYPE, req, 0x8484, 0, nop_cb, {0});
    this->control_transfer(VENDOR_WRITE_REQUEST_TYPE, wreq, 0x0404, 0, nop_cb);
    this->control_transfer(VENDOR_READ_REQUEST_TYPE, req, 0x8484, 0, nop_cb, {0});
    this->control_transfer(VENDOR_READ_REQUEST_TYPE, req, 0x8383, 0, nop_cb, {0});
    this->control_transfer(VENDOR_READ_REQUEST_TYPE, req, 0x8484, 0, nop_cb, {0});
    this->control_transfer(VENDOR_WRITE_REQUEST_TYPE, wreq, 0x0404, 1, nop_cb);
    this->control_transfer(VENDOR_READ_REQUEST_TYPE, req, 0x8484, 0, nop_cb, {0});
    this->control_transfer(VENDOR_READ_REQUEST_TYPE, req, 0x8383, 0, nop_cb, {0});
    this->control_transfer(VENDOR_WRITE_REQUEST_TYPE, wreq, 0, 1, nop_cb);
    this->control_transfer(VENDOR_WRITE_REQUEST_TYPE, wreq, 1, 0, nop_cb);
    this->control_transfer(VENDOR_WRITE_REQUEST_TYPE, wreq, 2, is_legacy ? 0x24 : 0x44, nop_cb);
  }

  // Build 7-byte line coding structure:
  // [0-3] baud rate (LE32), [4] stop bits, [5] parity, [6] data bits
  uint8_t line_coding[7] = {};
  uint32_t baud = channel->get_baud_rate();

  // Choose baud encoding based on chip type
  uint32_t nearest = nearest_supported_baud(baud);
  if (baud == nearest || this->chip_type_ == PL2303_TYPE_HXN) {
    encode_baud_direct(line_coding, baud);
  } else if (this->chip_type_ == PL2303_TYPE_TA || this->chip_type_ == PL2303_TYPE_TB) {
    encode_baud_divisor_alt(line_coding, baud);
  } else {
    encode_baud_divisor(line_coding, baud);
  }

  // Stop bits: 0=1, 1=1.5, 2=2
  switch (channel->get_stop_bits()) {
    case 2:
      line_coding[4] = 2;
      break;
    default:
      line_coding[4] = 0;
      break;
  }

  // Parity: 0=none, 1=odd, 2=even, 3=mark, 4=space
  switch (channel->parity_) {
    case UART_CONFIG_PARITY_ODD:
      line_coding[5] = 1;
      break;
    case UART_CONFIG_PARITY_EVEN:
      line_coding[5] = 2;
      break;
    case UART_CONFIG_PARITY_MARK:
      line_coding[5] = 3;
      break;
    case UART_CONFIG_PARITY_SPACE:
      line_coding[5] = 4;
      break;
    default:
      line_coding[5] = 0;
      break;
  }

  // Data bits
  line_coding[6] = channel->get_data_bits();

  ESP_LOGD(TAG, "PL2303: SET_LINE_REQUEST baud=%" PRIu32 " stop=%u parity=%u data=%u", baud, line_coding[4],
           line_coding[5], line_coding[6]);

  std::vector<uint8_t> lc_vec(line_coding, line_coding + 7);
  uint16_t iface = channel->cdc_dev_.bulk_interface_number;
  this->control_transfer(SET_LINE_REQUEST_TYPE, SET_LINE_REQUEST, 0, iface, nop_cb, lc_vec);

  // Assert DTR + RTS
  this->control_transfer(SET_CONTROL_REQUEST_TYPE, SET_CONTROL_REQUEST, CONTROL_DTR | CONTROL_RTS, iface, nop_cb);

  this->start_channels_();
}

}  // namespace esphome::usb_uart
#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
