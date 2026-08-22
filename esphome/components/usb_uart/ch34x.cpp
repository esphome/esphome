#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"

#include "esphome/components/bytebuffer/bytebuffer.h"

namespace esphome::usb_uart {

using namespace bytebuffer;

struct CH34xEntry {
  const char *name;
  uint16_t pid;
  uint8_t byte_idx;  // which status.data[] byte to inspect
  uint8_t mask;      // bitmask applied before comparison
  uint8_t match;     // 0xFF = wildcard (default/fallthrough for this PID)
  CH34xChipType chiptype;
  uint8_t num_ports;
};

static const CH34xEntry CH34X_TABLE[] = {
    {"CH342K", 0x55D2, 1, 0xFF, 0x41, CHIP_CH342K, 2},
    {"CH342F", 0x55D2, 1, 0xFF, 0xFF, CHIP_CH342F, 2},
    {"CH343J", 0x55D3, 1, 0xFF, 0x02, CHIP_CH343J, 1},
    {"CH343K", 0x55D3, 1, 0xFF, 0x01, CHIP_CH343K, 1},
    {"CH343G_AUTOBAUD", 0x55D3, 1, 0xFF, 0x18, CHIP_CH343G_AUTOBAUD, 1},
    {"CH343GP", 0x55D3, 1, 0xFF, 0xFF, CHIP_CH343GP, 1},
    {"CH9102X", 0x55D4, 1, 0xFF, 0x09, CHIP_CH9102X, 1},
    {"CH9102F", 0x55D4, 1, 0xFF, 0xFF, CHIP_CH9102F, 1},
    {"CH344L", 0x55D5, 1, 0xFF, 0xC0, CHIP_CH344L, 4},  // CH344L vs CH344L_V2 resolved below
    {"CH344Q", 0x55D5, 1, 0xFF, 0xFF, CHIP_CH344Q, 4},
    {"CH9103M", 0x55D7, 1, 0xFF, 0xFF, CHIP_CH9103M, 2},
    {"CH9101RY", 0x55D8, 1, 0xFF, 0x0A, CHIP_CH9101RY, 1},
    {"CH9101UH", 0x55D8, 1, 0xFF, 0xFF, CHIP_CH9101UH, 1},
    {"CH347TF", 0x55DB, 1, 0xFF, 0xFF, CHIP_CH347TF, 1},
    {"CH347TF", 0x55DD, 1, 0xFF, 0xFF, CHIP_CH347TF, 1},
    {"CH347TF", 0x55DA, 1, 0xFF, 0xFF, CHIP_CH347TF, 2},
    {"CH347TF", 0x55DE, 1, 0xFF, 0xFF, CHIP_CH347TF, 2},
    {"CH339W", 0x55E7, 1, 0xFF, 0xFF, CHIP_CH339W, 1},
    {"CH9104L", 0x55DF, 1, 0xFF, 0xFF, CHIP_CH9104L, 4},
    {"CH9111L_M0", 0x55E9, 1, 0xFF, 0xFF, CHIP_CH9111L_M0, 1},
    {"CH9111L_M1", 0x55EA, 1, 0xFF, 0xFF, CHIP_CH9111L_M1, 1},
    {"CH9114L", 0x55E8, 2, 0xFF, 0x48, CHIP_CH9114L, 4},
    {"CH9114W", 0x55E8, 2, 0xFF, 0x49, CHIP_CH9114W, 4},
    {"CH9114F", 0x55E8, 2, 0xFF, 0x4A, CHIP_CH9114F, 4},
    {"CH346C_M1", 0x55EB, 4, 0x01, 0x01, CHIP_CH346C_M1, 1},
    {"CH346C_M0", 0x55EB, 4, 0x01, 0xFF, CHIP_CH346C_M0, 1},
    {"CH346C_M2", 0x55EC, 1, 0xFF, 0xFF, CHIP_CH346C_M2, 2},
};

bool USBUartTypeCH34X::config_device_step(uint8_t step, bool ok, const uint8_t *response) {
  if (step == 0) {
    // Vendor-specific GET_CHIP_VERSION request (bRequest=0x5F): returns chip ID bytes
    // used to distinguish CH34x variants sharing the same PID.
    this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_IN, 0x5F, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0});
    return true;
  }
  // step 1: parse the chip-version response (falling back to "unknown" on failure).
  if (!ok) {
    ESP_LOGE(TAG, "CH34x chip detection failed");
    return false;
  }
  CH34xChipType chiptype = CHIP_UNKNOWN;
  uint8_t num_ports = 1;
  for (const auto &e : CH34X_TABLE) {
    if (e.pid != this->pid_)
      continue;
    if (e.match != 0xFF && (response[e.byte_idx] & e.mask) != e.match)
      continue;
    chiptype = e.chiptype;
    num_ports = e.num_ports;
    break;
  }
  // CH344L vs CH344L_V2 requires chipver (data[0]) in addition to chiptype (data[1])
  if (chiptype == CHIP_CH344L && (response[0] & 0xF0) != 0x40)
    chiptype = CHIP_CH344L_V2;
  const char *name = "unknown";
  for (const auto &e : CH34X_TABLE) {
    if (e.chiptype == chiptype) {
      name = e.name;
      break;
    }
  }
  this->chiptype_ = chiptype;
  this->chip_name_ = name;
  this->num_ports_ = num_ports;
  ESP_LOGD(TAG, "CH34x chip: %s, ports: %u", name, this->num_ports_);
  return false;
}

void USBUartTypeCH34X::dump_config() {
  USBUartTypeCdcAcm::dump_config();
  ESP_LOGCONFIG(TAG, "  CH34x chip: %s", this->chip_name_);
}

bool USBUartTypeCH34X::config_step(USBUartChannel *channel, uint8_t step, bool reload, bool ok,
                                   const uint8_t *response) {
  uint8_t cmd = 0xA1 + channel->index_;
  if (channel->index_ >= 2)
    cmd += 0xE;
  switch (step) {
    case 0: {
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
        return false;
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
      this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, cmd, value, (factor << 8) | divisor);
      return true;
    }
    case 1:
      this->config_transfer_(USB_VENDOR_DEV | usb_host::USB_DIR_OUT, cmd + 3, 0x80, 0);
      return true;
    default:
      return false;
  }
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

#endif  // USE_ESP32_VARIANT_ESP32P4 || USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 ||
        // USE_ESP32_VARIANT_ESP32S31 || USE_ESP32_VARIANT_ESP32H4
