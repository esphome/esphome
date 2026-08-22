#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
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
  if (this->channels_.empty())
    return false;

  if (step == 0) {
    ESP_LOGD(TAG, "Starting device setup");
    // Vendor IN read: device returns 4 chip-id bytes used below to distinguish variants.
    this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_IN, 0x96, 0, 0, std::vector<uint8_t>{0, 0, 0, 0});
    return true;
  }

  if (step == 1) {
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
      this->channel_write_count_ = 8;
    } else if (this->pid_ == 0x55D9) {
      this->chiptype_ = (response[1] & (0x02 << 6)) ? CHIP_CH348Q : CHIP_CH348L;
      this->num_ports_ = 8;
      this->port_offset_ = 0;
      this->channel_write_count_ = 4;
    } else {
      ESP_LOGE(TAG, "Unknown product_id: 0x%04X", this->pid_);
      return false;
    }

    ESP_LOGI(TAG, "Found chip type %s with %u ports", get_chiptype_string(this->chiptype_).c_str(), this->num_ports_);
    ESP_LOGD(TAG, "Configuring device registers for %u ports (%u init lanes)", this->num_ports_, this->init_lanes_);

    this->init_failed_mask_.store(0);
    this->init_device_failed_.store(false);
    this->init_group_start_ = 0;
    this->init_write_idx_ = 0;

    bool is_9344 = this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q;
    uint8_t rgadd = this->get_reg_address_(this->num_ports_ - 1);
    uint8_t buffer[3];

    // Device-level writes are independent of each other, so they go out as one round.
    this->init_pending_.store(is_9344 ? 3 : 2, std::memory_order_release);
    buffer[0] = CMD_W_R;
    buffer[1] = rgadd + R_C2;
    buffer[2] = 0x87;
    this->config_bulk_write_(nullptr, buffer, 3);
    if (is_9344) {
      buffer[1] = rgadd + R_C3;
      buffer[2] = 0x03;
      this->config_bulk_write_(nullptr, buffer, 3);
    }
    buffer[1] = rgadd + R_C3;
    buffer[2] = 0x08;
    this->config_bulk_write_(nullptr, buffer, 3);
    return true;
  }

  uint8_t n = static_cast<uint8_t>(this->channels_.size());
  for (;;) {
    if (this->init_group_start_ >= n) {
      this->finalize_init_();
      return false;
    }

    uint8_t group_end = this->init_group_start_ + this->init_lanes_;
    if (group_end > n)
      group_end = n;

    // Count the channels that will emit a write this round (skip out-of-range/failed ones).
    uint8_t count = 0;
    for (uint8_t c = this->init_group_start_; c < group_end; c++) {
      USBUartChannelBase *ch = this->channels_[c];
      if (ch->index_ >= this->num_ports_)
        continue;
      if (this->init_device_failed_.load() || (this->init_failed_mask_.load() & (1u << ch->index_)))
        continue;
      count++;
    }

    if (count == 0) {
      if (++this->init_write_idx_ >= this->channel_write_count_) {
        this->init_write_idx_ = 0;
        this->init_group_start_ += this->init_lanes_;
      }
      continue;
    }

    this->init_pending_.store(count, std::memory_order_release);
    for (uint8_t c = this->init_group_start_; c < group_end; c++) {
      USBUartChannelBase *ch = this->channels_[c];
      if (ch->index_ >= this->num_ports_)
        continue;
      if (this->init_device_failed_.load() || (this->init_failed_mask_.load() & (1u << ch->index_)))
        continue;
      uint8_t buffer[12];
      uint8_t len = 0;
      if (!this->build_channel_write_(ch, this->init_write_idx_, buffer, &len)) {
        if (this->init_pending_.fetch_sub(1, std::memory_order_acq_rel) == 1)
          this->cfg_done_.store(true, std::memory_order_release);
        continue;
      }
      this->config_bulk_write_(ch, buffer, len);
    }

    if (++this->init_write_idx_ >= this->channel_write_count_) {
      this->init_write_idx_ = 0;
      this->init_group_start_ += this->init_lanes_;
    }
    return true;
  }
}

