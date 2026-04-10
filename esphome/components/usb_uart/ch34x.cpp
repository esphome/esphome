#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"

#include "esphome/components/bytebuffer/bytebuffer.h"

namespace esphome::usb_uart {

using namespace bytebuffer;

struct CH34xEntry {
  uint16_t pid;
  uint8_t byte_idx;  // which status.data[] byte to inspect
  uint8_t mask;      // bitmask applied before comparison
  uint8_t match;     // 0xFF = wildcard (default/fallthrough for this PID)
  CH34xChipType chiptype;
  const char *name;
  uint8_t num_ports;
};

static const CH34xEntry CH34X_TABLE[] = {
    {0x55D2, 1, 0xFF, 0x41, CHIP_CH342K, "CH342K", 2},
    {0x55D2, 1, 0xFF, 0xFF, CHIP_CH342F, "CH342F", 2},
    {0x55D3, 1, 0xFF, 0x02, CHIP_CH343J, "CH343J", 1},
    {0x55D3, 1, 0xFF, 0x01, CHIP_CH343K, "CH343K", 1},
    {0x55D3, 1, 0xFF, 0x18, CHIP_CH343G_AUTOBAUD, "CH343G_AUTOBAUD", 1},
    {0x55D3, 1, 0xFF, 0xFF, CHIP_CH343GP, "CH343GP", 1},
    {0x55D4, 1, 0xFF, 0x09, CHIP_CH9102X, "CH9102X", 1},
    {0x55D4, 1, 0xFF, 0xFF, CHIP_CH9102F, "CH9102F", 1},
    {0x55D5, 1, 0xFF, 0xC0, CHIP_CH344L, "CH344L", 4},  // CH344L vs CH344L_V2 resolved below
    {0x55D5, 1, 0xFF, 0xFF, CHIP_CH344Q, "CH344Q", 4},
    {0x55D7, 1, 0xFF, 0xFF, CHIP_CH9103M, "CH9103M", 2},
    {0x55D8, 1, 0xFF, 0x0A, CHIP_CH9101RY, "CH9101RY", 1},
    {0x55D8, 1, 0xFF, 0xFF, CHIP_CH9101UH, "CH9101UH", 1},
    {0x55DB, 1, 0xFF, 0xFF, CHIP_CH347TF, "CH347TF", 1},
    {0x55DD, 1, 0xFF, 0xFF, CHIP_CH347TF, "CH347TF", 1},
    {0x55DA, 1, 0xFF, 0xFF, CHIP_CH347TF, "CH347TF", 2},
    {0x55DE, 1, 0xFF, 0xFF, CHIP_CH347TF, "CH347TF", 2},
    {0x55E7, 1, 0xFF, 0xFF, CHIP_CH339W, "CH339W", 1},
    {0x55DF, 1, 0xFF, 0xFF, CHIP_CH9104L, "CH9104L", 4},
    {0x55E9, 1, 0xFF, 0xFF, CHIP_CH9111L_M0, "CH9111L_M0", 1},
    {0x55EA, 1, 0xFF, 0xFF, CHIP_CH9111L_M1, "CH9111L_M1", 1},
    {0x55E8, 2, 0xFF, 0x48, CHIP_CH9114L, "CH9114L", 4},
    {0x55E8, 2, 0xFF, 0x49, CHIP_CH9114W, "CH9114W", 4},
    {0x55E8, 2, 0xFF, 0x4A, CHIP_CH9114F, "CH9114F", 4},
    {0x55EB, 4, 0x01, 0x01, CHIP_CH346C_M1, "CH346C_M1", 1},
    {0x55EB, 4, 0x01, 0xFF, CHIP_CH346C_M0, "CH346C_M0", 1},
    {0x55EC, 1, 0xFF, 0xFF, CHIP_CH346C_M2, "CH346C_M2", 2},
};

void USBUartTypeCH34X::enable_channels() {
  usb_host::transfer_cb_t cb = [this](const usb_host::TransferStatus &status) {
    if (!status.success) {
      this->defer([this, error_code = status.error_code]() {
        ESP_LOGE(TAG, "CH34x chip detection failed: %s", esp_err_to_name(error_code));
        this->apply_line_settings_();
      });
      return;
    }
    CH34xChipType chiptype = CHIP_UNKNOWN;
    uint8_t num_ports = 1;
    for (const auto &e : CH34X_TABLE) {
      if (e.pid != this->pid_)
        continue;
      if (e.match != 0xFF && (status.data[e.byte_idx] & e.mask) != e.match)
        continue;
      chiptype = e.chiptype;
      num_ports = e.num_ports;
      break;
    }
    // CH344L vs CH344L_V2 requires chipver (data[0]) in addition to chiptype (data[1])
    if (chiptype == CHIP_CH344L && (status.data[0] & 0xF0) != 0x40)
      chiptype = CHIP_CH344L_V2;
    const char *name = "unknown";
    for (const auto &e : CH34X_TABLE) {
      if (e.chiptype == chiptype) {
        name = e.name;
        break;
      }
    }
    this->defer([this, chiptype, num_ports, name]() {
      this->chiptype_ = chiptype;
      this->chip_name_ = name;
      this->num_ports_ = num_ports;
      ESP_LOGD(TAG, "CH34x chip: %s, ports: %u", name, this->num_ports_);
      this->apply_line_settings_();
    });
  };
  // Vendor-specific GET_CHIP_VERSION request (bRequest=0x5F): returns chip ID bytes
  // used to distinguish CH34x variants sharing the same PID.
  this->control_transfer(USB_VENDOR_DEV | usb_host::USB_DIR_IN, 0x5F, 0, 0, cb, {0, 0, 0, 0, 0, 0, 0, 0});
}

void USBUartTypeCH34X::dump_config() {
  USBUartTypeCdcAcm::dump_config();
  ESP_LOGCONFIG(TAG, "  CH34x chip: %s", this->chip_name_);
}

void USBUartTypeCH34X::apply_line_settings_() {
  for (auto *channel : this->channels_) {
    if (!channel->initialised_.load())
      continue;
    usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
      if (!status.success) {
        ESP_LOGE(TAG, "Control transfer failed, status=%s", esp_err_to_name(status.error_code));
        channel->initialised_.store(false);
      }
    };

    uint8_t divisor = 7;
    uint32_t clk = 12000000;

    auto baud_rate = channel->baud_rate_;
    if (baud_rate < 256000) {
      if (baud_rate > 6000000 / 255) {
        divisor = 3;
        clk = 6000000;
      } else if (baud_rate > 750000 / 255) {
        divisor = 2;
        clk = 750000;
      } else if (baud_rate > 93750 / 255) {
        divisor = 1;
        clk = 93750;
      } else {
        divisor = 0;
        clk = 11719;
      }
    }
    ESP_LOGV(TAG, "baud_rate: %" PRIu32 ", divisor: %d, clk: %" PRIu32, baud_rate, divisor, clk);
    auto factor = static_cast<uint8_t>(clk / baud_rate);
    if (factor == 0 || factor == 0xFF) {
      ESP_LOGE(TAG, "Invalid baud rate %" PRIu32, baud_rate);
      channel->initialised_.store(false);
      continue;
    }
    if ((clk / factor - baud_rate) > (baud_rate - clk / (factor + 1)))
      factor++;
    factor = 256 - factor;

    uint16_t value = 0xC0;
    if (channel->stop_bits_ == UART_CONFIG_STOP_BITS_2)
      value |= 4;
    switch (channel->parity_) {
      case UART_CONFIG_PARITY_NONE:
        break;
      default:
        value |= 8 | ((channel->parity_ - 1) << 4);
        break;
    }
    value |= channel->data_bits_ - 5;
    value <<= 8;
    value |= 0x8C;
    uint8_t cmd = 0xA1 + channel->index_;
    if (channel->index_ >= 2)
      cmd += 0xE;
    this->control_transfer(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, cmd, value, (factor << 8) | divisor, callback);
    this->control_transfer(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, cmd + 3, 0x80, 0, callback);
  }
  this->start_channels();
}

std::vector<CdcEps> USBUartTypeCH34X::parse_descriptors(usb_device_handle_t dev_hdl) {
  auto result = USBUartTypeCdcAcm::parse_descriptors(dev_hdl);
  // ch34x doesn't use the interrupt endpoint, and we don't have endpoints to spare
  for (auto &cdc_dev : result) {
    cdc_dev.interrupt_interface_number = 0xFF;
  }
  return result;
}
}  // namespace esphome::usb_uart

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3
