#include "daly_hkms_bms.h"
#include "esphome/core/log.h"

namespace esphome {
namespace daly_hkms_bms {

static const char *const TAG = "daly_hkms_bms";

// the DALY BMS only _kinda_ does Modbus. The device address is offset by 0x80 (so BMS #1 has address 0x81)
// which would be fine, however the Modbus address of the response has a different offset of 0x50,
// which makes this very much non-standard-compliant...
static const uint8_t DALY_MODBUS_REQUEST_ADDRESS_OFFSET = 0x80;
static const uint8_t DALY_MODBUS_RESPONSE_ADDRESS_OFFSET = 0x50;

static const uint8_t MODBUS_CMD_READ_HOLDING_REGISTERS = 0x03;

static const uint16_t MODBUS_ADDR_SUM_VOLT = 56;
static const uint16_t MODBUS_REGISTER_COUNT = 3;

void DalyHkmsBmsComponent::set_daly_address(uint8_t daly_address) {
  this->daly_address_ = daly_address;

  // set ModbusDevice address to the response address so the modbus component forwards
  // the response of this device to this component
  uint8_t modbus_response_address = daly_address + DALY_MODBUS_RESPONSE_ADDRESS_OFFSET;
  this->set_address(modbus_response_address);
}

void DalyHkmsBmsComponent::loop() {
  // If update() was unable to send we retry until we can send.
  if (!this->waiting_to_update_)
    return;
  update();
}

void DalyHkmsBmsComponent::update() {
  // If our last send has had no reply yet, and it wasn't that long ago, do nothing.
  uint32_t now = millis();
  if (now - this->last_send_ < this->get_update_interval() / 2) {
    return;
  }

  // The bus might be slow, or there might be other devices, or other components might be talking to our device.
  if (this->waiting_for_response()) {
    this->waiting_to_update_ = true;
    return;
  }

  this->waiting_to_update_ = false;

  // send the request using Modbus directly instead of ModbusDevice so we can send the data with the request address
  this->parent_->send(this->daly_address_ + DALY_MODBUS_REQUEST_ADDRESS_OFFSET, MODBUS_CMD_READ_HOLDING_REGISTERS, MODBUS_ADDR_SUM_VOLT, MODBUS_REGISTER_COUNT, 0, nullptr);

  this->last_send_ = millis();
}

void DalyHkmsBmsComponent::on_modbus_data(const std::vector<uint8_t> &data) {
  // Other components might be sending commands to our device. But we don't get called with enough
  // context to know what is what. So if we didn't do a send, we ignore the data.
  if (!this->last_send_)
    return;
  this->last_send_ = 0;

  // Also ignore the data if the message is too short. Otherwise we will publish invalid values.
  if (data.size() < MODBUS_REGISTER_COUNT * 2)
    return;

#ifdef USE_SENSOR
  if (this->voltage_sensor_) {
    float voltage = encode_uint16(data[0], data[1]) / 10.0;
    this->voltage_sensor_->publish_state(voltage);
  }
  
  if (this->current_sensor_) {
    float current = (encode_uint16(data[2], data[3]) - 30000) / 10.0;
    this->current_sensor_->publish_state(current);
  }

  if (this->battery_level_sensor_) {
    float current = encode_uint16(data[4], data[5]) / 10.0;
    this->battery_level_sensor_->publish_state(current);
  }
#endif
}

void DalyHkmsBmsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DALY HKMS BMS:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->daly_address_);
}

}  // namespace daly_hkms_bms
}  // namespace esphome