// Per-channel settings. On full init every channel is configured by config_device_step();
// this reload path (load_settings -> apply_channel_settings) reconfigures one channel at
// runtime. It reuses build_channel_write_() so the register encoding lives in exactly one
// place, starting after the mode write(s): the port stays in UART mode across a reload, only
// the baud/parity parameters and their commit writes are re-sent. Previously this path was a
// separate copy that skipped the CH348 R_C4=0x00 baud commit, so runtime baud changes did not
// latch on a CH348 while they did on a CH9344.
bool USBUartTypeCH934X::config_step(USBUartChannelBase *channel, uint8_t step, bool reload, bool ok,
                                    const uint8_t *response) {
  if (!reload || channel->index_ >= this->num_ports_)
    return false;

  const bool is_9344 = this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q;
  const uint8_t start_idx = is_9344 ? 2 : 1;  // skip the mode write(s), reconfigure params + commit

  usb_host::transfer_cb_t callback = [this](const usb_host::TransferStatus &status) {
    if (!status.success)
      ESP_LOGE(TAG, "Reload register write failed: %s", esp_err_to_name(status.error_code));
  };

  uint8_t buffer[12];
  uint8_t len = 0;
  for (uint8_t idx = start_idx; this->build_channel_write_(channel, idx, buffer, &len); idx++)
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, len);

  return false;
}

bool USBUartTypeCH934X::config_bulk_write_(USBUartChannelBase *channel, const uint8_t *data, uint16_t len) {
  uint8_t port = (channel != nullptr) ? channel->index_ : 0xFF;
  usb_host::transfer_cb_t callback = [this, port](const usb_host::TransferStatus &status) {
    if (!status.success) {
      if (port == 0xFF) {
        this->init_device_failed_.store(true, std::memory_order_release);
      } else {
        this->init_failed_mask_.fetch_or(static_cast<uint8_t>(1u << port), std::memory_order_acq_rel);
      }
      ESP_LOGE(TAG, "Init command write failed (port %d): %s", static_cast<int8_t>(port),
               esp_err_to_name(status.error_code));
    }
    // The write that completes the round releases the config machine and wakes the loop.
    if (this->init_pending_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      this->cfg_done_.store(true, std::memory_order_release);
      this->enable_loop_soon_any_context();
      App.wake_loop_threadsafe();
    }
  };

  bool submitted = this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, data, len);
  if (!submitted) {
    // No free transfer slot: no callback will fire, so account for this write here (loop
    // thread) and mark it failed instead of silently dropping it.
    if (port == 0xFF) {
      this->init_device_failed_.store(true, std::memory_order_release);
    } else {
      this->init_failed_mask_.fetch_or(static_cast<uint8_t>(1u << port), std::memory_order_acq_rel);
    }
    ESP_LOGE(TAG, "Init command write submit failed (port %d): no free transfer slot", static_cast<int8_t>(port));
    if (this->init_pending_.fetch_sub(1, std::memory_order_acq_rel) == 1)
      this->cfg_done_.store(true, std::memory_order_release);
  }
  return submitted;
}

