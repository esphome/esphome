#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/datatypes.h"

// https://www.belling.com.cn/media/file_object/bel_product/BL0906/datasheet/BL0906_V1.02_cn.pdf
// https://www.belling.com.cn/media/file_object/bel_product/BL0906/guide/BL0906%20APP%20Note_V1.02.pdf

namespace esphome::bl0906 {

// Stage values for the read state machine. After STAGE_CHANNEL_6 the state machine
// jumps to the two sentinel stages below, then to STAGE_IDLE which marks the cycle
// as complete and disables the loop.
enum BL0906Stage : uint8_t {
  STAGE_TEMP = 0,       // chip temperature
  STAGE_CHANNEL_1 = 1,  // per-phase current + power + energy
  STAGE_CHANNEL_2 = 2,
  STAGE_CHANNEL_3 = 3,
  STAGE_CHANNEL_4 = 4,
  STAGE_CHANNEL_5 = 5,
  STAGE_CHANNEL_6 = 6,
  STAGE_FREQ = UINT8_MAX - 2,   // frequency + voltage
  STAGE_POWER = UINT8_MAX - 1,  // total power + total energy
  STAGE_IDLE = UINT8_MAX,       // cycle complete
};

struct DataPacket {  // NOLINT(altera-struct-pack-align)
  uint8_t l{0};
  uint8_t m{0};
  uint8_t h{0};
  uint8_t checksum;  // checksum
  uint8_t address;
} __attribute__((packed));

struct ube24_t {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  uint8_t l{0};
  uint8_t m{0};
  uint8_t h{0};
} __attribute__((packed));

struct sbe24_t {  // NOLINT(readability-identifier-naming,altera-struct-pack-align)
  uint8_t l{0};
  uint8_t m{0};
  int8_t h{0};
} __attribute__((packed));

template<typename... Ts> class ResetEnergyAction;

class BL0906;

using ActionCallbackFuncPtr = void (BL0906::*)();

class BL0906 : public PollingComponent, public uart::UARTDevice {
  SUB_SENSOR(voltage)
  SUB_SENSOR(current_1)
  SUB_SENSOR(current_2)
  SUB_SENSOR(current_3)
  SUB_SENSOR(current_4)
  SUB_SENSOR(current_5)
  SUB_SENSOR(current_6)
  SUB_SENSOR(power_1)
  SUB_SENSOR(power_2)
  SUB_SENSOR(power_3)
  SUB_SENSOR(power_4)
  SUB_SENSOR(power_5)
  SUB_SENSOR(power_6)
  SUB_SENSOR(total_power)
  SUB_SENSOR(energy_1)
  SUB_SENSOR(energy_2)
  SUB_SENSOR(energy_3)
  SUB_SENSOR(energy_4)
  SUB_SENSOR(energy_5)
  SUB_SENSOR(energy_6)
  SUB_SENSOR(total_energy)
  SUB_SENSOR(frequency)
  SUB_SENSOR(temperature)

 public:
  void loop() override;

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  template<typename... Ts> friend class ResetEnergyAction;

  void reset_energy_();

  void read_data_(uint8_t address, float reference, sensor::Sensor *sensor);

  void bias_correction_(uint8_t address, float measurements, float correction);

  BL0906Stage current_stage_{STAGE_IDLE};
  void advance_stage_();
  size_t enqueue_action_(ActionCallbackFuncPtr function);
  void handle_actions_();

 private:
  std::vector<ActionCallbackFuncPtr> action_queue_{};
};

template<typename... Ts> class ResetEnergyAction : public Action<Ts...>, public Parented<BL0906> {
 public:
  void play(const Ts &...x) override { this->parent_->enqueue_action_(&BL0906::reset_energy_); }
};

}  // namespace esphome::bl0906
