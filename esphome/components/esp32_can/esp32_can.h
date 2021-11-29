#pragma once

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/defines.h"
#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"
#include "ESP32CAN.h"

#ifdef USE_ESP32

extern CAN_device_t CAN_cfg;  // TODO enforce singleton aspect at compile time

namespace esphome {
namespace esp32_can {

class Esp32Can : public canbus::Canbus {
 public:
  Esp32Can(){};
  void dump_config() override;
  void set_tx_pin(InternalGPIOPin *tx_pin) { this->tx_pin_ = tx_pin; }
  void set_rx_pin(InternalGPIOPin *rx_pin) { this->rx_pin_ = rx_pin; }
  void set_rx_buffer_size(size_t rx_buffer_size) { this->rx_buffer_size_ = rx_buffer_size; }

  void on_shutdown() override { ESP32Can.CANStop(); }

 protected:
   bool setup_internal() override;
   float get_setup_priority() const override {
     return esphome::setup_priority::BUS;
   }

  canbus::Error send_message(canbus::CanFrame *frame) {
    CAN_frame_t tx_frame;

    tx_frame.MsgID = frame->can_id;
    tx_frame.FIR.B.DLC = frame->can_data_length_code;
    tx_frame.FIR.B.FF = frame->use_extended_id ? CAN_frame_ext : CAN_frame_std;
    tx_frame.FIR.B.RTR = frame->remote_transmission_request ? CAN_RTR : CAN_no_RTR;

    for (int i = 0; i < frame->can_data_length_code; i++) {
      tx_frame.data.u8[i] = frame->data[i];
    }

    ESP32Can.CANWriteFrame(&tx_frame);

    return canbus::ERROR_OK;
  }

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

  InternalGPIOPin *tx_pin_;
  InternalGPIOPin *rx_pin_;
  size_t rx_buffer_size_;
};

}  // namespace esp32_can
}  // namespace esphome
#endif
