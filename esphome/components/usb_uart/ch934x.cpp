#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/components/bytebuffer/bytebuffer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef USE_UART_DEBUGGER
#include "esphome/components/uart/uart_debugger.h"
#endif

namespace esphome {
namespace usb_uart {

using namespace bytebuffer;

/**
 * CH934x - USB to Multi-UART with Multiplexed Endpoints
 *
 * This driver supports CH9344 (4 ports) and CH348 (8 ports) chips.
 * Unlike CDC-ACM devices, these chips use:
 * - Shared IN/OUT endpoints for all serial ports (multiplexed data)
 * - Separate CMD endpoints for control/status
 *
 * Data format on multiplexed endpoints:
 * RX: 32-byte blocks [port][len][data...]
 * TX: Variable [port][len_lo][len_hi][data...]
 */

// CH934x protocol constants
static constexpr uint8_t CMD_W_R = 0xC0;
static constexpr uint8_t CMD_W_BR = 0x80;
static constexpr uint8_t R_C1 = 0x01;
static constexpr uint8_t R_C2 = 0x02;
static constexpr uint8_t R_C3 = 0x03;
static constexpr uint8_t R_C4 = 0x04;
static constexpr uint8_t R_INIT = 0xA1;

// Chip type identification - using enum from header

// RX data format constants
static constexpr size_t RX_BLOCK_SIZE = 32;  // Fixed 32-byte blocks
static constexpr size_t RX_HEADER_SIZE = 2;  // Port + length
static constexpr size_t RX_MAX_DATA = 30;    // Max data per block

// TX data format constants
static constexpr size_t TX_HEADER_SIZE = 3;  // Port + length_lo + length_hi

static std::string get_chiptype_string_(uint8_t enum_value) {
  switch (enum_value) {
    case CHIP_CH9344L:
      return "CH9344L";
    case CHIP_CH9344Q:
      return "CH9344Q";
    case CHIP_CH348L:
      return "CH348L";
    case CHIP_CH348Q:
      return "CH348Q";
  }
  return "unknown";
}

/**
 * Parse USB descriptors to find the CH934x UART hub interface
 * @param config_desc The configuration descriptor
 * @param intf_idx The index of the interface to examine
 * @return Optional Ch934xEps structure with endpoint information
 */
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

  // Parse all 4 endpoints (data in/out, cmd in/out)
  for (uint8_t i = 0; i < intf_desc->bNumEndpoints; i++) {
    ep_offset = conf_offset;
    const auto *ep = usb_parse_endpoint_descriptor_by_index(intf_desc, i, config_desc->wTotalLength, &ep_offset);
    if (!ep) {
      ESP_LOGE(TAG, "Ran out of endpoints before finding all required endpoints");
      return nullopt;
    }

    ESP_LOGD(TAG, "ep[%d]: bEndpointAddress=%02X, bmAttributes=%02X", i, ep->bEndpointAddress, ep->bmAttributes);

    // Endpoints are ordered: data_in, data_out, cmd_in, cmd_out
    switch (i) {
      case 0:
      case 1:
        // Data endpoints (bulk, bidirectional)
        // Note: Some devices swap the order, so check direction
        if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK && (ep->bEndpointAddress & usb_host::USB_DIR_IN)) {
          eps.in_ep = ep;
        } else if (ep->bmAttributes == USB_BM_ATTRIBUTES_XFER_BULK) {
          eps.out_ep = ep;
        }
        break;
      case 2:
      case 3:
        // Command endpoints (bulk, bidirectional)
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

  // Verify we found all required endpoints
  if (eps.in_ep != nullptr && eps.out_ep != nullptr && eps.ep_cmd_read != nullptr && eps.ep_cmd_write != nullptr) {
    return eps;
  }

  ESP_LOGE(TAG, "Failed to find all required endpoints");
  return nullopt;
}

/**
 * Helper class for efficient buffer allocation
 * Uses stack for small sizes, heap for large
 */
template<size_t STACK_SIZE> class SmallBufferWithHeapFallback {
 public:
  uint8_t *get(size_t size) {
    if (size <= STACK_SIZE) {
      return this->stack_buffer_;
    }
    this->heap_buffer_ = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    return this->heap_buffer_.get();
  }

 private:
  uint8_t stack_buffer_[STACK_SIZE];
  std::unique_ptr<uint8_t[]> heap_buffer_;
};

/**
 * Helper to calculate register address based on chip type and port number
 */
uint8_t USBUartTypeCH934X::get_reg_address_(uint8_t portnum) {
  uint8_t rgadd = 0;

  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    rgadd = 0x10 * portnum + 0x08;
  } else if (this->chiptype_ == CHIP_CH348L || this->chiptype_ == CHIP_CH348Q) {
    if (portnum < 4) {
      rgadd = 0x10 * portnum;
    } else {
      rgadd = 0x10 * (portnum - 4) + 0x08;
    }
  }

