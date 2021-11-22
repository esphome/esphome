#pragma once

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "ESP32CAN.h"

#ifdef USE_ESP32

CAN_device_t CAN_cfg;  // TODO enforce singleton aspect at compile time

namespace esphome {
namespace esp32_canbus {

class EspCanBus : public canbus::Canbus {
 public:
  EspCanBus(){};

 protected:
  float get_setup_priority() const override { return esphome::setup_priority::BUS; }

  // TODO on_shutdown?

  bool setup_internal() override {
    ESP32Can.CANStop();

    CAN_cfg.speed = CAN_SPEED_500KBPS;  // TODO
    CAN_cfg.tx_pin_id = GPIO_NUM_25;    // TODO
    CAN_cfg.rx_pin_id = GPIO_NUM_26;    // TODO
    const int rx_queue_size = 50;       // TODO

    CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
    int success = ESP32Can.CANInit();
    return (success == 0);
  }

  canbus::Error send_message(canbus::CanFrame *frame) { return canbus::ERROR_FAILTX; }

  canbus::Error read_message(canbus::CanFrame *frame) {
    CAN_frame_t rx_frame;

    // Receive next CAN frame from queue
    if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE) {
      frame->can_id = rx_frame.MsgID;
      frame->can_data_length_code = rx_frame.FIR.B.DLC;
      frame->use_extended_id = (rx_frame.FIR.B.FF != CAN_frame_std);
      frame->remote_transmission_request = (rx_frame.FIR.B.RTR == CAN_RTR);
      for (int i = 0; i < rx_frame.FIR.B.DLC; i++) {
        frame->data[i] = rx_frame.data.u8[i];
      }
      return canbus::ERROR_OK;
    } else {
      return canbus::ERROR_NOMSG;
    }
  }
};

}  // namespace esp32_canbus
}  // namespace esphome
#endif
