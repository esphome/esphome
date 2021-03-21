#include "esp32_can.h"
#include "esphome/core/log.h"

#include "CAN.h"

namespace esphome {
namespace esp32_can {

static const char *TAG = "esp32_can";

static int get_bitrate(CanSpeed bitrate) {
  switch (bitrate) {
    case CAN_5KBPS:
      return 5E3;
    case CAN_10KBPS:
      return 10E3;
    case CAN_20KBPS:
      return 20E3;
    case CAN_31K25BPS:
      return 31250;
    case CAN_33KBPS:
      return 33E3;
    case CAN_40KBPS:
      return 40E3;
    case CAN_50KBPS:
      return 50E3;
    case CAN_80KBPS:
      return 80E3;
    case CAN_83K3BPS:
      return 83300;
    case CAN_95KBPS:
      return 95E3;
    case CAN_100KBPS:
      return 100E3;
    case CAN_125KBPS:
      return 125E3;
    case CAN_200KBPS:
      return 200E3;
    case CAN_250KBPS:
      return 250E3;
    case CAN_500KBPS:
      return 500E3;
    case CAN_1000KBPS:
      return 1000E3;
    default:
      return -1;
  }
}

bool ESP32Can::setup_internal_() {
  int rx_pin = GPIO_NUM_13;
  int tx_pin = GPIO_NUM_22;
  CAN.setPins(rx_pin, tx_pin);

  int bitrate = get_bitrate(this->bit_rate_);
  if (bitrate <= 0) {
    return false;
  }
  if (!CAN.begin(bitrate)) {
    return false;
  }
  return true;
};

canbus::Error ESP32Can::send_message(struct canbus::CanFrame *frame) {
  if (frame->can_data_length_code > canbus::CAN_MAX_DATA_LENGTH) {
    return canbus::ERROR_FAILTX;
  }
  if (frame->use_extended_id) {
    CAN.beginPacket(frame->can_id, frame->can_data_length_code, frame->remote_transmission_request);
  } else {
    CAN.beginExtendedPacket(frame->can_id, frame->can_data_length_code, frame->remote_transmission_request);
  }
  if (!frame->remote_transmission_request) {
    CAN.write(frame->data, frame->can_data_length_code);
  }
  CAN.endPacket();

  return canbus::ERROR_OK;
}

canbus::Error ESP32Can::read_message(struct canbus::CanFrame *frame) {
  canbus::Error rc;
  int packetSize = CAN.parsePacket();

  if (packetSize) {
    frame->can_id = CAN.packetId();
    frame->use_extended_id = CAN.packetExtended();
    frame->remote_transmission_request = CAN.packetRtr());
    frame->can_data_length_code = CAN.packetDlc();

    if (!frame->remote_transmission_request) {
      uint8_t dlc = frame->can_data_length_code;
      uint8_t *data;
      while (dlc-- > 0) {
        *data++ = (char) CAN.read();
      }
    }
    return canbus::ERROR_OK;

  } else {
    return canbus::ERROR_NOMSG;
  }
}

}  // namespace esp32_can
}  // namespace esphome