  return rgadd;
}

/**
 * Device-level initialization
 * Identifies chip type and configures global settings
 * Channels are configured in the callback after chip detection
 */
void USBUartTypeCH934X::configure_device_() {
  ESP_LOGD(TAG, "Starting device setup");

  // Callback for bulk transfers (simple status logging)
  esphome::usb_host::transfer_cb_t bulk_callback = [=](const esphome::usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "Bulk transfer failed, status=%s", esp_err_to_name(status.error_code));
    }
  };

  // Callback for control transfer to identify chip type
  esphome::usb_host::transfer_cb_t ctrl_callback = [=, this](const esphome::usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "Control transfer failed, status=%s", esp_err_to_name(status.error_code));
      return;
    }

    ESP_LOGV(TAG, "Received chip id response bytes 0x%02x 0x%02x 0x%02x 0x%02x", status.data[0], status.data[1],
             status.data[2], status.data[3]);

    this->chipversion_ = status.data[0];

    // Determine chip type based on product ID and version bytes
    if (this->pid_ == 0xE018) {
      // CH9344 series (4 ports)
      if (this->chipversion_ >= 0x39) {
        this->bpackload_ = true;
      }
      if (status.data[0] >= 0x40) {
        this->chiptype_ = CHIP_CH9344Q;
      } else {
        this->chiptype_ = CHIP_CH9344L;
      }
      this->num_ports_ = 4;
      this->port_offset_ = 4;
    } else if (this->pid_ == 0x55D9) {
      // CH348 series (8 ports)
      if (this->chipversion_ >= 0x8a) {
        this->bpackload_ = true;
      }
      if (status.data[1] & (0x02 << 6)) {
        this->chiptype_ = CHIP_CH348Q;
      } else {
        this->chiptype_ = CHIP_CH348L;
      }
      this->num_ports_ = 8;
      this->port_offset_ = 0;
    } else {
      ESP_LOGE(TAG, "Unknown product_id: 0x%04X", this->pid_);
      return;
    }

    ESP_LOGI(TAG, "Found chip type %s (version 0x%02X) with %u ports", get_chiptype_string_(this->chiptype_).c_str(),
             this->chipversion_, this->num_ports_);

    // Now that we know the chip type, defer channel configuration to main loop
    this->defer([this]() { this->configure_channels_after_detection_(); });
  };

  // Send chip version request
  auto err = this->control_transfer(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, 0x96, 0, 0, ctrl_callback);
  if (err != true) {
    ESP_LOGE(TAG, "Fetching chip id failed");
    return;
  }

  ESP_LOGD(TAG, "Chip detection request sent, waiting for callback...");
}

/**
 * Configure device registers and channels after chip detection
 * Called via defer() from chip detection callback
 */
void USBUartTypeCH934X::configure_channels_after_detection_() {
  if (this->num_ports_ == 0) {
    ESP_LOGE(TAG, "Cannot configure channels - chip not detected");
    return;
  }

  ESP_LOGD(TAG, "Configuring device registers for %u ports", this->num_ports_);

  // Callback for bulk transfers
  esphome::usb_host::transfer_cb_t bulk_callback = [=](const esphome::usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "Bulk transfer failed, status=%s", esp_err_to_name(status.error_code));
    }
  };

  // Configure port registers (chip type is now known)
  uint8_t portnum = this->num_ports_ - 1;
  uint8_t rgadd = this->get_reg_address_(portnum);

  SmallBufferWithHeapFallback<3> buffer_alloc;
  uint8_t *buffer = buffer_alloc.get(3);

  // Configure port registers
  buffer[0] = CMD_W_R;
  buffer[1] = rgadd + R_C2;
  buffer[2] = 0x87;
  ESP_LOGD(TAG, "Executing transfer_out for command 0xC0 to endpoint 0x%02X",
           this->uart_host_dev_.ep_cmd_write->bEndpointAddress);
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, bulk_callback, buffer, 3);

  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    buffer[1] = rgadd + R_C3;
    buffer[2] = 0x03;
    ESP_LOGD(TAG, "Executing transfer_out for command 0xC0 to endpoint 0x%02X",
             this->uart_host_dev_.ep_cmd_write->bEndpointAddress);
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, bulk_callback, buffer, 3);
  }

  buffer[1] = rgadd + R_C3;
  buffer[2] = 0x08;
  ESP_LOGD(TAG, "Executing transfer_out for command 0xC0 to endpoint 0x%02X",
           this->uart_host_dev_.ep_cmd_write->bEndpointAddress);
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, bulk_callback, buffer, 3);

  // Now configure each channel
  for (auto channel : this->channels_) {
    if (channel->index_ >= this->num_ports_) {
      ESP_LOGW(TAG, "Channel %d exceeds number of available ports (%d)", channel->index_, this->num_ports_);
      continue;
    }

    if (this->configure_channel_(channel)) {
      channel->initialised_.store(true);
      ESP_LOGD(TAG, "Channel %d initialized successfully", channel->index_);
    } else {
      ESP_LOGE(TAG, "Failed to initialize channel %d", channel->index_);
    }
  }

  // Start continuous RX reader
  this->start_rx_reader_();

  // Start command channel reader
  this->start_command_reader_();

  ESP_LOGD(TAG, "Device configuration completed");
}

