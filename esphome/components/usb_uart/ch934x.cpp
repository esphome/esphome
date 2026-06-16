#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/string_ref.h"
#include "esp_random.h"

namespace esphome::usb_uart {

static constexpr uint8_t CMD_W_R = 0xC0;
static constexpr uint8_t CMD_W_BR = 0x80;
static constexpr uint8_t R_C1 = 0x01;
static constexpr uint8_t R_C2 = 0x02;
static constexpr uint8_t R_C3 = 0x03;
static constexpr uint8_t R_C4 = 0x04;
static constexpr uint8_t R_INIT = 0xA1;

static constexpr size_t RX_BLOCK_SIZE = 32;
static constexpr size_t RX_HEADER_SIZE = 2;
static constexpr size_t RX_MAX_DATA = 30;

static StringRef get_chiptype_string(uint8_t enum_value) {
  switch (enum_value) {
    case CHIP_CH9344L:
      return StringRef::from_lit("CH9344L");
    case CHIP_CH9344Q:
      return StringRef::from_lit("CH9344Q");
    case CHIP_CH348L:
      return StringRef::from_lit("CH348L");
    case CHIP_CH348Q:
      return StringRef::from_lit("CH348Q");
    default:
      return StringRef::from_lit("unknown");
  }
}

static optional<Ch934xEps> get_uart_hub(const usb_config_desc_t *config_desc, uint8_t intf_idx) {
  int conf_offset, ep_offset;
  Ch934xEps eps{};
  eps.data_interface = intf_idx;

  const auto *intf_desc = usb_parse_interface_descriptor(config_desc, intf_idx, 0, &conf_offset);
  if (!intf_desc) {
    ESP_LOGE(TAG, "usb_parse_interface_descriptor failed");
    return nullopt;
  }

  ESP_LOGD(TAG, "intf_desc: bInterfaceClass=%02X, bInterfaceSubClass=%02X, bInterfaceProtocol=%02X, bNumEndpoints=%d",
           intf_desc->bInterfaceClass, intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol,
           intf_desc->bNumEndpoints);

  for (uint8_t i = 0; i < intf_desc->bNumEndpoints; i++) {
    ep_offset = conf_offset;
    const auto *ep = usb_parse_endpoint_descriptor_by_index(intf_desc, i, config_desc->wTotalLength, &ep_offset);
    if (!ep) {
      ESP_LOGE(TAG, "Ran out of endpoints before finding all required endpoints");
      return nullopt;
    }

    ESP_LOGD(TAG, "ep[%d]: bEndpointAddress=%02X, bmAttributes=%02X", i, ep->bEndpointAddress, ep->bmAttributes);

    switch (i) {
      case 0:
      case 1:
        if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK && (ep->bEndpointAddress & usb_host::USB_DIR_IN)) {
          eps.in_ep = ep;
        } else if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK) {
          eps.out_ep = ep;
        }
        break;
      case 2:
      case 3:
        if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK && (ep->bEndpointAddress & usb_host::USB_DIR_IN)) {
          eps.ep_cmd_read = ep;
        } else if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK) {
          eps.ep_cmd_write = ep;
        }
        break;
      default:
        ESP_LOGE(TAG, "Unexpected 5th endpoint detected");
        return nullopt;
    }
  }

  if (eps.in_ep != nullptr && eps.out_ep != nullptr && eps.ep_cmd_read != nullptr && eps.ep_cmd_write != nullptr) {
    return eps;
  }

  ESP_LOGE(TAG, "Failed to find all required endpoints");
  return nullopt;
}

uint8_t USBUartTypeCH934X::get_reg_address_(uint8_t portnum) {
  uint8_t rgadd = 0;
  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    rgadd = 0x10 * portnum + 0x08;
  } else if (this->chiptype_ == CHIP_CH348L || this->chiptype_ == CHIP_CH348Q) {
    rgadd = (portnum < 4) ? 0x10 * portnum : 0x10 * (portnum - 4) + 0x08;
  }
  return rgadd;
}

