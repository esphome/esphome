#pragma once

#include "esphome/core/component.h"
#include "esphome/core/frame_reader.h"
#include "esphome/core/helpers.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome::pmsx003 {

static constexpr uint8_t START_CHARACTER_1 = 0x42;
static constexpr uint8_t START_CHARACTER_2 = 0x4D;
// PMS5003ST: 36 byte payload after the 2 start and 2 length bytes
static constexpr uint8_t MAX_FRAME_SIZE = 40;
// 42 4D, payload length (2, BE), payload ending in its checksum (2)
using PayloadLength = PayloadLengthU16BE<LengthField{.offset = 2}>;
/// The wrong-type warning, for a frame longer than any supported type sends
void warn_too_long(size_t len);
using PmsFrame = FrameFormat<FrameHeader<START_CHARACTER_1, START_CHARACTER_2>, PayloadLength,
                             WarnIfLongerThan<MAX_FRAME_SIZE, warn_too_long>>;

enum class Type : uint8_t {
  PMS1003 = 0,
  PMS3003,
  PMSX003,  // PMS5003, PMS6003, PMS7003, PMSA003 (NOT PMSA003I - see `pmsa003i` component)
  PMS5003S,
  PMS5003T,
  PMS5003ST,
  PMS9003M,
  LUFTMY,  // Luftmy LD10, LD11, LD13, LD15, LD16: standard PM + particle counts, no atmospheric PM values
};

enum class Command : uint8_t {
  MEASUREMENT_MODE = 0xE1,  // Data Options: `CMD_MEASUREMENT_MODE_PASSIVE`, `CMD_MEASUREMENT_MODE_ACTIVE`
  MANUAL_MEASUREMENT = 0xE2,
  SLEEP_MODE = 0xE4,  // Data Options: `CMD_SLEEP_MODE_SLEEP`, `CMD_SLEEP_MODE_WAKEUP`
};

enum class State : uint8_t {
  IDLE = 0,
  STABILISING,
  WAITING,
};

class PMSX003Component final : public uart::UARTDevice, public Component {
 public:
  PMSX003Component() = default;
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

  void set_type(Type type) { this->type_ = type; }

  // "Standard Particle"
  SUB_SENSOR(pm_1_0_std)
  SUB_SENSOR(pm_2_5_std)
  SUB_SENSOR(pm_10_0_std)

  // "Under Atmospheric Pressure"
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_10_0)

  // Particle counts by size
  SUB_SENSOR(pm_particles_03um)
  SUB_SENSOR(pm_particles_05um)
  SUB_SENSOR(pm_particles_10um)
  SUB_SENSOR(pm_particles_25um)
  SUB_SENSOR(pm_particles_50um)
  SUB_SENSOR(pm_particles_100um)

  // Formaldehyde
  SUB_SENSOR(formaldehyde)

  // Temperature and Humidity
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)

 protected:
  bool check_frame_(const uint8_t *frame, size_t total_size);
  void parse_data_(const uint8_t *frame);
  bool check_payload_length_(uint16_t payload_length);
  void send_command_(Command cmd, uint16_t data);

  Type type_;
  State state_{State::IDLE};
  bool initialised_{false};
  LengthFrameReader<MAX_FRAME_SIZE, PmsFrame> reader_;
  uint32_t fan_on_time_{0};
  uint32_t last_update_{0};
  uint32_t last_transmission_{0};
  uint32_t update_interval_{0};
};

}  // namespace esphome::pmsx003