/**
 * Configure a single UART channel
 * Sets up UART parameters for the specified channel
 */
bool USBUartTypeCH934X::configure_channel_(USBUartChannel *channel) {
  ESP_LOGD(TAG, "Configuring channel %d", channel->index_);

  if (!this->set_uart_mode(channel)) {
    ESP_LOGE(TAG, "Failed to set UART mode for channel %d", channel->index_);
    return false;
  }

  // Configure UART parameters (baudrate, stop bits, parity, data bits)
  if (!this->configure_uart_parameters_(channel)) {
    ESP_LOGE(TAG, "Failed to configure UART parameters for channel %d", channel->index_);
    return false;
  }

  // ESPHome doesn't support DTR/RTS or flow control
  // Disable them explicitly (DTR=0, RTS=0)
  uint8_t portnum = channel->index_;
  uint8_t rgadd = this->get_reg_address_(portnum);

  SmallBufferWithHeapFallback<3> buffer_alloc;
  uint8_t *buffer = buffer_alloc.get(3);

  esphome::usb_host::transfer_cb_t callback = [=](const esphome::usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "Control line setup failed, status=%s", esp_err_to_name(status.error_code));
    }
  };

  // Configure R_C1 register - enables UART RX/TX (CRITICAL!)
  buffer[0] = CMD_W_R;
  buffer[1] = rgadd + R_C1;
  buffer[2] = 0x07;  // Enable RX/TX (bits 0-2)
  ESP_LOGD(TAG, "Configuring R_C1 register for channel %d (enable RX/TX)", channel->index_);
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  // Set DTR=0, RTS=0 (both inactive)
  buffer[0] = CMD_W_BR;
  buffer[1] = rgadd + R_C4;
  buffer[2] = 0x00;  // DTR=0, RTS=0
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  // For CH9344, also set control register
  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    buffer[0] = CMD_W_BR;
    buffer[1] = rgadd + R_C4;
    buffer[2] = 0x10;  // Additional control: disable RTS
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);
  }

  ESP_LOGD(TAG, "Channel %d: DTR/RTS disabled, RX/TX enabled", channel->index_);

  return true;
}

/**
 * Set UART mode for a channel (normal operation mode)
 */
bool USBUartTypeCH934X::set_uart_mode(USBUartChannel *channel) {
  ESP_LOGD(TAG, "Setting uart_mode for channel %d", channel->index_);

  esphome::usb_host::transfer_cb_t callback = [=](const esphome::usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "Bulk transfer failed, status=%s", esp_err_to_name(status.error_code));
    }
  };

  uint8_t portnum = channel->index_;
  uint8_t rgadd = this->get_reg_address_(portnum);

  SmallBufferWithHeapFallback<3> buffer_alloc;
  uint8_t *buffer = buffer_alloc.get(3);

  // Set UART to normal mode (0x50)
  buffer[0] = CMD_W_BR;
  buffer[1] = rgadd + R_C4;
  buffer[2] = 0x50;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  // CH348 doesn't need additional mode configuration
  if (this->chiptype_ == CHIP_CH348L || this->chiptype_ == CHIP_CH348Q) {
    return true;
  }

  // CH9344 needs additional mode byte configuration
  uint8_t mode = 0x00;  // Normal UART mode
  uint8_t mode4 = 0;

  // Build mode byte for all ports
  for (uint8_t i = 0; i < this->num_ports_; i++) {
    mode4 |= mode << (i * 2);
  }

  buffer[0] = CMD_W_BR;
  buffer[1] = 0x97;
  buffer[2] = mode4;
  this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  ESP_LOGD(TAG, "UART mode set completed");
  return true;
}

/**
 * Calculate encrypted output data for CH348
 * Performs multi-byte rotation followed by XOR masking
 * Based on Linux driver cal_outdata()
 *
 * @param buffer Pointer to 8-byte buffer to encrypt (in-place)
 * @param rol Number of rotation iterations (0-15)
 * @param xor_val XOR mask value
 */
static void cal_outdata(uint8_t *buffer, uint8_t rol, uint8_t xor_val) {
  // Perform rol iterations of left-rotation across 8-byte block
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

  // Apply XOR mask to all 8 bytes
  for (uint8_t i = 0; i < 8; i++) {
    buffer[i] = buffer[i] ^ xor_val;
  }
}

/**
 * Configure UART parameters (baudrate, stop bits, parity, data bits)
 * Based on Linux driver ch9344_tty_set_termios()
 */