bool USBUartTypeCH934X::config_device_step(uint8_t step, bool ok, const uint8_t *response) {
  if (step == 0) {
    ESP_LOGD(TAG, "Starting device setup");
    // Fetch the chip id. NOTE: this preserves the original request exactly, including the
    // USB_DIR_OUT direction with no data stage, so the parsing below inspects the setup
    // packet bytes rather than a device IN response. This looks like a latent bug (a vendor
    // IN read with a buffer was likely intended) and is left unchanged to preserve behaviour.
    this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, 0x96, 0, 0);
    return true;
  }

  // step 1: identify the chip and run one-time device + per-channel register setup. The
  // CH934x configures via fire-and-forget bulk writes, so there is nothing to wait on.
  if (!ok) {
    ESP_LOGE(TAG, "Fetching chip id failed");
    return false;
  }

  ESP_LOGV(TAG, "Received chip id response bytes 0x%02x 0x%02x 0x%02x 0x%02x", response[0], response[1], response[2],
           response[3]);

  if (this->pid_ == 0xE018) {
    this->chiptype_ = (response[0] >= 0x40) ? CHIP_CH9344Q : CHIP_CH9344L;
    this->num_ports_ = 4;
    this->port_offset_ = 4;
  } else if (this->pid_ == 0x55D9) {
    this->chiptype_ = (response[1] & (0x02 << 6)) ? CHIP_CH348Q : CHIP_CH348L;
    this->num_ports_ = 8;
    this->port_offset_ = 0;
  } else {
    ESP_LOGE(TAG, "Unknown product_id: 0x%04X", this->pid_);
    return false;
  }

  ESP_LOGI(TAG, "Found chip type %s with %u ports", get_chiptype_string(this->chiptype_).c_str(), this->num_ports_);
  ESP_LOGD(TAG, "Configuring device registers for %u ports", this->num_ports_);

  usb_host::transfer_cb_t bulk_callback = [=](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGE(TAG, "Bulk transfer failed, status=%s", esp_err_to_name(status.error_code));
  };

  uint8_t portnum = this->num_ports_ - 1;
  uint8_t rgadd = this->get_reg_address_(portnum);
  uint8_t buffer[6];

  buffer[0] = CMD_W_R;
  buffer[1] = rgadd + R_C2;
  buffer[2] = 0x87;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, bulk_callback, buffer, 3);

  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    buffer[1] = rgadd + R_C3;
    buffer[2] = 0x03;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, bulk_callback, buffer, 3);
  }

  buffer[1] = rgadd + R_C3;
  buffer[2] = 0x08;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, bulk_callback, buffer, 3);

  for (auto *channel : this->channels_) {
    if (channel->index_ >= this->num_ports_) {
      ESP_LOGW(TAG, "Channel %d exceeds number of available ports (%d)", channel->index_, this->num_ports_);
      continue;
    }
    if (this->configure_channel_(channel)) {
      channel->initialised_.store(true);
    } else {
      ESP_LOGE(TAG, "Failed to initialize channel %d", channel->index_);
    }
  }

  // Point every channel's shared TX queue to channel 0, and wire up the shared
  // data endpoint on channel 0 so start_output() can use it directly.
  // Pre-compute each channel's TX port byte (port_offset + index) to avoid
  // casting to USBUartTypeCH934X on every write_array call.
  auto *shared = this->channels_[0];
  shared->cdc_dev_.out_ep = this->uart_host_dev_.out_ep;
  for (auto *channel : this->channels_) {
    auto *ch934x_channel = static_cast<CH934XChannel *>(channel);
    ch934x_channel->tx_shared_channel_ = shared;
    ch934x_channel->tx_port_byte_ = static_cast<uint8_t>(this->port_offset_ + channel->index_);
  }

  this->start_rx_reader_();
  this->start_command_reader_();
  return false;
}

// Per-channel settings. On full init every channel is already configured by
// config_device_step(); this is used by load_settings() to re-apply UART parameters
// (baud/parity/stop/data) to an already-open channel.
bool USBUartTypeCH934X::config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok,
                                    const uint8_t *response) {
  if (!reload || channel->index_ >= this->num_ports_)
    return false;

  this->configure_uart_parameters_(channel);

  // Re-send the post-parameter register writes that configure_channel_() issues after
  // configure_uart_parameters_() during full init. Without these, the device does not
  // apply the new settings on a runtime reload:
  //   - R_C1 | 0x07 (CMD_W_R): re-assert UART enable / control lines
  //   - R_C4 | 0x00 then R_C4 | 0x10 (CMD_W_BR, CH9344 only): commit the new baud config
  uint8_t portnum = channel->index_;
  uint8_t rgadd = this->get_reg_address_(portnum);
  uint8_t buffer[3];

  usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGE(TAG, "Reload post-param write failed: %s", esp_err_to_name(status.error_code));
  };

  buffer[0] = CMD_W_R;
  buffer[1] = rgadd + R_C1;
  buffer[2] = 0x07;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    buffer[0] = CMD_W_BR;
    buffer[1] = rgadd + R_C4;
    buffer[2] = 0x00;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

    buffer[2] = 0x10;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);
  }

  return false;
}

