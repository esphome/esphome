#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"

#include "esphome/components/bytebuffer/bytebuffer.h"

#include <cinttypes>
#include <iterator>

namespace esphome::usb_uart {

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

// VENDOR_SPECIFIC wValue reads
static constexpr uint16_t GET_FW_VER_2N = 0x0010;
static constexpr uint16_t GET_PARTNUM = 0x370B;

static constexpr uint8_t PARTNUM_CP2102N_QFN28 = 0x20;
static constexpr uint8_t PARTNUM_CP2102N_QFN24 = 0x21;
static constexpr uint8_t PARTNUM_CP2102N_QFN20 = 0x22;

// CP2102N firmware up to 1.0.4 reads xon_limit as flow_replace (erratum E104)
static constexpr uint32_t CP2102N_LAST_FW_WITHOUT_FLOW_CONTROL = 0x010004;

// SET_MHS value bits. The high byte selects which lines the low byte changes.
static constexpr uint16_t CONTROL_DTR = 0x0001;
static constexpr uint16_t CONTROL_RTS = 0x0002;
static constexpr uint16_t CONTROL_WRITE_DTR = 0x0100;
static constexpr uint16_t CONTROL_WRITE_RTS = 0x0200;

// CP210xFlowBlock::control_handshake bits
static constexpr uint32_t SERIAL_DTR_MASK = 0x03;
static constexpr uint32_t SERIAL_DTR_INACTIVE = 0x00;
static constexpr uint32_t SERIAL_DTR_ACTIVE = 0x01;
static constexpr uint32_t SERIAL_CTS_HANDSHAKE = 1 << 3;
static constexpr uint32_t SERIAL_DSR_HANDSHAKE = 1 << 4;
static constexpr uint32_t SERIAL_DCD_HANDSHAKE = 1 << 5;
static constexpr uint32_t SERIAL_DSR_SENSITIVITY = 1 << 6;

// CP210xFlowBlock::flow_replace bits
static constexpr uint32_t SERIAL_AUTO_TRANSMIT = 1 << 0;
static constexpr uint32_t SERIAL_AUTO_RECEIVE = 1 << 1;
static constexpr uint32_t SERIAL_RTS_MASK = 0xC0;
static constexpr uint32_t SERIAL_RTS_INACTIVE = 0x00;
static constexpr uint32_t SERIAL_RTS_ACTIVE = 0x40;
static constexpr uint32_t SERIAL_RTS_FLOW_CTL = 0x80;

// Receive queue levels at which the chip sends XON and XOFF
static constexpr uint32_t XON_XOFF_LIMIT = 128;
// SET_CHARS block: EOF, error, break, event, XON and XOFF characters. XON and XOFF are the
// termios defaults DC1 and DC3.
static constexpr uint8_t SPECIAL_CHARS[] = {0, 0, 0, 0, 0x11, 0x13};

// Put DTR and RTS into the block. Under RTS/CTS an asserted RTS hands the line to the chip.
static void set_block_modem_lines(CP210xFlowBlock &block, bool dtr, bool rts) {
  block.control_handshake =
      (block.control_handshake & ~SERIAL_DTR_MASK) | (dtr ? SERIAL_DTR_ACTIVE : SERIAL_DTR_INACTIVE);
  uint32_t rts_bits = SERIAL_RTS_INACTIVE;
  if (rts)
    rts_bits = (block.control_handshake & SERIAL_CTS_HANDSHAKE) ? SERIAL_RTS_FLOW_CTL : SERIAL_RTS_ACTIVE;
  block.flow_replace = (block.flow_replace & ~SERIAL_RTS_MASK) | rts_bits;
}

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
  ESP_LOGD(TAG, "bDeviceClass: %u, bDeviceSubClass: %u, bNumInterfaces: %u", device_desc->bDeviceClass,
           device_desc->bDeviceSubClass, config_desc->bNumInterfaces);
  if (device_desc->bDeviceClass != 0) {
    ESP_LOGE(TAG, "bDeviceClass != 0");
    return {};
  }