bool USBUartTypeCH934X::configure_uart_parameters_(USBUartChannel *channel) {
  uint32_t baud_rate = channel->get_baud_rate();
  uint8_t data_bits = channel->get_data_bits();
  uint8_t stop_bits = channel->get_stop_bits();
  UARTParityOptions parity = channel->parity_;
  uint8_t portnum = channel->index_;

  ESP_LOGD(TAG, "Configuring UART parameters for channel %d: %u baud, %u data bits, parity=%u, stop bits=%u", portnum,
           baud_rate, data_bits, parity, stop_bits);

  uint8_t rgadd = this->get_reg_address_(portnum);

  esphome::usb_host::transfer_cb_t callback = [=](const esphome::usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "UART parameter setup failed, status=%s", esp_err_to_name(status.error_code));
    }
  };

  // Calculate baud rate parameters (already extracted above)
  uint8_t pedt = 0x00;       // Extended divisor type
  uint32_t clrt = 12000000;  // Clock rate
  uint8_t bd1, bd2, bd3 = 0;

  // For high baud rates, use different clock
  if (baud_rate > 115200) {
    pedt = 0x01;
    clrt = 44236800;
  }

  // Check for special baud rates
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
      // Calculate divisor for arbitrary baud rates
      uint32_t factor = clrt / baud_rate;
      bd1 = factor & 0xFF;
      bd2 = (factor >> 8) & 0xFF;
      bd3 = 0;
      break;
    }
  }

  // For special baud rates, calculate bd1/bd2 differently
  if (bd3 != 0) {
    bd1 = 0;
    bd2 = 0;
  }

  // Configure stop bits
  uint8_t sbit = 0x00;
  if (stop_bits == UART_CONFIG_STOP_BITS_2) {
    sbit = 0x04;
  }
  // Note: 1.5 stop bits not directly supported in register format

  // Configure parity
  uint8_t pbit = 0x00;
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
    case UART_CONFIG_PARITY_NONE:
    default:
      pbit = 0x00;
      break;
  }

  // Configure data bits
  uint8_t dbit = 0x00;
  switch (data_bits) {
    case 5:
      dbit = 0x00;
      break;
    case 6:
      dbit = 0x01;
      break;
    case 7:
      dbit = 0x02;
      break;
    case 8:
      dbit = 0x03;
      break;
    default:
      dbit = 0x03;
      break;
  }

  ESP_LOGD(TAG, "Baud rate calculation: bd1=0x%02X, bd2=0x%02X, bd3=0x%02X, pedt=0x%02X", bd1, bd2, bd3, pedt);
  ESP_LOGD(TAG, "Line parameters: dbit=0x%02X, pbit=0x%02X, sbit=0x%02X", dbit, pbit, sbit);

  // CH9344 configuration sequence
  if (this->chiptype_ == CHIP_CH9344L || this->chiptype_ == CHIP_CH9344Q) {
    SmallBufferWithHeapFallback<6> buffer_alloc;
    uint8_t *buffer = buffer_alloc.get(6);

    // Command 1: Set extended control
    buffer[0] = CMD_W_BR;
    buffer[1] = rgadd + R_C1;
    buffer[2] = pedt | 0x50;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

    // Command 2: Set baud rate divisor
    buffer[0] = 0x20;  // CMD_S_T
    buffer[1] = rgadd + R_C3;
    buffer[2] = bd1;
    buffer[3] = bd2;
    buffer[4] = bd3;
    buffer[5] = 0x00;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 6);

    // Command 3: Set data/parity/stop bits
    buffer[0] = CMD_W_R;
    buffer[1] = rgadd + R_C3;
    buffer[2] = dbit | pbit | sbit;
    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 3);

  } else if (this->chiptype_ == CHIP_CH348L || this->chiptype_ == CHIP_CH348Q) {
    // CH348 uses encrypted configuration format with random rol/xor
    SmallBufferWithHeapFallback<12> buffer_alloc;
    uint8_t *buffer = buffer_alloc.get(12);

    // Generate random encryption parameters (as in Linux driver)
    uint8_t rol = esp_random() & 0x0F;      // Random 0-15
    uint8_t xor_val = esp_random() & 0xFF;  // Random 0-255

    buffer[0] = 0x90 | (portnum & 0x0F);  // CMD_WB_E with port
    buffer[1] = R_INIT;
    buffer[2] = (portnum & 0x0F) | ((rol << 4) & 0xF0);

    // Baud rate (32-bit, big-endian)
    buffer[3] = (baud_rate >> 24) & 0xFF;
    buffer[4] = (baud_rate >> 16) & 0xFF;
    buffer[5] = (baud_rate >> 8) & 0xFF;
    buffer[6] = baud_rate & 0xFF;

    // Stop bits
    buffer[7] = (stop_bits == UART_CONFIG_STOP_BITS_2) ? 0x02 : 0x00;

    // Parity type
    buffer[8] = parity;

    // Data bits
    buffer[9] = data_bits;

    // Timeout (calculated based on baud rate)
    uint8_t timeout = 0x0A;  // Default 10ms
    if (baud_rate < 9600) {
      timeout = 0x1E;  // 30ms for slow baud rates
    }
    buffer[10] = timeout;

    // XOR value (sent to chip for decryption)
    buffer[11] = xor_val;

    // Apply multi-byte rotation and XOR encryption to bytes 3-10 (8 bytes)
    cal_outdata(buffer + 3, rol, xor_val);

    ESP_LOGD(TAG, "CH348 encryption: rol=0x%02X, xor=0x%02X", rol, xor_val);

    this->transfer_out(this->uart_host_dev_.ep_cmd_write->bEndpointAddress, callback, buffer, 12);
  }

  ESP_LOGD(TAG, "UART parameters configured for channel %d", portnum);
  return true;
}