bool USBUartTypeCH934X::configure_channel_(USBUartChannel *channel) {
  if (!this->set_uart_mode_(channel)) {
    ESP_LOGE(TAG, "Failed to set UART mode for channel %d", channel->index_);
    return false;
  }
  if (!this->configure_uart_parameters_(channel)) {
    ESP_LOGE(TAG, "Failed to configure UART parameters for channel %d", channel->index_);
    return false;
  }

  uint8_t portnum = channel->index_;
  uint8_t rgadd = this->get_reg_address_(portnum);
  uint8_t buffer[3];

  usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGE(TAG, "Control line setup failed, status=%s", esp_err_to_name(status.error_code));
  };

  buffer[0] = CMD_W_R;
  buffer[1] = rgadd + R_C1;
  buffer[2] = 0x07;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  buffer[0] = CMD_W_BR;
  buffer[1] = rgadd + R_C4;
  buffer[2] = 0x00;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    buffer[0] = CMD_W_BR;
    buffer[1] = rgadd + R_C4;
    buffer[2] = 0x10;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);
  }

  return true;
}

bool USBUartTypeCH934X::set_uart_mode_(USBUartChannel *channel) {
  uint8_t portnum = channel->index_;
  uint8_t rgadd = this->get_reg_address_(portnum);
  uint8_t buffer[3];

  usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGE(TAG, "Bulk transfer failed, status=%s", esp_err_to_name(status.error_code));
  };

  buffer[0] = CMD_W_BR;
  buffer[1] = rgadd + R_C4;
  buffer[2] = 0x50;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  if (this->chiptype_ == CHIP_CH348L || this->chiptype_ == CHIP_CH348Q)
    return true;

  buffer[0] = CMD_W_BR;
  buffer[1] = 0x97;
  buffer[2] = 0x00;  // normal UART mode for all ports
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  return true;
}

static void cal_outdata(uint8_t *buffer, uint8_t rol, uint8_t xor_val) {
  for (uint8_t i = 0; i < rol; i++) {
    uint8_t en_status = buffer[0];
    buffer[0] = (buffer[0] << 1) | ((buffer[1] & 0x80) ? 1 : 0);
    buffer[1] = (buffer[1] << 1) | ((buffer[2] & 0x80) ? 1 : 0);
    buffer[2] = (buffer[2] << 1) | ((buffer[3] & 0x80) ? 1 : 0);
    buffer[3] = (buffer[3] << 1) | ((buffer[4] & 0x80) ? 1 : 0);
    buffer[4] = (buffer[4] << 1) | ((buffer[5] & 0x80) ? 1 : 0);
    buffer[5] = (buffer[5] << 1) | ((buffer[6] & 0x80) ? 1 : 0);
    buffer[6] = (buffer[6] << 1) | ((buffer[7] & 0x80) ? 1 : 0);
    buffer[7] = (buffer[7] << 1) | ((en_status & 0x80) ? 1 : 0);
  }
  for (uint8_t i = 0; i < 8; i++)
    buffer[i] ^= xor_val;
}