  for (uint8_t i = 0; i != config_desc->bNumInterfaces; i++) {
    const auto *data_desc = usb_parse_interface_descriptor(config_desc, i, 0, &conf_offset);
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
    const auto *out_ep = usb_parse_endpoint_descriptor_by_index(data_desc, 0, config_desc->wTotalLength, &ep_offset);
    if (!out_ep) {
      ESP_LOGE(TAG, "out_ep: usb_parse_endpoint_descriptor_by_index failed");
      continue;
    }
    ep_offset = conf_offset;
    const auto *in_ep = usb_parse_endpoint_descriptor_by_index(data_desc, 1, config_desc->wTotalLength, &ep_offset);
    if (!in_ep) {
      ESP_LOGE(TAG, "in_ep: usb_parse_endpoint_descriptor_by_index failed");
      continue;
    }
    CdcEps eps = in_ep->bEndpointAddress & usb_host::USB_DIR_IN
                     ? CdcEps{nullptr, in_ep, out_ep, data_desc->bInterfaceNumber}
                     : CdcEps{nullptr, out_ep, in_ep, data_desc->bInterfaceNumber};
    eps.bulk_interface_class = data_desc->bInterfaceClass;
    cdc_devs.push_back(eps);
  }
  return cdc_devs;
}

bool USBUartTypeCP210X::config_device_step(uint8_t step, bool ok, const uint8_t *response) {
  switch (step) {
    case 0:
      // A fresh device: the previous one's cached blocks must not steer modem_control_transfer()
      this->no_flow_control_ = false;
      this->flow_blocks_.fill({});
      this->config_read_(USB_VENDOR_DEV | usb_host::USB_DIR_IN, VENDOR_SPECIFIC, GET_PARTNUM, 0, this->cfg_response_,
                         1);
      return true;
    case 1:
      if (!ok)
        return false;
      ESP_LOGD(TAG, "Part number 0x%02X", response[0]);
      if (response[0] != PARTNUM_CP2102N_QFN28 && response[0] != PARTNUM_CP2102N_QFN24 &&
          response[0] != PARTNUM_CP2102N_QFN20)
        return false;
      this->config_read_(USB_VENDOR_DEV | usb_host::USB_DIR_IN, VENDOR_SPECIFIC, GET_FW_VER_2N, 0, this->cfg_response_,
                         3);
      return true;
    case 2: {
      if (!ok)
        return false;
      const uint32_t version = (response[0] << 16) | (response[1] << 8) | response[2];
      ESP_LOGD(TAG, "CP2102N firmware %u.%u.%u", response[0], response[1], response[2]);
      this->no_flow_control_ = version <= CP2102N_LAST_FW_WITHOUT_FLOW_CONTROL;
      if (this->no_flow_control_) {
        ESP_LOGW(TAG, "CP2102N firmware 1.0.4 and older cannot do flow control (erratum E104), using none");
      }
      return false;
    }
    default:
      return false;
  }
}

