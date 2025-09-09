#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#define HEADER_LEN 40
#define MAGIC_SIZE 8
#define TLV_HEADER_LEN 8
#define PACKET_LENGTH_OFFSET 12
#define PACKET_LENGTH_SIZE 4
#define NUM_TLVS_OFFSET 32
#define NUM_TLV_SIZE 4
#define TLV_HEADER_SIZE 8
#define TLV_TYPE_TARGET_OBJECT_LIST 1010
#define TARGET_STRUCT_SIZE 112// id, posX, posY, posZ, velX, velY, velZ, accX, accY, accZ, ec[16], g, confidence

namespace esphome {
namespace iwr6843aop {

class IWR6843AOPComponent : public Component {
 public:
  // Add default constructor for when component is disabled
  IWR6843AOPComponent() : uart1_dev_(nullptr), uart2_dev_(nullptr) {}
  
  // Existing constructor
  IWR6843AOPComponent(uart::UARTComponent *uart1, uart::UARTComponent *uart2) : uart1_dev_(uart1), uart2_dev_(uart2) {}
  void setup() override;
  void loop() override;

  void set_float_input(const std::string &key, esphome::number::Number *number);
  void set_binary_sensor(const std::string &key, esphome::binary_sensor::BinarySensor *sensor);

  void cfg_iwr6843aop();
  void read_iwr6843aop_data();
  void parse_target_list_tlv(const std::vector<uint8_t> &tlv_payload);

 protected:
  uart::UARTComponent *uart1_dev_;
  uart::UARTComponent *uart2_dev_;
  uint32_t last_update_{0};

  esphome::number::Number *width_{nullptr};
  esphome::number::Number *length_{nullptr};

  esphome::binary_sensor::BinarySensor *room_presence_{nullptr};
  esphome::binary_sensor::BinarySensor *bed_presence_{nullptr};
};

}  // namespace iwr6843aop
}  // namespace esphome