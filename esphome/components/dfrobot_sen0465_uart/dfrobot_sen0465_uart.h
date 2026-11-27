#pragma once

#include "esphome/components/dfrobot_sen0465_base/dfrobot_sen0465_base.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/frame_reader.h"

namespace esphome::dfrobot_sen0465_uart {

class DFRobotSEN0465UARTComponent final : public dfrobot_sen0465_base::DFRobotSEN0465Component,
                                          public uart::UARTDevice {
 public:
  using DFRobotSEN0465Component::DFRobotSEN0465Component;

  void setup() override;
  void loop() override;

 protected:
  bool write_frame(const uint8_t *frame) override;
  void read_response() override;

  FrameReader<dfrobot_sen0465_base::FRAME_SIZE, dfrobot_sen0465_base::FRAME_HEAD> reader_;
};

}  // namespace esphome::dfrobot_sen0465_uart
