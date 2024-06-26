#include "daly_bms.h"
#include <vector>
#include "esphome/core/log.h"

namespace esphome {
namespace daly_bms {

static const char *const TAG = "daly_bms";

static const uint8_t DALY_FRAME_SIZE = 13;
static const uint8_t DALY_TEMPERATURE_OFFSET = 40;
static const uint16_t DALY_CURRENT_OFFSET = 30000;

static const uint8_t DALY_REQUEST_BATTERY_LEVEL = 0x90;
static const uint8_t DALY_REQUEST_MIN_MAX_VOLTAGE = 0x91;
static const uint8_t DALY_REQUEST_MIN_MAX_TEMPERATURE = 0x92;
static const uint8_t DALY_REQUEST_MOS = 0x93;
static const uint8_t DALY_REQUEST_STATUS = 0x94;
static const uint8_t DALY_REQUEST_CELL_VOLTAGE = 0x95;
static const uint8_t DALY_REQUEST_TEMPERATURE = 0x96;

void DalyBmsComponent::setup() { this->next_request_ = 1; }

void DalyBmsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Daly BMS:");
  this->check_uart_settings(9600);
}

void DalyBmsComponent::update() {
  this->trigger_next_ = true;
  this->next_request_ = 0;
}

void DalyBmsComponent::loop() {
  const uint32_t now = millis();
  if (this->receiving_ && (now - this->last_transmission_ >= 200)) {
    // last transmission too long ago. Reset RX index.
    ESP_LOGW(TAG, "Last transmission too long ago. Reset RX index.");
    this->data_.clear();
    this->receiving_ = false;
  }
  if ((now - this->last_transmission_ >= 250) && !this->trigger_next_) {
    // last transmittion longer than 0.25s ago -> trigger next request
    this->last_transmission_ = now;
    this->trigger_next_ = true;
  }
  if (available())
    this->last_transmission_ = now;
  while (available()) {
    uint8_t c;
    read_byte(&c);
    if (!this->receiving_) {
      if (c != 0xa5)
        continue;
      this->receiving_ = true;
    }
    this->data_.push_back(c);
    if (this->data_.size() == 4)
      this->data_count_ = c;
    if ((this->data_.size() > 4) and (data_.size() == this->data_count_ + 5)) {
      this->decode_data_(this->data_);
      this->data_.clear();
      this->receiving_ = false;
    }
  }

  if (this->trigger_next_) {
    this->trigger_next_ = false;
    switch (this->next_request_) {
      case 0:
        this->request_data_(DALY_REQUEST_BATTERY_LEVEL);
        this->next_request_ = 1;
        break;
      case 1:
        this->request_data_(DALY_REQUEST_MIN_MAX_VOLTAGE);
        this->next_request_ = 2;
        break;
      case 2:
        this->request_data_(DALY_REQUEST_MIN_MAX_TEMPERATURE);
        this->next_request_ = 3;
        break;
      case 3:
        this->request_data_(DALY_REQUEST_MOS);
        this->next_request_ = 4;
        break;
      case 4:
        this->request_data_(DALY_REQUEST_STATUS);
        this->next_request_ = 5;
        break;
      case 5:
        this->request_data_(DALY_REQUEST_CELL_VOLTAGE);
        this->next_request_ = 6;
        break;
      case 6:
        this->request_data_(DALY_REQUEST_TEMPERATURE);
        this->next_request_ = 7;
        break;
      case 7:
      default:
        break;
    }
  }
}

float DalyBmsComponent::get_setup_priority() const { return setup_priority::DATA; }

void DalyBmsComponent::request_data_(uint8_t data_id) {
  uint8_t request_message[DALY_FRAME_SIZE];

  request_message[0] = 0xA5;         // Start Flag
  request_message[1] = this->addr_;  // Communication Module Address
  request_message[2] = data_id;      // Data ID
  request_message[3] = 0x08;         // Data Length (Fixed)
  request_message[4] = 0x00;         // Empty Data
  request_message[5] = 0x00;         //     |
  request_message[6] = 0x00;         //     |
  request_message[7] = 0x00;         //     |
  request_message[8] = 0x00;         //     |
  request_message[9] = 0x00;         //     |
  request_message[10] = 0x00;        //     |
  request_message[11] = 0x00;        // Empty Data

  request_message[12] = (uint8_t) (request_message[0] + request_message[1] + request_message[2] +
                                   request_message[3]);  // Checksum (Lower byte of the other bytes sum)

  ESP_LOGV(TAG, "Request datapacket Nr %x", data_id);
  this->write_array(request_message, sizeof(request_message));
  this->flush();
}

void DalyBmsComponent::decode_data_(std::vector<uint8_t> data) {
  auto it = data.begin();

  while ((it = std::find(it, data.end(), 0xA5)) != data.end()) {
    if (data.end() - it >= DALY_FRAME_SIZE && it[1] == 0x01) {
      uint8_t checksum;
      int sum = 0;
      for (int i = 0; i < 12; i++) {
        sum += it[i];
      }
      checksum = sum;

      if (checksum == it[12]) {
        switch (it[2]) {
#ifdef USE_SENSOR
          case DALY_REQUEST_BATTERY_LEVEL:
            if (this->voltage_sensor_) {
              this->voltage_sensor_->publish_state((float) encode_uint16(it[4], it[5]) / 10);
            }
            if (this->current_sensor_) {
              this->current_sensor_->publish_state(((float) (encode_uint16(it[8], it[9]) - DALY_CURRENT_OFFSET) / 10));
            }
            if (this->battery_level_sensor_) {
              this->battery_level_sensor_->publish_state((float) encode_uint16(it[10], it[11]) / 10);
            }
            break;

          case DALY_REQUEST_MIN_MAX_VOLTAGE:
            if (this->max_cell_voltage_sensor_) {
              this->max_cell_voltage_sensor_->publish_state((float) encode_uint16(it[4], it[5]) / 1000);
            }
            if (this->max_cell_voltage_number_sensor_) {
              this->max_cell_voltage_number_sensor_->publish_state(it[6]);
            }
            if (this->min_cell_voltage_sensor_) {
              this->min_cell_voltage_sensor_->publish_state((float) encode_uint16(it[7], it[8]) / 1000);
            }
            if (this->min_cell_voltage_number_sensor_) {
              this->min_cell_voltage_number_sensor_->publish_state(it[9]);
            }
            break;

          case DALY_REQUEST_MIN_MAX_TEMPERATURE:
            if (this->max_temperature_sensor_) {
              this->max_temperature_sensor_->publish_state(it[4] - DALY_TEMPERATURE_OFFSET);
            }
            if (this->max_temperature_probe_number_sensor_) {
              this->max_temperature_probe_number_sensor_->publish_state(it[5]);
            }
            if (this->min_temperature_sensor_) {
              this->min_temperature_sensor_->publish_state(it[6] - DALY_TEMPERATURE_OFFSET);
            }
            if (this->min_temperature_probe_number_sensor_) {
              this->min_temperature_probe_number_sensor_->publish_state(it[7]);
            }
            break;
#endif
          case DALY_REQUEST_MOS:
#ifdef USE_TEXT_SENSOR
            if (this->status_text_sensor_ != nullptr) {
              switch (it[4]) {
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
            if (this->charging_mos_enabled_binary_sensor_) {
              this->charging_mos_enabled_binary_sensor_->publish_state(it[5]);
            }
            if (this->discharging_mos_enabled_binary_sensor_) {
              this->discharging_mos_enabled_binary_sensor_->publish_state(it[6]);
            }
#endif
#ifdef USE_SENSOR
            if (this->remaining_capacity_sensor_) {
              this->remaining_capacity_sensor_->publish_state((float) encode_uint32(it[8], it[9], it[10], it[11]) /
                                                              1000);
            }
#endif
            break;

#ifdef USE_SENSOR
          case DALY_REQUEST_STATUS:
            if (this->cells_number_sensor_) {
              this->cells_number_sensor_->publish_state(it[4]);
            }
            break;

          case DALY_REQUEST_TEMPERATURE:
            if (it[4] == 1) {
              if (this->temperature_1_sensor_) {
                this->temperature_1_sensor_->publish_state(it[5] - DALY_TEMPERATURE_OFFSET);
              }
              if (this->temperature_2_sensor_) {
                this->temperature_2_sensor_->publish_state(it[6] - DALY_TEMPERATURE_OFFSET);
              }
            }
            break;

          case DALY_REQUEST_CELL_VOLTAGE:
            switch (it[4]) {
              case 1:
                if (this->cell_1_voltage_sensor_) {
                  this->cell_1_voltage_sensor_->publish_state((float) encode_uint16(it[5], it[6]) / 1000);
                }
                if (this->cell_2_voltage_sensor_) {
                  this->cell_2_voltage_sensor_->publish_state((float) encode_uint16(it[7], it[8]) / 1000);
                }
                if (this->cell_3_voltage_sensor_) {
                  this->cell_3_voltage_sensor_->publish_state((float) encode_uint16(it[9], it[10]) / 1000);
                }
                break;
              case 2:
                if (this->cell_4_voltage_sensor_) {
                  this->cell_4_voltage_sensor_->publish_state((float) encode_uint16(it[5], it[6]) / 1000);
                }
                if (this->cell_5_voltage_sensor_) {
                  this->cell_5_voltage_sensor_->publish_state((float) encode_uint16(it[7], it[8]) / 1000);
                }
                if (this->cell_6_voltage_sensor_) {
                  this->cell_6_voltage_sensor_->publish_state((float) encode_uint16(it[9], it[10]) / 1000);
                }
                break;
              case 3:
                if (this->cell_7_voltage_sensor_) {
                  this->cell_7_voltage_sensor_->publish_state((float) encode_uint16(it[5], it[6]) / 1000);
                }
                if (this->cell_8_voltage_sensor_) {
                  this->cell_8_voltage_sensor_->publish_state((float) encode_uint16(it[7], it[8]) / 1000);
                }
                if (this->cell_9_voltage_sensor_) {
                  this->cell_9_voltage_sensor_->publish_state((float) encode_uint16(it[9], it[10]) / 1000);
                }
                break;
              case 4:
                if (this->cell_10_voltage_sensor_) {
                  this->cell_10_voltage_sensor_->publish_state((float) encode_uint16(it[5], it[6]) / 1000);
                }
                if (this->cell_11_voltage_sensor_) {
                  this->cell_11_voltage_sensor_->publish_state((float) encode_uint16(it[7], it[8]) / 1000);
                }
                if (this->cell_12_voltage_sensor_) {
                  this->cell_12_voltage_sensor_->publish_state((float) encode_uint16(it[9], it[10]) / 1000);
                }
                break;
              case 5:
                if (this->cell_13_voltage_sensor_) {
                  this->cell_13_voltage_sensor_->publish_state((float) encode_uint16(it[5], it[6]) / 1000);
                }
                if (this->cell_14_voltage_sensor_) {
                  this->cell_14_voltage_sensor_->publish_state((float) encode_uint16(it[7], it[8]) / 1000);
                }
                if (this->cell_15_voltage_sensor_) {
                  this->cell_15_voltage_sensor_->publish_state((float) encode_uint16(it[9], it[10]) / 1000);
                }
                break;
              case 6:
                if (this->cell_16_voltage_sensor_) {
                  this->cell_16_voltage_sensor_->publish_state((float) encode_uint16(it[5], it[6]) / 1000);
                }
                break;
            }
            break;
#endif
          default:
            break;
        }
      } else {
        ESP_LOGW(TAG, "Checksum-Error on Packet %x", it[4]);
      }
      std::advance(it, DALY_FRAME_SIZE);
    } else {
      std::advance(it, 1);
    }
  }
}

}  // namespace daly_bms
}  // namespace esphome
