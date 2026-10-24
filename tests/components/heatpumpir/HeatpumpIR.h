#pragma once

#include <cstdint>

#include "IRSender.h"

static constexpr uint8_t POWER_OFF = 0;
static constexpr uint8_t POWER_ON = 1;

static constexpr uint8_t MODE_AUTO = 1;
static constexpr uint8_t MODE_HEAT = 2;
static constexpr uint8_t MODE_COOL = 3;
static constexpr uint8_t MODE_DRY = 4;
static constexpr uint8_t MODE_FAN = 5;

static constexpr uint8_t FAN_AUTO = 0;
static constexpr uint8_t FAN_1 = 1;
static constexpr uint8_t FAN_2 = 2;
static constexpr uint8_t FAN_3 = 3;
static constexpr uint8_t FAN_4 = 4;
static constexpr uint8_t FAN_5 = 5;

static constexpr uint8_t VDIR_AUTO = 0;
static constexpr uint8_t VDIR_SWING = 1;
static constexpr uint8_t VDIR_UP = 2;
static constexpr uint8_t VDIR_MUP = 3;
static constexpr uint8_t VDIR_MIDDLE = 4;
static constexpr uint8_t VDIR_MDOWN = 5;
static constexpr uint8_t VDIR_DOWN = 6;

static constexpr uint8_t HDIR_AUTO = 0;
static constexpr uint8_t HDIR_SWING = 1;
static constexpr uint8_t HDIR_MIDDLE = 2;
static constexpr uint8_t HDIR_LEFT = 3;
static constexpr uint8_t HDIR_MLEFT = 4;
static constexpr uint8_t HDIR_MRIGHT = 5;
static constexpr uint8_t HDIR_RIGHT = 6;

class HeatpumpIR {
 public:
  HeatpumpIR() = default;
  virtual ~HeatpumpIR() = default;

  virtual void send(IRSender &, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) {}
  virtual void send(IRSender &, uint8_t) {}
};

#define DEFINE_NOOP_HEATPUMP_CLASS(name) \
  class name : public HeatpumpIR {};

DEFINE_NOOP_HEATPUMP_CLASS(AUXHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(BalluHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(CarrierMCAHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(CarrierNQVHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(DaikinHeatpumpARC417IR)
DEFINE_NOOP_HEATPUMP_CLASS(DaikinHeatpumpARC480A14IR)
DEFINE_NOOP_HEATPUMP_CLASS(DaikinHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(ElectroluxYALHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(FuegoHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(FujitsuHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(GreeYAAHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(GreeYANHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(GreeYACHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(GreeYTHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(HisenseHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(HitachiHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(HyundaiHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(IVTHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MideaHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiFAHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiFDHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiFEHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiHeavyFDTCHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiHeavyZJHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiHeavyZMHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiHeavyZMPHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiKJHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiMSCHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiMSYHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(MitsubishiSEZKDXXHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(PanasonicDKEHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(PanasonicEKEHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(PanasonicJKEHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(PanasonicLKEHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(PanasonicNKEHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(SamsungAQVHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(SamsungFJMHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(SharpHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(ToshibaDaiseikaiHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(ToshibaHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(ZHLT01HeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(NibeHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(Qlima1HeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(Qlima2HeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(SamsungAQV12MSANHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(ZHJG01HeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(AIRWAYHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(BGHHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(PanasonicAltDKEHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(PhilcoPHS32HeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(VaillantHeatpumpIR)
DEFINE_NOOP_HEATPUMP_CLASS(R51MHeatpumpIR)

#undef DEFINE_NOOP_HEATPUMP_CLASS

// Distinctive timings so a concatenated resend is obvious in the captured bursts.
class GreeGenericHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) override {
    ir.setFrequency(38);
    ir.mark(555);
    ir.space(666);
    ir.space(0);
  }
};

// greeyap sendBuffer walks 24 bytes in 8-byte groups and ends each group with space(0).
class GreeYAPHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) override {
    ir.setFrequency(38);
    for (int block = 1; block <= 3; block++) {
      ir.mark(1000 * block);
      ir.space(400 * block);
      ir.space(0);
    }
  }
};

// panasonic_ckp sends one message, delay(1000), then a second message; both end in space(0).
inline uint32_t panasonic_ckp_delay_ms = 0;

class PanasonicCKPHeatpumpIR : public HeatpumpIR {
 public:
  void send(IRSender &ir, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t) override {
    ir.setFrequency(38);
    ir.mark(111);
    ir.space(222);
    ir.space(0);
    panasonic_ckp_delay_ms += 1000;
    ir.setFrequency(38);
    ir.mark(333);
    ir.space(444);
    ir.space(0);
  }
};
