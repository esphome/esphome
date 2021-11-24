#include "esp32_can.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

CAN_device_t CAN_cfg;

namespace esphome {
namespace esp32_can {

static const char *const TAG = "esp32.can";

uint32_t from_canspeed_(canbus::CanSpeed can_speed) {
  uint32_t set;
  switch (can_speed) {
    case (canbus::CAN_5KBPS):  //   5Kbps
      set = 5;
      break;
    case (canbus::CAN_10KBPS):  //  10Kbps
      set = 10;
      break;
    case (canbus::CAN_20KBPS):  //  20Kbps
      set = 20;
      break;
    case (canbus::CAN_33KBPS):  //  33.333Kbps
      set = 33;
      break;
    case (canbus::CAN_40KBPS):  //  40Kbps
      set = 40;
      break;
    case (canbus::CAN_50KBPS):  //  50Kbps
      set = 50;
      break;
    case (canbus::CAN_80KBPS):  //  80Kbps
      set = 80;
      break;
    case (canbus::CAN_83K3BPS):  //  83.333Kbps
      set = 83;
      break;
    case (canbus::CAN_100KBPS):  // 100Kbps
      set = 100;
      break;
    case (canbus::CAN_125KBPS):  // 125Kbps
      set = 125;
      break;
    case (canbus::CAN_200KBPS):  // 200Kbps
      set = 200;
      break;
    case (canbus::CAN_250KBPS):  // 250Kbps
      set = 250;
      break;
    case (canbus::CAN_500KBPS):  // 500Kbps
      set = 500;
      break;
    case (canbus::CAN_1000KBPS):  //   1Mbps
      set = 100;
      break;
    default:
      ESP_LOGE(TAG, "  Invalid can speed: %d!", can_speed);
      set = 0;
      break;
  }

  return set;
}

bool EspCan::setup_internal() {
  CAN_cfg.speed = (CAN_speed_t) from_canspeed_(this->bit_rate_);
  CAN_cfg.tx_pin_id = gpio_num_t(this->tx_pin_->get_pin());
  CAN_cfg.rx_pin_id = gpio_num_t(this->rx_pin_->get_pin());
  CAN_cfg.rx_queue = xQueueCreate(this->rx_buffer_size_, sizeof(CAN_frame_t));
  int success = ESP32Can.CANInit();
  return (success == 0);
}

}  // namespace esp32_can
}  // namespace esphome

#endif  // USE_ESP32