/**
 * Parse USB descriptors to identify CH934x device
 */
bool USBUartTypeCH934X::parse_descriptors(usb_device_handle_t dev_hdl) {
  const usb_config_desc_t *config_desc;
  const usb_device_desc_t *device_desc;
  int desc_offset = 0;

  // Get required descriptors
  if (usb_host_get_device_descriptor(dev_hdl, &device_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_device_descriptor failed");
    return false;
  }
  if (usb_host_get_active_config_descriptor(dev_hdl, &config_desc) != ESP_OK) {
    ESP_LOGE(TAG, "get_active_config_descriptor failed");
    return false;
  }

  // Store product ID for chip type identification
  this->pid_ = device_desc->idProduct;

  // This is a composite device that uses Interface Association Descriptor
  const auto *this_desc = reinterpret_cast<const usb_standard_desc_t *>(config_desc);
  this_desc = usb_parse_next_descriptor(this_desc, config_desc->wTotalLength, &desc_offset);
  if (!this_desc) {
    ESP_LOGE(TAG, "Failed to parse next descriptor");
    return false;
  }

  const auto *iad_desc = reinterpret_cast<const usb_iad_desc_t *>(this_desc);
  ESP_LOGV(TAG, "Found UART device in composite device");

  optional<Ch934xEps> uart_host_dev = get_uart_hub(config_desc, iad_desc->bFirstInterface);
  if (uart_host_dev.has_value()) {
    this->uart_host_dev_ = *uart_host_dev;
    return true;
  }

  return false;
}

/**
 * Enable all configured channels
 * Called after device connection is established
 * Note: Actual channel configuration happens asynchronously after chip detection
 */
void USBUartTypeCH934X::enable_channels() {
  // Start chip detection - channels will be configured in callback
  this->configure_device_();
}

/**
 * Start continuous RX reader on the multiplexed IN endpoint
 * This runs continuously in the USB task context
 */
void USBUartTypeCH934X::start_rx_reader_() {
  if (this->rx_running_.load()) {
    ESP_LOGV(TAG, "RX reader already running, skipping start");
    return;  // Already running
  }

  const auto *ep = this->uart_host_dev_.in_ep;

  // CALLBACK CONTEXT: Executed in USB task
  auto callback = [this, ep](const usb_host::TransferStatus &status) {
    ESP_LOGV(TAG, "RX callback: success=%d, data_len=%u, error=%s", status.success, status.data_len,
             esp_err_to_name(status.error_code));

    if (!status.success) {
      ESP_LOGE(TAG, "RX transfer failed, status=%s", esp_err_to_name(status.error_code));
      this->rx_running_.store(false);
      return;
    }

    if (status.data_len > 0) {
      ESP_LOGD(TAG, "RX data received: %u bytes", status.data_len);
      // Process received data (demultiplex to channels)
      this->handle_rx_data_(status.data, status.data_len);
    } else {
      ESP_LOGV(TAG, "RX transfer complete but no data (zero-length packet)");
    }

    // Immediately restart for continuous reception
    this->rx_running_.store(false);
    this->start_rx_reader_();
  };

  this->rx_running_.store(true);
  this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize);

  ESP_LOGD(TAG, "RX reader started on endpoint 0x%02X (MPS=%u)", ep->bEndpointAddress, ep->wMaxPacketSize);
}

/**
 * Handle received RX data - demultiplex to appropriate channels
 * Format: 32-byte blocks [port][len][data(30 bytes)]
 *
 * THREAD CONTEXT: USB task
 */