bool USBUartTypeCH934X::configure_uart_parameters_(USBUartChannel *channel) {
  uint32_t baud_rate = channel->get_baud_rate();
  uint8_t data_bits = channel->get_data_bits();
  uint8_t stop_bits = channel->get_stop_bits();
  UARTParityOptions parity = channel->parity_;
  uint8_t portnum = channel->index_;
  uint8_t rgadd = this->get_reg_address_(portnum);

  usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGE(TAG, "UART parameter setup failed, status=%s", esp_err_to_name(status.error_code));
  };

  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    uint8_t pedt = (baud_rate > 115200) ? 0x01 : 0x00;
    uint32_t clrt = (baud_rate > 115200) ? 44236800 : 12000000;
    uint8_t bd1 = 0, bd2 = 0, bd3 = 0;

    switch (baud_rate) {
      case 250000:
        bd3 = 1;
        break;
      case 500000:
        bd3 = 2;
        break;
      case 1000000:
        bd3 = 3;
        break;
      case 1500000:
        bd3 = 4;
        break;
      case 3000000:
        bd3 = 5;
        break;
      case 12000000:
        bd3 = 6;
        break;
      default: {
        uint32_t factor = clrt / baud_rate;
        bd1 = factor & 0xFF;
        bd2 = (factor >> 8) & 0xFF;
        break;
      }
    }

    uint8_t sbit = (stop_bits == UART_CONFIG_STOP_BITS_2) ? 0x04 : 0x00;
    uint8_t pbit = 0;
    switch (parity) {
      case UART_CONFIG_PARITY_ODD:
        pbit = 0x08;
        break;
      case UART_CONFIG_PARITY_EVEN:
        pbit = (0x01 << 4) | 0x08;
        break;
      case UART_CONFIG_PARITY_MARK:
        pbit = (0x02 << 4) | 0x08;
        break;
      case UART_CONFIG_PARITY_SPACE:
        pbit = (0x03 << 4) | 0x08;
        break;
      default:
        break;
    }
    uint8_t dbit = (data_bits >= 5 && data_bits <= 8) ? (data_bits - 5) : 0x03;

    uint8_t buffer[6];
    buffer[0] = CMD_W_BR;
    buffer[1] = rgadd + R_C1;
    buffer[2] = pedt | 0x50;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

    buffer[0] = 0x20;
    buffer[1] = rgadd + R_C3;
    buffer[2] = bd1;
    buffer[3] = bd2;
    buffer[4] = bd3;
    buffer[5] = 0x00;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 6);

    buffer[0] = CMD_W_R;
    buffer[1] = rgadd + R_C3;
    buffer[2] = dbit | pbit | sbit;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  } else if (this->chiptype_ == CHIP_CH348L || this->chiptype_ == CHIP_CH348Q) {
    uint8_t rol = esp_random() & 0x0F;
    uint8_t xor_val = esp_random() & 0xFF;
    uint8_t buffer[12];

    buffer[0] = 0x90 | (portnum & 0x0F);
    buffer[1] = R_INIT;
    buffer[2] = (portnum & 0x0F) | ((rol << 4) & 0xF0);
    buffer[3] = (baud_rate >> 24) & 0xFF;
    buffer[4] = (baud_rate >> 16) & 0xFF;
    buffer[5] = (baud_rate >> 8) & 0xFF;
    buffer[6] = baud_rate & 0xFF;
    buffer[7] = (stop_bits == UART_CONFIG_STOP_BITS_2) ? 0x02 : 0x00;
    buffer[8] = parity;
    buffer[9] = data_bits;
    buffer[10] = (baud_rate < 9600) ? 0x1E : 0x0A;
    buffer[11] = xor_val;

    cal_outdata(buffer + 3, rol, xor_val);
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 12);
  }

  return true;
}

bool USBUartTypeCH934X::parse_descriptors_(usb_device_handle_t dev_hdl) {
  const usb_config_desc_t *config_desc;
  const usb_device_desc_t *device_desc;
  int desc_offset = 0;

  if (usb_host_get_device_descriptor(dev_hdl, &device_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_device_descriptor failed");
    return false;
  }
  if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_active_config_descriptor failed");
    return false;
  }

  this->pid_ = device_desc->idProduct;

  const auto *this_desc = reinterpret_cast<const usb_standard_desc_t *>(config_desc);
  this_desc = usb_parse_next_descriptor(this_desc, config_desc->wTotalLength, &desc_offset);
  if (!this_desc) {
    ESP_LOGE(TAG, "Failed to parse next descriptor");
    return false;
  }

  const auto *iad_desc = reinterpret_cast<const usb_iad_desc_t *>(this_desc);
  optional<Ch934xEps> uart_host_dev = get_uart_hub(config_desc, iad_desc->bFirstInterface);
  if (uart_host_dev.has_value()) {
    this->uart_host_dev_ = *uart_host_dev;
    return true;
  }

  return false;
}

void USBUartTypeCH934X::start_rx_reader_() {
  if (this->rx_running_.load())
    return;

  const auto *ep = this->uart_host_dev_.in_ep;

  auto callback = [this](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "RX transfer failed, status=%s", esp_err_to_name(status.error_code));
      this->rx_running_.store(false);
      return;
    }
    if (status.data_len > 0)
      this->demux_rx_data_(status.data, status.data_len);
    this->rx_running_.store(false);
    this->start_rx_reader_();
  };

  this->rx_running_.store(true);
  this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize);
}