void USBUartTypeCH934X::finalize_init_() {
  const bool device_failed = this->init_device_failed_.load();
  const uint8_t failed_mask = this->init_failed_mask_.load();

  // Channel 0 owns the shared TX queue/pool for every channel, so a channel-0 (or
  // device-level) init failure cannot be worked around by using the other ports -- their TX
  // would route through a dead queue. Fail the whole device setup instead.
  if (device_failed || (failed_mask & 0x01u) != 0) {
    for (auto *channel : this->channels_)
      channel->initialised_.store(false);
    ESP_LOGE(TAG, "Device init failed (channel 0 or device-level write); marking setup failed");
    this->status_set_error(LOG_STR("CH934X device initialisation failed"));
    return;
  }

  // Wire the shared TX endpoint (channel 0) and per-channel TX routing
  auto *shared = this->channels_[0];
  shared->cdc_dev_.out_ep = this->uart_host_dev_.out_ep;

  bool any_failed = false;

  for (auto *channel : this->channels_) {
    auto *ch934x_channel = static_cast<CH934XChannel *>(channel);
    ch934x_channel->tx_shared_channel_ = shared;
    ch934x_channel->tx_port_byte_ = static_cast<uint8_t>(this->port_offset_ + channel->index_);

    bool failed = channel->index_ >= this->num_ports_ || (failed_mask & (1u << channel->index_)) != 0;
    if (failed) {
      channel->initialised_.store(false);
      any_failed = true;
      ESP_LOGE(TAG, "Channel %d failed initialisation and will not be used", channel->index_);
    } else {
      channel->initialised_.store(true);
    }
  }

  if (any_failed) {
    this->status_set_error(LOG_STR("One or more CH934X channels failed to initialise"));
  } else {
    this->status_clear_error();
  }

  this->start_rx_reader_();
  this->start_command_reader_();
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

bool USBUartTypeCH934X::build_channel_write_(USBUartChannelBase *channel, uint8_t idx, uint8_t *buffer, uint8_t *len) {
  uint8_t portnum = channel->index_;
  uint8_t rgadd = this->get_reg_address_(portnum);
  bool is_9344 = this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q;

  if (is_9344) {
    // CH9344 per-channel sequence: mode(2) + parameters(3) + control-line/baud commit(3) = 8.
    switch (idx) {
      case 0:  // UART mode
        buffer[0] = CMD_W_BR;
        buffer[1] = rgadd + R_C4;
        buffer[2] = 0x50;
        *len = 3;
        return true;
      case 1:  // normal UART mode for all ports
        buffer[0] = CMD_W_BR;
        buffer[1] = 0x97;
        buffer[2] = 0x00;
        *len = 3;
        return true;
      case 2:
      case 3:
      case 4: {
        uint32_t baud_rate = channel->get_baud_rate();
        uint8_t data_bits = channel->get_data_bits();
        uint8_t stop_bits = channel->get_stop_bits();
        UARTParityOptions parity = channel->parity_;
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
        if (idx == 2) {
          buffer[0] = CMD_W_BR;
          buffer[1] = rgadd + R_C1;
          buffer[2] = pedt | 0x50;
          *len = 3;
          return true;
        }
        if (idx == 3) {
          buffer[0] = 0x20;
          buffer[1] = rgadd + R_C3;
          buffer[2] = bd1;
          buffer[3] = bd2;
          buffer[4] = bd3;
          buffer[5] = 0x00;
          *len = 6;
          return true;
        }
        buffer[0] = CMD_W_R;  // idx == 4
        buffer[1] = rgadd + R_C3;
        buffer[2] = dbit | pbit | sbit;
        *len = 3;
        return true;
      }
      case 5:  // re-assert control lines
        buffer[0] = CMD_W_R;
        buffer[1] = rgadd + R_C1;
        buffer[2] = 0x07;
        *len = 3;
        return true;
      case 6:  // commit baud config
        buffer[0] = CMD_W_BR;
        buffer[1] = rgadd + R_C4;
        buffer[2] = 0x00;
        *len = 3;
        return true;
      case 7:
        buffer[0] = CMD_W_BR;
        buffer[1] = rgadd + R_C4;
        buffer[2] = 0x10;
        *len = 3;
        return true;
      default:
        return false;
    }
  }

  // CH348 per-channel sequence: mode(1) + parameters(1) + control-line/baud commit(2) = 4.
  switch (idx) {
    case 0:  // UART mode
      buffer[0] = CMD_W_BR;
      buffer[1] = rgadd + R_C4;
      buffer[2] = 0x50;
      *len = 3;
      return true;
    case 1: {
      uint32_t baud_rate = channel->get_baud_rate();
      uint8_t data_bits = channel->get_data_bits();
      uint8_t stop_bits = channel->get_stop_bits();
      UARTParityOptions parity = channel->parity_;
      uint8_t rol = esp_random() & 0x0F;
      uint8_t xor_val = esp_random() & 0xFF;
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
      *len = 12;
      return true;
    }
    case 2:  // re-assert control lines
      buffer[0] = CMD_W_R;
      buffer[1] = rgadd + R_C1;
      buffer[2] = 0x07;
      *len = 3;
      return true;
    case 3:  // commit baud config
      buffer[0] = CMD_W_BR;
      buffer[1] = rgadd + R_C4;
      buffer[2] = 0x00;
      *len = 3;
      return true;
    default:
      return false;
  }
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
  // Called from both the main loop (start_input on every read_array) and the USB task (the
  // success callback re-arming), so the running guard must be atomic to avoid a double submit.
  bool expected = false;
  if (!this->rx_running_.compare_exchange_strong(expected, true))
    return;

  const auto *ep = this->uart_host_dev_.in_ep;

  auto callback = [this](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "RX transfer failed, status=%s", esp_err_to_name(status.error_code));
      this->rx_running_.store(false);
      return;
    }
    bool pool_full = false;
    if (status.data_len > 0)
      pool_full = this->demux_rx_data_(status.data, status.data_len);
    this->rx_running_.store(false);
    if (!pool_full)
      this->start_rx_reader_();
  };

  if (!this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize)) {
    ESP_LOGW(TAG, "Failed to submit RX transfer");
    this->rx_running_.store(false);
  }
}