void USBUartTypeCH934X::handle_rx_data_(const uint8_t *data, size_t len) {
  ESP_LOGD(TAG, "Processing RX data: %u bytes", len);

  // Process data in 32-byte blocks
  for (size_t i = 0; i < len; i += RX_BLOCK_SIZE) {
    if (i + RX_HEADER_SIZE > len) {
      ESP_LOGW(TAG, "Incomplete RX block at offset %u (need %u bytes, have %u)", i, RX_HEADER_SIZE, len - i);
      break;  // Incomplete header
    }

    // Parse header
    uint8_t port_num = data[i];
    uint8_t data_len = data[i + 1];

    ESP_LOGV(TAG, "RX block[%u]: port=%u, len=%u", i / RX_BLOCK_SIZE, port_num, data_len);

    // Validate port number
    if (port_num < this->port_offset_ || port_num >= (this->port_offset_ + this->num_ports_)) {
      ESP_LOGW(TAG, "Invalid port number in RX data: %u (offset=%u, num_ports=%u)", port_num, this->port_offset_,
               this->num_ports_);
      break;
    }

    // Validate data length
    if (data_len > RX_MAX_DATA) {
      ESP_LOGW(TAG, "Invalid data length in RX: %u (max=%u)", data_len, RX_MAX_DATA);
      break;
    }

    if (data_len == 0) {
      ESP_LOGV(TAG, "Empty RX block for port %u", port_num);
      continue;  // No data in this block
    }

    // Adjust port number by offset
    uint8_t adjusted_port = port_num - this->port_offset_;

    ESP_LOGD(TAG, "RX data for port %u (adjusted from %u): %u bytes", adjusted_port, port_num, data_len);

    // Find the corresponding channel
    USBUartChannel *channel = nullptr;
    for (auto *ch : this->channels_) {
      if (ch->index_ == adjusted_port) {
        channel = ch;
        break;
      }
    }

    if (channel == nullptr) {
      ESP_LOGW(TAG, "No channel configured for port %u", adjusted_port);
      continue;
    }

    if (!channel->initialised_.load()) {
      ESP_LOGV(TAG, "Channel %u not initialized, dropping data", adjusted_port);
      continue;  // Channel not configured or not open
    }

    // Allocate chunk from pool
    UsbDataChunk *chunk = this->chunk_pool_.allocate();
    if (chunk == nullptr) {
      // No chunks available - increment dropped counter
      ESP_LOGW(TAG, "No chunks available, dropping %u bytes for channel %u", data_len, adjusted_port);
      this->usb_data_queue_.increment_dropped_count();
      continue;
    }

    // Copy data to chunk
    memcpy(chunk->data, data + i + RX_HEADER_SIZE, data_len);
    chunk->length = data_len;
    chunk->channel = channel;

    // Push to lock-free queue for main loop processing
    bool pushed = this->usb_data_queue_.push(chunk);
    if (!pushed) {
      ESP_LOGE(TAG, "Failed to push chunk to queue for channel %u", adjusted_port);
      this->chunk_pool_.release(chunk);
    } else {
      ESP_LOGV(TAG, "Queued %u bytes for channel %u", data_len, adjusted_port);
    }
  }
}

/**
 * Start command channel reader for status/control messages
 */
void USBUartTypeCH934X::start_command_reader_() {
  if (this->cmd_running_.load()) {
    return;  // Already running
  }

  const auto *ep = this->uart_host_dev_.ep_cmd_read;

  // CALLBACK CONTEXT: Executed in USB task
  auto callback = [this, ep](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGW(TAG, "CMD transfer failed, status=%s", esp_err_to_name(status.error_code));
      this->cmd_running_.store(false);
      return;
    }

    if (status.data_len > 0) {
      // Process command data (status updates, modem signals, etc.)
      this->handle_command_data_(status.data, status.data_len);
    }

    // Immediately restart for continuous reception
    this->cmd_running_.store(false);
    this->start_command_reader_();
  };

  this->cmd_running_.store(true);
  this->transfer_in(ep->bEndpointAddress, callback, ep->wMaxPacketSize);

  ESP_LOGD(TAG, "CMD reader started on endpoint 0x%02X", ep->bEndpointAddress);
}

/**
 * Handle command channel data (status updates, modem signals, etc.)
 *
 * THREAD CONTEXT: USB task
 *
 * For now, we just log the data. Full implementation would parse:
 * - Modem status changes (CTS, DSR, RI, DCD)
 * - Line status (overrun, parity, framing errors)
 * - Write buffer empty notifications
 */
void USBUartTypeCH934X::handle_command_data_(const uint8_t *data, size_t len) {
  // TODO: Implement full command parsing based on Linux driver ch9344_cmd_irq()
  // For now, just log for debugging
  ESP_LOGV(TAG, "CMD data received: %d bytes", len);

  // Basic parsing structure (from Linux driver):
  // Byte 0: Port number + flags
  // Byte 1: Register/type identifier
  // Bytes 2+: Data (varies by type)

  // Common types:
  // - R_II_B1 (0x06): Line status errors
  // - R_II_B2 (0x02): Write buffer empty
  // - R_II_B3 (0x00): Modem status change
}

/**
 * Handle TX multiplexing - send data from channels to shared OUT endpoint
 * This runs in main loop context
 *
 * THREAD CONTEXT: Main loop (safe to access output_buffer_)
 */
void USBUartTypeCH934X::handle_tx_multiplexing_() {
  // If a TX transfer is already in progress, wait for it to complete
  if (this->tx_in_progress_.load()) {
    return;
  }

  // Round-robin through channels looking for data to send
  size_t channels_checked = 0;
  while (channels_checked < this->channels_.size()) {
    auto *channel = this->channels_[this->tx_current_channel_index_];

    // Move to next channel for next iteration
    this->tx_current_channel_index_ = (this->tx_current_channel_index_ + 1) % this->channels_.size();
    channels_checked++;

    // Skip if channel not initialized or no data available
    if (!channel->initialised_.load() || channel->output_buffer_.is_empty()) {
      continue;
    }

    // Found a channel with data - send it
    this->send_next_channel_data_(channel);
    return;
  }

  // No channels have data to send
}

