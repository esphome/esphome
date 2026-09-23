#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/uart/uart_debugger.h"

#include "esphome/components/bytebuffer/bytebuffer.h"
#include <cinttypes>

namespace esphome::usb_uart {

using namespace bytebuffer;

// FTDI chip family identifiers. These map to USB device bcdDevice values
// and determine how baudrate divisors and clock sources are calculated.
enum FtdiChipType {
  TYPE_AM = 0,
  TYPE_BM = 1,
  TYPE_2232C = 2,
  TYPE_R = 3,
  TYPE_2232H = 4,
  TYPE_4232H = 5,
  TYPE_232H = 6,
  TYPE_230X = 7,
};

static int ftdi_to_clkbits_am(int baudrate, uint32_t *encoded_divisor) {
  static const char FRAC_CODE[8] = {0, 3, 2, 4, 1, 5, 6, 7};
  static const char AM_ADJUST_UP[8] = {0, 0, 0, 1, 0, 3, 2, 1};
  static const char AM_ADJUST_DN[8] = {0, 0, 0, 1, 0, 1, 2, 3};
  int divisor, best_divisor, best_baud, best_baud_diff;
  int i;
  divisor = 24000000 / baudrate;

  divisor -= AM_ADJUST_DN[divisor & 7];

  best_divisor = 0;
  best_baud = 0;
  best_baud_diff = 0;
  for (i = 0; i < 2; i++) {
    int try_divisor = divisor + i;
    int baud_estimate;
    int baud_diff;

    if (try_divisor <= 8) {
      try_divisor = 8;
    } else if (divisor < 16) {
      try_divisor = 16;
    } else {
      try_divisor += AM_ADJUST_UP[try_divisor & 7];
      if (try_divisor > 0x1FFF8) {
        // Round down to maximum supported divisor value (for AM)
        try_divisor = 0x1FFF8;
      }
    }
    baud_estimate = (24000000 + (try_divisor / 2)) / try_divisor;
    if (baud_estimate < baudrate) {
      baud_diff = baudrate - baud_estimate;
    } else {
      baud_diff = baud_estimate - baudrate;
    }
    if (i == 0 || baud_diff < best_baud_diff) {
      best_divisor = try_divisor;
      best_baud = baud_estimate;
      best_baud_diff = baud_diff;
      if (baud_diff == 0) {
        break;
      }
    }
  }
  *encoded_divisor = (best_divisor >> 3) | (FRAC_CODE[best_divisor & 7] << 14);
  if (*encoded_divisor == 1) {
    *encoded_divisor = 0;  // 3000000 baud
  } else if (*encoded_divisor == 0x4001) {
    *encoded_divisor = 1;  // 2000000 baud (BM only)
  }
  return best_baud;
}

static int ftdi_to_clkbits(int baudrate, unsigned int clk, int clk_div, uint32_t *encoded_divisor) {
  static const char FRAC_CODE[8] = {0, 3, 2, 4, 1, 5, 6, 7};
  int best_baud = 0;
  int divisor, best_divisor;
  if (baudrate >= clk / clk_div) {
    *encoded_divisor = 0;
    best_baud = clk / clk_div;
  } else if (baudrate >= clk / (clk_div + clk_div / 2)) {
    *encoded_divisor = 1;
    best_baud = clk / (clk_div + clk_div / 2);
  } else if (baudrate >= clk / (2 * clk_div)) {
    *encoded_divisor = 2;
    best_baud = clk / (2 * clk_div);
  } else {
    divisor = clk * 16 / clk_div / baudrate;
    if (divisor & 1) {
      best_divisor = divisor / 2 + 1;
    } else {
      best_divisor = divisor / 2;
    }
    if (best_divisor > 0x20000)
      best_divisor = 0x1ffff;
    best_baud = clk * 16 / clk_div / best_divisor;
    if (best_baud & 1) {
      best_baud = best_baud / 2 + 1;
    } else {
      best_baud = best_baud / 2;
    }
    *encoded_divisor = (best_divisor >> 3) | (FRAC_CODE[best_divisor & 0x7] << 14);
  }
  return best_baud;
}

struct FtdiConfig {
  uint16_t value;
  uint16_t ftdi_index;
  int best_baud;
};

static FtdiConfig ftdi_convert_baudrate(int baudrate, uint8_t chip_type, uint8_t channel_index) {
  uint32_t encoded_divisor;

  FtdiConfig config{};

  if (baudrate <= 0) {
    return config;
  }

  static constexpr uint32_t H_CLK = 120000000;
  static constexpr uint32_t C_CLK = 48000000;
  if ((chip_type == TYPE_2232H) || (chip_type == TYPE_4232H) || (chip_type == TYPE_232H)) {
    if (baudrate * 10 > H_CLK / 0x3fff) {
      config.best_baud = ftdi_to_clkbits(baudrate, H_CLK, 10, &encoded_divisor);
      encoded_divisor |= 0x20000; /* switch on CLK/10*/
    } else {
      config.best_baud = ftdi_to_clkbits(baudrate, C_CLK, 16, &encoded_divisor);
    }
  } else if ((chip_type == TYPE_BM) || (chip_type == TYPE_2232C) || (chip_type == TYPE_R) || (chip_type == TYPE_230X)) {
    config.best_baud = ftdi_to_clkbits(baudrate, C_CLK, 16, &encoded_divisor);
  } else {
    config.best_baud = ftdi_to_clkbits_am(baudrate, &encoded_divisor);
  }

  config.value = (uint16_t) (encoded_divisor & 0xFFFF);
  if (chip_type == TYPE_2232H || chip_type == TYPE_4232H || chip_type == TYPE_232H) {
    config.ftdi_index = (uint16_t) (encoded_divisor >> 8);
    config.ftdi_index &= 0xFF00;
    config.ftdi_index |= (channel_index + 1);
  } else {
    config.ftdi_index = (uint16_t) (encoded_divisor >> 16);
  }

  return config;
}

static optional<CdcEps> get_uart(const usb_config_desc_t *config_desc, uint8_t intf_idx) {
  int conf_offset, ep_offset;
  CdcEps eps{};

  const auto *intf_desc = usb_parse_interface_descriptor(config_desc, intf_idx, 0, &conf_offset);
  if (!intf_desc) {
    ESP_LOGD(TAG, "usb_parse_interface_descriptor failed for intf_idx=%d (end of interfaces)", intf_idx);
    return nullopt;
  }
  ESP_LOGD(TAG,
           "intf_desc [idx=%d]: bInterfaceClass=%02X, bInterfaceSubClass=%02X, bInterfaceProtocol=%02X, "
           "bNumEndpoints=%d, bInterfaceNumber=%d",
           intf_idx, intf_desc->bInterfaceClass, intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol,
           intf_desc->bNumEndpoints, intf_desc->bInterfaceNumber);

  std::vector<const usb_ep_desc_t *> endpoints;
  for (uint8_t i = 0; i != intf_desc->bNumEndpoints; i++) {
    ep_offset = conf_offset;
    const auto *ep = usb_parse_endpoint_descriptor_by_index(intf_desc, i, config_desc->wTotalLength, &ep_offset);
    if (!ep) {
      ESP_LOGE(TAG, "Ran out of endpoints at %d before finding all %d endpoints", i, intf_desc->bNumEndpoints);
      return nullopt;
    }
    ESP_LOGD(TAG, "ep: bEndpointAddress=%02X, bmAttributes=%02X", ep->bEndpointAddress, ep->bmAttributes);

    if (ep->bmAttributes != 0x2) {
      ESP_LOGD(TAG, "Skipping non-bulk endpoint: %02X", ep->bEndpointAddress);
      continue;
    }
    endpoints.push_back(ep);
  }

  const usb_ep_desc_t *ep1 = nullptr;
  const usb_ep_desc_t *ep2 = nullptr;
  for (const auto *ep : endpoints) {
    if (ep1 == nullptr) {
      ep1 = ep;
    } else if (ep2 == nullptr) {
      ep2 = ep;
      break;
    }
  }

  if (ep1 == nullptr || ep2 == nullptr) {
    ESP_LOGD(TAG, "Interface %d has %zu endpoints (need 2 bulk endpoints)", intf_idx, endpoints.size());
    return nullopt;
  }

  ESP_LOGD(TAG, "Interface %d: ep1=0x%02X, ep2=0x%02X", intf_idx, ep1->bEndpointAddress, ep2->bEndpointAddress);

  if (ep1->bEndpointAddress & usb_host::USB_DIR_IN) {
    eps.in_ep = ep1;
    eps.out_ep = ep2;
    ESP_LOGD(TAG, "ep1 is IN (RX): ep1=0x%02X (in_ep), ep2=0x%02X (out_ep)", ep1->bEndpointAddress,
             ep2->bEndpointAddress);
  } else {
    eps.out_ep = ep1;
    eps.in_ep = ep2;
    ESP_LOGD(TAG, "ep1 is OUT (TX): ep1=0x%02X (out_ep), ep2=0x%02X (in_ep)", ep1->bEndpointAddress,
             ep2->bEndpointAddress);
  }

  eps.bulk_interface_number = intf_desc->bInterfaceNumber;
  return eps;
}

std::vector<CdcEps> USBUartTypeFT23XX::parse_descriptors(usb_device_handle_t dev_hdl) {
  const usb_config_desc_t *config_desc;
  const usb_device_desc_t *device_desc;
  std::vector<CdcEps> cdc_devs{};
  std::string type_string;

  if (usb_host_get_device_descriptor(dev_hdl, &device_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_device_descriptor failed");
    return {};
  }
  if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_active_config_descriptor failed");
    return {};
  }
  if (device_desc->bcdDevice == 0x400 || (device_desc->bcdDevice == 0x200 && device_desc->iSerialNumber == 0)) {
    this->chip_type_ = TYPE_BM;
    type_string = "BM type chip";
  } else if (device_desc->bcdDevice == 0x200) {
    this->chip_type_ = TYPE_AM;
    type_string = "AM type chip";
  } else if (device_desc->bcdDevice == 0x500) {
    this->chip_type_ = TYPE_2232C;
    type_string = "2232C chip";
  } else if (device_desc->bcdDevice == 0x600) {
    this->chip_type_ = TYPE_R;
    type_string = "type R chip";
  } else if (device_desc->bcdDevice == 0x700) {
    this->chip_type_ = TYPE_2232H;
    type_string = "2232H chip";
  } else if (device_desc->bcdDevice == 0x800) {
    this->chip_type_ = TYPE_4232H;
    type_string = "4232H chip";
  } else if (device_desc->bcdDevice == 0x900) {
    this->chip_type_ = TYPE_232H;
    type_string = "232H type chip";
  } else if (device_desc->bcdDevice == 0x1000) {
    this->chip_type_ = TYPE_230X;
    type_string = "230x chip";
  }

  ESP_LOGD(TAG, "Found FTDI %s based device", type_string.c_str());
  for (size_t intf_idx = 0; intf_idx < this->channels_.size(); intf_idx++) {
    if (auto eps = get_uart(config_desc, static_cast<uint8_t>(intf_idx))) {
      cdc_devs.push_back(*eps);
      ESP_LOGD(TAG, "Found CDC interface at USB interface index %zu", intf_idx);
    }
  }
  return cdc_devs;
}

void USBUartTypeFT23XX::start_input(USBUartChannel *channel) {
  if (!channel->initialised_.load())
    return;

  // Use compare_exchange_strong to avoid a check-then-act race: start_input() is called
  // from both the USB task (self-restart on success) and the main loop (backpressure
  // restart), so a plain load()/store() pair can let both threads submit a transfer.
  auto started = false;
  if (!channel->input_started_.compare_exchange_strong(started, true))
    return;

  const auto *ep = channel->cdc_dev_.in_ep;

  auto callback = [this, channel](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "RX Transfer failed, status=%s", esp_err_to_name(status.error_code));
      channel->input_started_.store(false);
      return;
    }