bool USBUartTypeCH934X::demux_rx_data_(const uint8_t *data, size_t len) {
  bool pool_full = false;
  for (size_t i = 0; i + RX_BLOCK_SIZE <= len; i += RX_BLOCK_SIZE) {
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

    USBUartChannelBase *channel = this->channels_[adjusted_port];
    if (!channel->initialised_.load())
      continue;

    UsbDataChunk *chunk = this->chunk_pool_.allocate();
    if (chunk == nullptr) {
      // Pool exhausted: mirror the CDC path -- drop, then stop re-arming so read_array()
      // restarts RX once the main loop has drained the queue (backpressure).
      this->usb_data_queue_.increment_dropped_count();
      pool_full = true;
      continue;
    }
    memcpy(chunk->data, data + i + RX_HEADER_SIZE, data_len);
    chunk->length = data_len;
    chunk->channel = channel;
    this->usb_data_queue_.push(chunk);
  }
  this->enable_loop_soon_any_context();
  App.wake_loop_threadsafe();
  return pool_full;
}

void USBUartTypeCH934X::start_command_reader_() {
  // Re-armed from both the main loop and the USB task (see start_rx_reader_); atomic guard.
  bool expected = false;
  if (!this->cmd_running_.compare_exchange_strong(expected, true))
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

  if (!this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize)) {
    ESP_LOGW(TAG, "Failed to submit CMD transfer");
    this->cmd_running_.store(false);
  }
}

void USBUartTypeCH934X::handle_command_data_(const uint8_t * /*data*/, size_t len) {
  ESP_LOGV(TAG, "CMD data received: %u bytes", len);
}

void USBUartTypeCH934X::start_input(USBUartChannelBase *channel) {
  auto started = false;
  if (!channel->input_started_.compare_exchange_strong(started, true))
    return;
  this->start_rx_reader_();
  this->start_command_reader_();
}

void CH934XChannel::write_array(const uint8_t *data, size_t len) {
  if (!this->initialised_.load() || this->tx_shared_channel_ == nullptr)
    return;

  auto *shared = this->tx_shared_channel_;
#ifdef USE_UART_DEBUGGER
  if (this->debug_) {
    constexpr size_t batch = 16;
    char buf[format_hex_pretty_size(batch)];
    for (size_t off = 0; off < len; off += batch) {
      size_t n = std::min(len - off, batch);
      format_hex_pretty_to(buf, data + off, n, ',');
      ESP_LOGD(TAG, "%s>>> %s", this->debug_prefix_.c_str(), buf);
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
    if (ep->wMaxPacketSize > usb_host::USB_MAX_PACKET_SIZE) {
      ESP_LOGW(TAG, "Corrected MPS of EP 0x%02X from %u to %u", static_cast<uint8_t>(ep->bEndpointAddress & 0xFF),
               ep->wMaxPacketSize, usb_host::USB_MAX_PACKET_SIZE);
      ep_mutable->wMaxPacketSize = usb_host::USB_MAX_PACKET_SIZE;
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

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