/**
 * Send data from the specified channel
 *
 * THREAD CONTEXT: Main loop
 */
void USBUartTypeCH934X::send_next_channel_data_(USBUartChannel *channel) {
  if (!channel->initialised_.load() || channel->output_buffer_.is_empty()) {
    return;
  }

  const auto *ep = this->uart_host_dev_.out_ep;

  // Calculate maximum data size (leave room for 3-byte header)
  size_t max_data_size = ep->wMaxPacketSize - TX_HEADER_SIZE;
  size_t available = channel->output_buffer_.get_available();
  size_t data_to_send = std::min(available, max_data_size);

  // Allocate buffer for TX packet on heap (must be valid for async transfer)
  auto *buffer = new uint8_t[ep->wMaxPacketSize];

  // Build TX packet: [port][len_lo][len_hi][data...]
  buffer[0] = this->port_offset_ + channel->index_;
  buffer[1] = data_to_send & 0xFF;
  buffer[2] = (data_to_send >> 8) & 0xFF;

  // Copy data from output buffer
  channel->output_buffer_.pop(buffer + TX_HEADER_SIZE, data_to_send);

#ifdef USE_UART_DEBUGGER
  // Copy buffer data for debug logging (before callback)
  std::vector<uint8_t> debug_data;
  if (channel->debug_) {
    debug_data.assign(buffer + TX_HEADER_SIZE, buffer + TX_HEADER_SIZE + data_to_send);
  }
#endif

  // CALLBACK CONTEXT: Executed in USB task
  auto callback = [this, channel, data_to_send, buffer
#ifdef USE_UART_DEBUGGER
                   ,
                   debug_data
#endif
  ](const usb_host::TransferStatus &status) {
    this->tx_in_progress_.store(false);

    if (!status.success) {
      ESP_LOGE(TAG, "TX transfer failed for channel %d, status=%s", channel->index_,
               esp_err_to_name(status.error_code));
      delete[] buffer;  // Free heap buffer on error
      return;
    }

    ESP_LOGV(TAG, "TX complete: channel %d, %d bytes", channel->index_, data_to_send);

#ifdef USE_UART_DEBUGGER
    if (channel->debug_) {
      // Defer debug logging to main loop
      this->defer([channel, debug_data] {
        std::string debug_prefix = channel->get_debug_prefix();
        uart::UARTDebug::log_hex(uart::UART_DIRECTION_TX, debug_data, ',', debug_prefix);
      });
    }
#endif

    // Free heap buffer
    delete[] buffer;

    // Defer next TX attempt to main loop
    this->defer([this] { this->handle_tx_multiplexing_(); });
  };

  this->tx_in_progress_.store(true);
  this->transfer_out(ep->bEndpointAddress, callback, buffer, data_to_send + TX_HEADER_SIZE);

  ESP_LOGV(TAG, "TX started: channel %d, %d bytes", channel->index_, data_to_send);
}

void USBUartTypeCH934X::start_input(USBUartChannel *channel) {
  // CH934X uses multiplexed RX endpoint managed by start_rx_reader_()
  // This method is called by the base class but has no per-channel effect
  // Input is handled by continuous RX reader in USB task
}

/**
 * Main loop - processes RX queue and handles TX multiplexing
 *
 * THREAD CONTEXT: Main loop
 */
