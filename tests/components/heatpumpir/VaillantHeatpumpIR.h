#pragma once

#include "HeatpumpIR.h"

class VaillantHeatpumpIR : public HeatpumpIR {
 public:
  using HeatpumpIR::send;

  void send(IRSender &, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, bool, bool) {}
};
