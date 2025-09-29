#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
#include "usb_uart.h"
#include "usb/usb_host.h"
#include "esphome/core/log.h"

#include "esphome/components/bytebuffer/bytebuffer.h"

namespace esphome {
namespace usb_uart {

using namespace bytebuffer;
/**
 * CH34x
 */

#if defined(USE_UART_DEBUGGER)
static std::string get_chiptype_string_(uint8_t enum_value) {
  std::string retval;
  switch (enum_value) {
    case 0:
	    retval = "CH342F";
      break;
    case 1:
	    retval = "CH342K";
      break;
    case 2:
      retval = "CH343GP";
      break;
    case 3:
      retval = "CH343G_AUTOBAUD";
      break;
    case 4:
      retval = "CH343K";
      break;
    case 5:
      retval = "CH343J";
      break;
    case 6:
      retval = "CH344L";
      break;
    case 7:
      retval = "CH344L_V2";
      break;
    case 8:
      retval = "CH344Q";
      break;
    case 9:
      retval = "CH347TF";
      break;
    case 10:
      retval = "CH9101UH";
      break;
    case 11:
      retval = "CH9101RY";
      break;
    case 12:
      retval = "CH9102F";
      break;
    case 13:
      retval = "CH9102X";
      break;
    case 14:
      retval = "CH9103M";
      break;
    case 15:
      retval = "CH9104L";
      break;
    case 16:
      retval = "CH340B";
      break;
    case 17:
      retval = "CH339W";
      break;
    case 18:
      retval = "CH9111L_M0";
      break;
    case 19:
      retval = "CH9111L_M1";
      break;
    case 20:
      retval = "CH9114L";
      break;
    case 21:
      retval = "CH9114W";
      break;
    case 22:
      retval = "CH9114F";
      break;
    case 23:
      retval = "CH346C_M0";
      break;
    case 24:
      retval = "CH346C_M1";
      break;
    case 25:
      retval = "CH346C_M2";
    break;
    case 255:
    default:
      retval = "unknown";
      break;
    }
  return retval;
}

void USBUartTypeCH34X::enum_chip_type_()
{
    uint8_t size = 8;
    std::vector<uint8_t> buffer = {0, 0, 0, 0, 0, 0, 0, 0};
    usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
      if (!status.success) {
        ESP_LOGE(TAG, "Control transfer for chiptype enumeration failed, status=%s", esp_err_to_name(status.error_code));
        return;
      }
    };
    auto res = this->control_transfer(USB_VENDOR_DEV | usb_host::USB_DIR_IN, 0x5F, 0, 0, callback, buffer);
    uint8_t chipver_ = buffer[0];
	  uint8_t chiptype = buffer[1];
		switch (this->pid_) {
	    case 0x55D2:
		    this->num_ports_ = 2;
		    if (chiptype == 0x41)
			    this->chiptype_ = CHIP_CH342K;
		    else
			    this->chiptype_ = CHIP_CH342F;
		    break;
	  case 0x55D3:
		  this->num_ports_ = 1;
		    if (chiptype == 0x02)
			    this->chiptype_ = CHIP_CH343J;
		    else if (chiptype_ == 0x01)
			    this->chiptype_ = CHIP_CH343K;
		    else if (chiptype_ == 0x18)
			    this->chiptype_ = CHIP_CH343G_AUTOBAUD;
		    else
			    this->chiptype_ = CHIP_CH343GP;
		    break;
	  case 0x55D4:
		  this->num_ports_ = 1;
		  if (chiptype == 0x09)
			  this->chiptype_ = CHIP_CH9102X;
		  else
			  this->chiptype_ = CHIP_CH9102F;
		  break;
	  case 0x55D5:
		  this->num_ports_ = 4;
		  if (chiptype == 0xC0) {
			  if ((chipver_ & 0xF0) == 0x40)
				  this->chiptype_ = CHIP_CH344L;
			  else
				  this->chiptype_ = CHIP_CH344L_V2;
		  } else
			  this->chiptype_ = CHIP_CH344Q;
		  break;
	  case 0x55D7:
		  this->num_ports_ = 2;
		  this->chiptype_ = CHIP_CH9103M;
		  break;
	  case 0x55D8:
		  this->num_ports_ = 1;
		  if (chiptype == 0x0A)
			  this->chiptype_ = CHIP_CH9101RY;
		  else
			  this->chiptype_ = CHIP_CH9101UH;
		  break;
	  case 0x55DB:
	  case 0x55DD:
		  this->num_ports_ = 1;
		  this->chiptype_ = CHIP_CH347TF;
		  break;
	  case 0x55DA:
	  case 0x55DE:
		  this->num_ports_ = 2;
		  this->chiptype_ = CHIP_CH347TF;
		  break;
	  case 0x55E7:
		  this->num_ports_ = 1;
		  this->chiptype_ = CHIP_CH339W;
		  break;
	  case 0x55DF:
		  this->num_ports_ = 4;
		  this->chiptype_ = CHIP_CH9104L;
		  break;
	  case 0x55E9:
		  this->num_ports_ = 1;
		  this->chiptype_ = CHIP_CH9111L_M0;
		  break;
	  case 0x55EA:
		  this->num_ports_ = 1;
		  this->chiptype_ = CHIP_CH9111L_M1;
		  break;
	  case 0x55E8:
		  this->num_ports_ = 4;
		  chiptype = buffer[2];
		  if (chiptype == 0x48)
			  this->chiptype_ = CHIP_CH9114L;
		  else if (chiptype_ == 0x49)
			  this->chiptype_ = CHIP_CH9114W;
		  else if (chiptype_ == 0x4A)
			  this->chiptype_ = CHIP_CH9114F;
		  break;
	  case 0x55EB:
		  this->num_ports_ = 1;
		if (buffer[4] & 0x01)
			this->chiptype_ = CHIP_CH346C_M1;
		else
			this->chiptype_ = CHIP_CH346C_M0;
		  break;
	case 0x55EC:
		this->num_ports_ = 2;
		this->chiptype_ = CHIP_CH346C_M2;
		break;
	default:
    this->num_ports_ = 0;
    this->chiptype_ = 255;
		break;
	}
  ESP_LOGD(TAG, "Found chip type %s with %u ports", get_chiptype_string_(this->chiptype_).c_str(), this->num_ports_);
}
#endif

/**
 * Hacky fix for some devices that report incorrect MPS values
 * @param ep The endpoint descriptor
 */
static void fix_mps(const usb_ep_desc_t *ep) {
  if (ep != nullptr) {
    auto *ep_mutable = const_cast<usb_ep_desc_t *>(ep);
    if (ep->wMaxPacketSize > 64) {
      ESP_LOGW(TAG, "Corrected MPS of EP 0x%02X from %u to 64", static_cast<uint8_t>(ep->bEndpointAddress & 0xFF),
               ep->wMaxPacketSize);
      ep_mutable->wMaxPacketSize = 64;
    }
  }
}
static void fix_mps_p4(const usb_ep_desc_t *ep, uint16_t mps) {
  if (ep != nullptr) {
    auto *ep_mutable = const_cast<usb_ep_desc_t *>(ep);
    if (ep->wMaxPacketSize > mps) {
      ESP_LOGW(TAG, "Corrected MPS of EP 0x%02X from %u to %u", static_cast<uint8_t>(ep->bEndpointAddress & 0xFF),
               ep->wMaxPacketSize, mps);
      ep_mutable->wMaxPacketSize = mps;
    }
  }
}
void USBUartTypeCH34X::on_connected() {
  usb_host::transfer_cb_t callback = [=](const usb_host::TransferStatus &status) {
      if (!status.success) {
        ESP_LOGE(TAG, "Control transfer failed, status=%s", esp_err_to_name(status.error_code));
      }
    };

  auto cdc_devs = this->parse_descriptors(this->device_handle_);
  if (cdc_devs.empty()) {
    this->status_set_error("No CDC-ACM device found");
    this->disconnect();
    return;
  }
  ESP_LOGD(TAG, "Found %zu CDC-ACM devices", cdc_devs.size());
  size_t i = 0;
  for (auto *channel : this->channels_) {
    if (i == cdc_devs.size()) {
      ESP_LOGE(TAG, "No configuration found for channel %d", channel->index_);
      this->status_set_warning("No configuration found for channel");
      break;
    }
    channel->cdc_dev_ = cdc_devs[i++];
#if defined(USE_ESP32_VARIANT_ESP32P4)
    fix_mps_p4(channel->cdc_dev_.in_ep, 512);
    fix_mps_p4(channel->cdc_dev_.out_ep, 512);
#else
    fix_mps(channel->cdc_dev_.in_ep);
    fix_mps(channel->cdc_dev_.out_ep);
#endif
    channel->initialised_.store(true);
    auto err =
        usb_host_interface_claim(this->handle_, this->device_handle_, channel->cdc_dev_.bulk_interface_number, 0);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "usb_host_interface_claim failed: %s, channel=%d, intf=%d", esp_err_to_name(err), channel->index_,
               channel->cdc_dev_.bulk_interface_number);
      this->status_set_error("usb_host_interface_claim failed");
      this->disconnect();
      return;
    }
  }
  this->enable_channels();
}

void USBUartTypeCH34X::enable_channels() {
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
  USBUartTypeCdcAcm::enable_channels();
}

}  // namespace usb_uart
}  // namespace esphome
#endif  // USE_ESP32_VARIANT_ESP32S2 || USE_ESP32_VARIANT_ESP32S3 || USE_ESP32_VARIANT_ESP32P4