    // FTDI prepends a 2-byte modem/line status header to every bulk IN packet.
    size_t uart_data_len = (status.data_len > 2) ? (status.data_len - 2) : 0;

    if (uart_data_len > 0) {
      ESP_LOGV(TAG, "RX callback: Received %zu bytes, channel=%d", uart_data_len, channel->index_);
      if (!channel->dummy_receiver_) {
        UsbDataChunk *chunk = this->chunk_pool_.allocate();
        if (chunk == nullptr) {
          this->usb_data_queue_.increment_dropped_count();
          channel->input_started_.store(false);
          // Queue is full — wake the main loop to drain it, then let read_array()
          // retrigger start_input() rather than spinning here in the USB task.
          this->enable_loop_soon_any_context();
          App.wake_loop_threadsafe();
          return;
        }
        // Strip the 2-byte FTDI header before queuing.
        memcpy(chunk->data, status.data + 2, uart_data_len);
        chunk->length = static_cast<uint16_t>(uart_data_len);
        chunk->channel = channel;
        this->usb_data_queue_.push(chunk);
#ifdef USE_UART_DEBUGGER
        if (channel->debug_) {
          uart::UARTDebug::log_hex(uart::UART_DIRECTION_RX,
                                   std::vector<uint8_t>(status.data + 2, status.data + 2 + uart_data_len), ',',
                                   channel->debug_prefix_);
        }
#endif
        this->enable_loop_soon_any_context();
        App.wake_loop_threadsafe();
      }
    } else if (status.data_len >= 2) {
      ESP_LOGVV(TAG, "RX: Status packet, modem=0x%02X line=0x%02X, ch=%d", status.data[0], status.data[1],
                channel->index_);
    }

