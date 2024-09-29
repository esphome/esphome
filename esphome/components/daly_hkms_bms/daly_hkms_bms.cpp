#include "daly_hkms_bms.h"
#include "daly_hkms_bms_registers.h"
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
  uint8_t modbus_request_address = this->daly_address_ + DALY_MODBUS_REQUEST_ADDRESS_OFFSET;
  this->parent_->send(
    modbus_request_address,
    MODBUS_CMD_READ_HOLDING_REGISTERS,
    0x00,
    DALY_MODBUS_REGISTER_COUNT,
    0,
    nullptr);

  this->last_send_ = millis();
}

void DalyHkmsBmsComponent::on_modbus_data(const std::vector<uint8_t> &data) {
  // Other components might be sending commands to our device. But we don't get called with enough
  // context to know what is what. So if we didn't do a send, we ignore the data.
  if (!this->last_send_)
    return;
  this->last_send_ = 0;

  // Also ignore the data if the message is too short. Otherwise we will publish invalid values.
  if (data.size() < DALY_MODBUS_REGISTER_COUNT * 2)
    return;

  auto get_register = [&](size_t i) -> uint16_t {
    return encode_uint16(data[i * 2], data[i * 2 + 1]);
  };

  auto publish_sensor_state = [&](sensor::Sensor *sensor, size_t i, int16_t offset, float factor,
                                  int32_t unavailable_value = -1) -> void {
    if (sensor == nullptr)
      return;
    uint16_t register_value = get_register(i);
    float value = register_value == unavailable_value ? NAN : (register_value + offset) * factor;
    sensor->publish_state(value);
  };

#ifdef USE_SENSOR
  publish_sensor_state(this->cell_1_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1     , 0, 0.001);
  publish_sensor_state(this->cell_2_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 1 , 0, 0.001);
  publish_sensor_state(this->cell_3_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 2 , 0, 0.001);
  publish_sensor_state(this->cell_4_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 3 , 0, 0.001);
  publish_sensor_state(this->cell_5_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 4 , 0, 0.001);
  publish_sensor_state(this->cell_6_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 5 , 0, 0.001);
  publish_sensor_state(this->cell_7_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 6 , 0, 0.001);
  publish_sensor_state(this->cell_8_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 7 , 0, 0.001);
  publish_sensor_state(this->cell_9_voltage_sensor_ , DALY_MODBUS_ADDR_CELL_VOLT_1 + 8 , 0, 0.001);
  publish_sensor_state(this->cell_10_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_1 + 9 , 0, 0.001);
  publish_sensor_state(this->cell_11_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_1 + 10, 0, 0.001);
  publish_sensor_state(this->cell_12_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_1 + 11, 0, 0.001);
  publish_sensor_state(this->cell_13_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_1 + 12, 0, 0.001);
  publish_sensor_state(this->cell_14_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_1 + 13, 0, 0.001);
  publish_sensor_state(this->cell_15_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_1 + 14, 0, 0.001);
  publish_sensor_state(this->cell_16_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_1 + 15, 0, 0.001);

  publish_sensor_state(this->temperature_1_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1    , -40, 1, 255);
  publish_sensor_state(this->temperature_2_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1 + 1, -40, 1, 255);
  publish_sensor_state(this->temperature_3_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1 + 2, -40, 1, 255);
  publish_sensor_state(this->temperature_4_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1 + 3, -40, 1, 255);
  publish_sensor_state(this->temperature_5_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1 + 4, -40, 1, 255);
  publish_sensor_state(this->temperature_6_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1 + 5, -40, 1, 255);
  publish_sensor_state(this->temperature_7_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1 + 6, -40, 1, 255);
  publish_sensor_state(this->temperature_8_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_1 + 7, -40, 1, 255);

  publish_sensor_state(this->voltage_sensor_, DALY_MODBUS_ADDR_VOLT, 0, 0.1);
  publish_sensor_state(this->current_sensor_, DALY_MODBUS_ADDR_CURR, -30000, 0.1);
  publish_sensor_state(this->battery_level_sensor_, DALY_MODBUS_ADDR_SOC, 0, 0.1);

  publish_sensor_state(this->cells_number_sensor_, DALY_MODBUS_ADDR_CELL_COUNT, 0, 1);
  publish_sensor_state(this->temps_number_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_COUNT, 0, 1);

  publish_sensor_state(this->max_cell_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_MAX, 0, 0.001);
  publish_sensor_state(this->max_cell_voltage_number_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_MAX_NUM, 0, 1);
  
  publish_sensor_state(this->min_cell_voltage_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_MIN, 0, 0.001);
  publish_sensor_state(this->min_cell_voltage_number_sensor_, DALY_MODBUS_ADDR_CELL_VOLT_MIN_NUM, 0, 1);

  publish_sensor_state(this->max_temperature_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_MAX, -40, 1, 255);
  publish_sensor_state(this->max_temperature_probe_number_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_MAX_NUM, 0, 1);
  
  publish_sensor_state(this->min_temperature_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_MIN, -40, 1, 255);
  publish_sensor_state(this->min_temperature_probe_number_sensor_, DALY_MODBUS_ADDR_CELL_TEMP_MIN_NUM, 0, 1);

  publish_sensor_state(this->remaining_capacity_sensor_, DALY_MODBUS_ADDR_REMAINING_CAPACITY, 0, 0.1);
  publish_sensor_state(this->cycles_sensor_, DALY_MODBUS_ADDR_CYCLES, 0, 1);

  publish_sensor_state(this->temperature_mos_sensor_, DALY_MODBUS_ADDR_MOS_TEMP, -40, 1, 255);
  publish_sensor_state(this->temperature_board_sensor_, DALY_MODBUS_ADDR_BOARD_TEMP, -40, 1, 255);
#endif

#ifdef USE_TEXT_SENSOR
  if (this->status_text_sensor_ != nullptr) {
    switch (get_register(DALY_MODBUS_ADDR_CHG_DSCHG_STATUS)) {
      case 0:
        this->status_text_sensor_->publish_state("Stationary");
        break;
      case 1:
        this->status_text_sensor_->publish_state("Charging");
        break;
      case 2:
        this->status_text_sensor_->publish_state("Discharging");
        break;
      default:
        break;
    }
  }
#endif

#ifdef USE_BINARY_SENSOR
  if (this->balancing_active_binary_sensor_) {
    this->balancing_active_binary_sensor_->publish_state(get_register(DALY_MODBUS_ADDR_BALANCE_STATUS) > 0);
  }
  if (this->charging_mos_enabled_binary_sensor_) {
    this->charging_mos_enabled_binary_sensor_->publish_state(get_register(DALY_MODBUS_ADDR_CHG_MOS_ACTIVE) > 0);
  }
  if (this->discharging_mos_enabled_binary_sensor_) {
    this->discharging_mos_enabled_binary_sensor_->publish_state(get_register(DALY_MODBUS_ADDR_DSCHG_MOS_ACTIVE) > 0);
  }
  if (this->precharging_mos_enabled_binary_sensor_) {
    this->precharging_mos_enabled_binary_sensor_->publish_state(get_register(DALY_MODBUS_ADDR_PRECHG_MOS_ACTIVE) > 0);
  }
#endif
}

void DalyHkmsBmsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "DALY HKMS BMS:");
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->daly_address_);
}

}  // namespace daly_hkms_bms
}  // namespace esphome