void USBUartTypeCH934X::demux_rx_data_(const uint8_t *data, size_t len) {
  // THREAD CONTEXT: USB task — must not write to input_buffer_ directly.
  // Demux each fixed-size RX block and push into the shared chunk pool/queue
  // for main-loop consumption via USBUartComponent::loop().
  for (size_t i = 0; i < len; i += RX_BLOCK_SIZE) {
    if (i + RX_HEADER_SIZE > len)
      break;

    uint8_t port_num = data[i];
    uint8_t data_len = data[i + 1];

    if (port_num < this->port_offset_ || port_num >= (this->port_offset_ + this->num_ports_)) {
      ESP_LOGW(TAG, "Invalid port number in RX data: %u", port_num);
      continue;
    }
    if (data_len > RX_MAX_DATA) {
      ESP_LOGW(TAG, "Invalid data length in RX: %u", data_len);
      continue;
    }
    if (data_len == 0)
      continue;

    uint8_t adjusted_port = port_num - this->port_offset_;
    if (adjusted_port >= this->channels_.size())
      continue;

    USBUartChannel *channel = this->channels_[adjusted_port];
    if (!channel->initialised_.load())
      continue;

    UsbDataChunk *chunk = this->chunk_pool_.allocate();
    if (chunk == nullptr) {
      this->usb_data_queue_.increment_dropped_count();
      continue;
    }
    memcpy(chunk->data, data + i + RX_HEADER_SIZE, data_len);
    chunk->length = data_len;
    chunk->channel = channel;
    this->usb_data_queue_.push(chunk);
  }
  this->enable_loop_soon_any_context();
  App.wake_loop_threadsafe();
}

void USBUartTypeCH934X::start_command_reader_() {
  if (this->cmd_running_.load())
    return;

  const auto *ep = this->uart_host_dev_.ep_cmd_read;

  auto callback = [this](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGW(TAG, "CMD transfer failed, status=%s", esp_err_to_name(status.error_code));
      this->cmd_running_.store(false);
      return;
    }
    if (status.data_len > 0)
      this->handle_command_data_(status.data, status.data_len);
    this->cmd_running_.store(false);
    this->start_command_reader_();
  };

  this->cmd_running_.store(true);
  this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize);
}

void USBUartTypeCH934X::handle_command_data_(const uint8_t *data, size_t len) {
  ESP_LOGV(TAG, "CMD data received: %d bytes", len);
}

void USBUartTypeCH934X::start_input(USBUartChannel * /*channel*/) {}

void CH934XChannel::write_array(const uint8_t *data, size_t len) {
  if (!this->initialised_.load() || this->tx_shared_channel_ == nullptr)
    return;

  auto *shared = this->tx_shared_channel_;

#ifdef USE_UART_DEBUGGER
  if (this->debug_) {
    constexpr size_t batch = 16;
    char buf[4 + format_hex_pretty_size(batch)];
    for (size_t off = 0; off < len; off += batch) {
      size_t n = std::min(len - off, batch);
      strcpy(buf, ">>> ");
      format_hex_pretty_to(buf + 4, sizeof(buf) - 4, data + off, n, ',');
      ESP_LOGD(TAG, "%s%s", this->debug_prefix_.c_str(), buf);
    }
  }
#endif

  while (len > 0) {
    UsbOutputChunk *chunk = shared->output_pool_.allocate();
    if (chunk == nullptr) {
      ESP_LOGE(TAG, "Output pool full - lost %zu bytes", len);
      break;
    }
    size_t data_len = std::min(len, TX_MAX_DATA);
    // Pre-build the TX header into the chunk: [port, len_lo, len_hi, data...]
    chunk->data[0] = this->tx_port_byte_;
    chunk->data[1] = data_len & 0xFF;
    chunk->data[2] = (data_len >> 8) & 0xFF;
    memcpy(chunk->data + TX_HEADER_SIZE, data, data_len);
    chunk->length = static_cast<uint16_t>(TX_HEADER_SIZE + data_len);
    shared->output_queue_.push(chunk);
    data += data_len;
    len -= data_len;
  }
  this->parent_->start_output(shared);
}

uart::UARTFlushResult CH934XChannel::flush() {
  // Poll the shared channel-0 queue and its output_started flag, not our own.
  auto *shared = this->tx_shared_channel_;
  if (shared == nullptr)
    return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
  uint32_t start = millis();
  while ((!shared->output_queue_.empty() || shared->output_started_.load()) &&
         millis() - start < this->flush_timeout_ms_) {
    this->parent_->start_output(shared);
    yield();
  }
  if (!shared->output_queue_.empty() || shared->output_started_.load())
    return uart::UARTFlushResult::UART_FLUSH_RESULT_TIMEOUT;
  return uart::UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
}