    channel->input_started_.store(false);
    this->start_input(channel);
  };

  if (!this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize)) {
    ESP_LOGE(TAG, "RX transfer submission failed for ep=0x%02X", ep->bEndpointAddress);
    channel->input_started_.store(false);
  }
}

void USBUartTypeFT23XX::on_rx_overflow(USBUartChannel *channel) {
  ESP_LOGW(TAG, "RX buffer overflow on channel %d, clearing to resync", channel->index_);
  channel->input_buffer_.clear();
}

bool USBUartTypeFT23XX::config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok,
                                    const uint8_t *response) {
  // On reload (settings change on an open channel) skip the SIO reset; the FTDI set_termios
  // path only re-applies baud + line properties and does not re-assert DTR/RTS.
  if (reload)
    step++;
  switch (step) {
    case 0:  // SIO reset (init only)
      this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, 0x00, 0x00,
                             channel->cdc_dev_.bulk_interface_number + 1);
      return true;
    case 1: {  // set baudrate
      auto config = ftdi_convert_baudrate(channel->baud_rate_, this->chip_type_, channel->index_);
      uint16_t usb_index = (config.ftdi_index & 0xFF00) | (channel->cdc_dev_.bulk_interface_number + 1);
      ESP_LOGD(TAG, "Baudrate: %u, value=0x%04X, ftdi_index=0x%04X", (unsigned) channel->baud_rate_, config.value,
               config.ftdi_index);
      this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, 0x03, config.value, usb_index);
      return true;
    }
    case 2: {  // set line properties (data bits / parity / stop bits)
      uint16_t value = channel->data_bits_;
      switch (channel->parity_) {
        case UART_CONFIG_PARITY_NONE:
          value |= (0x00 << 8);
          break;
        case UART_CONFIG_PARITY_ODD:
          value |= (0x01 << 8);
          break;
        case UART_CONFIG_PARITY_EVEN:
          value |= (0x02 << 8);
          break;
        case UART_CONFIG_PARITY_MARK:
          value |= (0x03 << 8);
          break;
        case UART_CONFIG_PARITY_SPACE:
          value |= (0x04 << 8);
          break;
      }
      switch (channel->stop_bits_) {
        default:  // 1 bit
          value |= (0x00 << 11);
          break;
        case UART_CONFIG_STOP_BITS_1_5:
          value |= (0x01 << 11);
          break;
        case UART_CONFIG_STOP_BITS_2:
          value |= (0x02 << 11);
          break;
      }
      value |= (0x00 << 14);
      this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, 0x04, value,
                             channel->cdc_dev_.bulk_interface_number + 1);
      return true;
    }
    case 3:  // set modem control DTR+RTS (init only)
      if (reload)
        return false;
      this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, 0x01, 0x0000,
                             channel->cdc_dev_.bulk_interface_number + 1);
      return true;
    default:
      return false;
  }
}

}  // namespace esphome::usb_uart
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