void USBUartTypeCH934X::loop() {
  // CRITICAL: Process USB events (connect/disconnect/defer callbacks)
  USBClient::loop();

  // Process RX data queue (USB task → Main loop)
  UsbDataChunk *chunk;
  while ((chunk = this->usb_data_queue_.pop()) != nullptr) {
    auto *channel = chunk->channel;

#ifdef USE_UART_DEBUGGER
    if (channel->debug_) {
      std::string debug_prefix = channel->get_debug_prefix();
      uart::UARTDebug::log_hex(uart::UART_DIRECTION_RX, std::vector<uint8_t>(chunk->data, chunk->data + chunk->length),
                               ',', debug_prefix);
    }
#endif

    // Push data to channel's ring buffer (now safe in main loop)
    channel->input_buffer_.push(chunk->data, chunk->length);

    // Return chunk to pool for reuse
    this->chunk_pool_.release(chunk);
  }

  // Log dropped chunks periodically
  uint16_t dropped = this->usb_data_queue_.get_and_reset_dropped_count();
  if (dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u USB data chunks due to buffer overflow", dropped);
  }

  // Handle TX multiplexing - send data from channels
  this->handle_tx_multiplexing_();
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

/**
 * Called when USB device is connected
 */
void USBUartTypeCH934X::on_connected() {
  ESP_LOGI(TAG, "CH934X on_connected() called for device %d (VID=%04X, PID=%04X)", this->device_addr_, this->vid_,
           this->pid_);

  usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
    if (!status.success) {
      ESP_LOGE(TAG, "Control transfer failed, status=%s", esp_err_to_name(status.error_code));
    }
  };

  if (!this->parse_descriptors(this->device_handle_)) {
    ESP_LOGE(TAG, "CH934X parse_descriptors failed for device %d", this->device_addr_);
    this->status_set_error("No UART-Serial-Hub device found");
    this->disconnect();
    return;
  }

  ESP_LOGI(TAG, "Found a USB-Serial-Hub device");

  // Fix MPS for ESP32-S2/S3 (not needed for P4)
  fix_mps(this->uart_host_dev_.in_ep);
  fix_mps(this->uart_host_dev_.out_ep);
  fix_mps(this->uart_host_dev_.ep_cmd_read);
  fix_mps(this->uart_host_dev_.ep_cmd_write);

  // Allocate transferbuffers as soon as soon as endpoints mps is fix
  ESP_LOGD(TAG, "Allocating transfer buffers for device %d", this->device_addr_);
  for (size_t i = 0; i < USB_HOST_MAX_REQUESTS; i++) {
    usb_host_transfer_alloc(esphome::usb_host::USB_MAX_PACKET_SIZE, 0, &this->requests_[i].transfer);
    this->requests_[i].client = this;  // Set once, never changes
  }

  // Claim the interface
  auto err = usb_host_interface_claim(this->handle_, this->device_handle_, this->uart_host_dev_.data_interface, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "usb_host_interface_claim failed: %s, intf=%d", esp_err_to_name(err),
             this->uart_host_dev_.data_interface);
    this->status_set_error("usb_host_interface_claim failed");
    this->disconnect();
    return;
  }

  // Enable all channels
  this->enable_channels();
}

/**
 * Called when USB device is disconnected
 */
void USBUartTypeCH934X::on_disconnected() {
  // Stop RX and CMD readers
  this->rx_running_.store(false);
  this->cmd_running_.store(false);
  this->tx_in_progress_.store(false);

  // Clear all channel state
  for (auto *channel : this->channels_) {
    channel->initialised_.store(false);
    channel->input_started_.store(false);
    channel->output_started_.store(false);
    channel->input_buffer_.clear();
    channel->output_buffer_.clear();
  }

  // Halt and flush all endpoints
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

  // Release interface
  usb_host_interface_release(this->handle_, this->device_handle_, this->uart_host_dev_.data_interface);

  // Call base class cleanup
  USBClient::on_disconnected();
}

/**
 * Override start_output to add CH934X multiplexing header
 * CH934X requires: [port][len_lo][len_hi][data...]
 */
void USBUartTypeCH934X::start_output(USBUartChannel *channel) {
  // IMPORTANT: This function must only be called from the main loop!
  // The output_buffer_ is not thread-safe and can only be accessed from main loop.
  if (channel->output_started_.load()) {
    return;
  }
  if (channel->output_buffer_.is_empty()) {
    return;
  }

  const auto *ep = this->uart_host_dev_.out_ep;

  // Calculate maximum data size (leave room for 3-byte header: [port][len_lo][len_hi])
  size_t max_data_size = ep->wMaxPacketSize - TX_HEADER_SIZE;
  size_t available = channel->output_buffer_.get_available();
  size_t data_to_send = std::min(available, max_data_size);

  // Allocate buffer for TX packet on heap - must persist until transfer completes
  auto *tx_data = new uint8_t[ep->wMaxPacketSize];

  // Build TX packet: [port][len_lo][len_hi][data...]
  tx_data[0] = this->port_offset_ + channel->index_;
  tx_data[1] = data_to_send & 0xFF;
  tx_data[2] = (data_to_send >> 8) & 0xFF;

  // Copy data from output buffer
  channel->output_buffer_.pop(tx_data + TX_HEADER_SIZE, data_to_send);

#ifdef USE_UART_DEBUGGER
  if (channel->debug_) {
    std::string debug_prefix = channel->get_debug_prefix();
    uart::UARTDebug::log_hex(uart::UART_DIRECTION_TX,
                             std::vector<uint8_t>(tx_data + TX_HEADER_SIZE, tx_data + TX_HEADER_SIZE + data_to_send),
                             ',',
                             debug_prefix);  // NOLINT()
  }
#endif

  // CALLBACK CONTEXT: This lambda is executed in USB task via transfer_callback
  auto callback = [this, channel, tx_data](const usb_host::TransferStatus &status) {
    // Clean up the heap-allocated buffer after transfer completes
    delete[] tx_data;
    ESP_LOGV(TAG, "Output Transfer result: length: %u; status %X", status.data_len, status.error_code);
    channel->output_started_.store(false);
    // Defer restart to main loop (defer is thread-safe)
    this->defer([this, channel] { this->start_output(channel); });
  };
  channel->output_started_.store(true);
  this->transfer_out(ep->bEndpointAddress, callback, tx_data, data_to_send + TX_HEADER_SIZE);
  ESP_LOGV(TAG, "Output %d bytes started", data_to_send);
}

}  // namespace usb_uart
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
