#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace canbus {

class CanbusSensor {
 public:
  void set_can_id(int can_id) { this->can_id_ = can_id; }

 private:
  int can_id_{0};
};

class CanbusBinarySensor : public CanbusSensor, public binary_sensor::BinarySensor {
  friend class Canbus;
};

class Canbus : public Component {
 public:
  /* special address description flags for the CAN_ID */
  static const uint32_t CAN_EFF_FLAG = 0x80000000UL; /* EFF/SFF is set in the MSB */
  static const uint32_t CAN_RTR_FLAG = 0x40000000UL; /* remote transmission request */
  static const uint32_t CAN_ERR_FLAG = 0x20000000UL; /* error message frame */

  /* valid bits in CAN ID for frame formats */
  static const uint32_t CAN_SFF_MASK = 0x000007FFUL; /* standard frame format (SFF) */
  static const uint32_t CAN_EFF_MASK = 0x1FFFFFFFUL; /* extended frame format (EFF) */
  static const uint32_t CAN_ERR_MASK = 0x1FFFFFFFUL; /* omit EFF, RTR, ERR flags */
  /*
   * Controller Area Network Identifier structure
   *
   * bit 0-28 : CAN identifier (11/29 bit)
   * bit 29   : error message frame flag (0 = data frame, 1 = error message)
   * bit 30   : remote transmission request flag (1 = rtr frame)
   * bit 31   : frame format flag (0 = standard 11 bit, 1 = extended 29 bit)
   */
  typedef uint32_t canid_t;

  /* CAN payload length and DLC definitions according to ISO 11898-1 */
  static const uint8_t CAN_MAX_DLC = 8;
  static const uint8_t CAN_MAX_DLEN = 8;

  enum CAN_SPEED {
    CAN_5KBPS,
    CAN_10KBPS,
    CAN_20KBPS,
    CAN_31K25BPS,
    CAN_33KBPS,
    CAN_40KBPS,
    CAN_50KBPS,
    CAN_80KBPS,
    CAN_83K3BPS,
    CAN_95KBPS,
    CAN_100KBPS,
    CAN_125KBPS,
    CAN_200KBPS,
    CAN_250KBPS,
    CAN_500KBPS,
    CAN_1000KBPS
  };
  enum ERROR : uint8_t {
    ERROR_OK = 0,
    ERROR_FAIL = 1,
    ERROR_ALLTXBUSY = 2,
    ERROR_FAILINIT = 3,
    ERROR_FAILTX = 4,
    ERROR_NOMSG = 5
  };

  Canbus(){};
  Canbus(const std::string &name){};
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;

  void send(int can_id, uint8_t *data);
  void register_can_device(CanbusSensor *component){};
  void set_sender_id(int sender_id) { this->sender_id_ = sender_id; }

 protected:
  int sender_id_{0};
  virtual bool send_internal_(int can_id, uint8_t *data);
  virtual bool setup_internal_();
  virtual ERROR set_bitrate_(const CAN_SPEED canSpeed);
};
}  // namespace canbus
}  // namespace esphome