static void fix_mps(const usb_ep_desc_t *ep) {
  if (ep != nullptr) {
    auto *ep_mutable = const_cast<usb_ep_desc_t *>(ep);
    if (ep->wMaxPacketSize > esphome::usb_host::USB_MAX_PACKET_SIZE) {
      ESP_LOGW(TAG, "Corrected MPS of EP 0x%02X from %u to %u", static_cast<uint8_t>(ep->bEndpointAddress & 0xFF),
               ep->wMaxPacketSize, esphome::usb_host::USB_MAX_PACKET_SIZE);
      ep_mutable->wMaxPacketSize = esphome::usb_host::USB_MAX_PACKET_SIZE;
    }
  }
}

void USBUartTypeCH934X::on_connected() {
  ESP_LOGI(TAG, "CH934X connected (VID=%04X, PID=%04X)", this->vid_, this->pid_);

  if (!this->parse_descriptors_(this->device_handle_)) {
    ESP_LOGE(TAG, "CH934X parse_descriptors_ failed");
    this->status_set_error(LOG_STR("No UART-Serial-Hub device found"));
    this->disconnect();
    return;
  }

  fix_mps(this->uart_host_dev_.in_ep);
  fix_mps(this->uart_host_dev_.out_ep);
  fix_mps(this->uart_host_dev_.ep_cmd_read);
  fix_mps(this->uart_host_dev_.ep_cmd_write);

  for (auto &request : this->requests_) {
    usb_host_transfer_alloc(esphome::usb_host::USB_MAX_PACKET_SIZE, 0, &request.transfer);
    request.client = this;
  }

  auto err = usb_host_interface_claim(this->handle_, this->device_handle_, this->uart_host_dev_.data_interface, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_interface_claim failed: %s, intf=%d", esp_err_to_name(err),
             this->uart_host_dev_.data_interface);
    this->status_set_error(LOG_STR("usb_host_interface_claim failed"));
    this->disconnect();
    return;
  }

  this->enable_channels();
}

void USBUartTypeCH934X::on_disconnected() {
  this->rx_running_.store(false);
  this->cmd_running_.store(false);

  for (auto *channel : this->channels_) {
    channel->initialised_.store(false);
    channel->input_started_.store(false);
    channel->input_buffer_.clear();
  }
  // Shared TX queue lives on channel 0 — drain it on disconnect
  if (!this->channels_.empty()) {
    auto *shared = this->channels_[0];
    shared->output_started_.store(false);
    UsbOutputChunk *c;
    while ((c = shared->output_queue_.pop()) != nullptr)
      shared->output_pool_.release(c);
  }

  if (this->uart_host_dev_.in_ep != nullptr) {
    usb_host_endpoint_halt(this->device_handle_, this->uart_host_dev_.in_ep->bEndpointAddress);
    usb_host_endpoint_flush(this->device_handle_, this->uart_host_dev_.in_ep->bEndpointAddress);
  }
  if (this->uart_host_dev_.out_ep != nullptr) {
    usb_host_endpoint_halt(this->device_handle_, this->uart_host_dev_.out_ep->bEndpointAddress);
    usb_host_endpoint_flush(this->device_handle_, this->uart_host_dev_.out_ep->bEndpointAddress);
  }
  if (this->uart_host_dev_.ep_cmd_read != nullptr) {
    usb_host_endpoint_halt(this->device_handle_, this->uart_host_dev_.ep_cmd_read->bEndpointAddress);
    usb_host_endpoint_flush(this->device_handle_, this->uart_host_dev_.ep_cmd_read->bEndpointAddress);
  }
  if (this->uart_host_dev_.ep_cmd_write != nullptr) {
    usb_host_endpoint_halt(this->device_handle_, this->uart_host_dev_.ep_cmd_write->bEndpointAddress);
    usb_host_endpoint_flush(this->device_handle_, this->uart_host_dev_.ep_cmd_write->bEndpointAddress);
  }

  usb_host_interface_release(this->handle_, this->device_handle_, this->uart_host_dev_.data_interface);
  USBClient::on_disconnected();
}

}  // namespace esphome::usb_uart
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