bool USBUartTypeCP210X::config_step(USBUartChannelBase *channel, uint8_t step, bool reload, bool ok,
                                    const uint8_t *response) {
  // On reload, skip the one-time IFC_ENABLE step (the interface is already enabled).
  if (reload)
    step++;
  const bool xon_xoff = channel->flow_control_ == uart::UART_FLOW_CONTROL_XON_XOFF;
  if (step >= 3) {
    if (this->no_flow_control_) {
      // Firmware with erratum E104 takes flow_replace from xon_limit, so a SET_FLOW can drop RTS.
      // Skip the flow control steps; the lines then stay on SET_MHS as the cached block has no CTS.
      step += 3;
    } else if (!xon_xoff) {
      // SET_CHARS is only needed for XON/XOFF
      step++;
    }
  }
  switch (step) {
    case 0:
      this->config_transfer_(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, IFC_ENABLE, 1, channel->index_);
      return true;
    case 1: {
      uint16_t line_control = static_cast<uint8_t>(channel->line_stop_bits_());
      line_control |= static_cast<uint8_t>(channel->line_parity_()) << 4;
      line_control |= channel->data_bits_ << 8;
      ESP_LOGD(TAG, "Line control value 0x%X", line_control);
      this->config_transfer_(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, SET_LINE_CTL, line_control, channel->index_);
      return true;
    }
    case 2: {
      auto baud = ByteBuffer::wrap(channel->baud_rate_, LITTLE);
      this->config_transfer_(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, SET_BAUDRATE, 0, channel->index_, baud.get_data());
      return true;
    }
    case 3:
      this->config_transfer_(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, SET_CHARS, 0, channel->index_,
                             std::vector<uint8_t>(std::begin(SPECIAL_CHARS), std::end(SPECIAL_CHARS)));
      return true;
    case 4: {
      // Read into the channel's cached block, which the next step edits and writes back
      this->config_read_(USB_VENDOR_IFC | usb_host::USB_DIR_IN, GET_FLOW, 0, channel->index_,
                         reinterpret_cast<uint8_t *>(&this->flow_blocks_[channel->index_]), sizeof(CP210xFlowBlock));
      return true;
    }
    case 5: {
      // Set flow control, re-applied on every settings change. The block also carries DTR and RTS,
      // so this puts them on the wire too.
      auto &block = this->flow_blocks_[channel->index_];
      block.control_handshake &=
          ~(SERIAL_DSR_HANDSHAKE | SERIAL_DCD_HANDSHAKE | SERIAL_DSR_SENSITIVITY | SERIAL_CTS_HANDSHAKE);
      if (channel->flow_control_ == uart::UART_FLOW_CONTROL_RTS_CTS)
        block.control_handshake |= SERIAL_CTS_HANDSHAKE;
      if (xon_xoff) {
        block.flow_replace |= SERIAL_AUTO_TRANSMIT | SERIAL_AUTO_RECEIVE;
        block.xon_limit = XON_XOFF_LIMIT;
        block.xoff_limit = XON_XOFF_LIMIT;
      } else {
        block.flow_replace &= ~(SERIAL_AUTO_TRANSMIT | SERIAL_AUTO_RECEIVE);
      }
      set_block_modem_lines(block, channel->dtr_, channel->rts_);
      this->set_flow_transfer_(channel);
      return true;
    }
    case 6:
      // Raise the lines on open (init only)
      if (reload)
        return false;
      this->modem_control_transfer(channel);
      return true;
    default:
      return false;
  }
}

void USBUartTypeCP210X::modem_control_transfer(USBUartChannelBase *channel) {
  // Under RTS/CTS the chip drives RTS, so the lines go through SET_FLOW
  auto &block = this->flow_blocks_[channel->index_];
  if (block.control_handshake & SERIAL_CTS_HANDSHAKE) {
    set_block_modem_lines(block, channel->dtr_, channel->rts_);
    this->set_flow_transfer_(channel);
    return;
  }
  const uint16_t value =
      CONTROL_WRITE_DTR | CONTROL_WRITE_RTS | (channel->dtr_ ? CONTROL_DTR : 0) | (channel->rts_ ? CONTROL_RTS : 0);
  ESP_LOGD(TAG, "SET_MHS: DTR=%s RTS=%s", ONOFF(channel->dtr_), ONOFF(channel->rts_));
  this->config_transfer_(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, SET_MHS, value, channel->index_);
}

void USBUartTypeCP210X::set_flow_transfer_(USBUartChannelBase *channel) {
  const auto &block = this->flow_blocks_[channel->index_];
  ESP_LOGD(TAG, "SET_FLOW: control handshake 0x%02" PRIX32 ", flow replace 0x%02" PRIX32, block.control_handshake,
           block.flow_replace);
  const auto *bytes = reinterpret_cast<const uint8_t *>(&block);
  this->config_transfer_(USB_VENDOR_IFC | usb_host::USB_DIR_OUT, SET_FLOW, 0, channel->index_,
                         std::vector<uint8_t>(bytes, bytes + sizeof(block)));
}
}  // namespace esphome::usb_uart

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
