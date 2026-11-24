#include "daly_bms.h"
#include <algorithm>
#include <vector>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::daly_bms {

ESPHOME_LOG_TAG(TAG, "daly_bms");

static const uint8_t DALY_FRAME_SIZE = 13;
static const uint8_t DALY_TEMPERATURE_OFFSET = 40;
static const uint16_t DALY_CURRENT_OFFSET = 30000;

// One cycle sends these requests in this order, the request at position N as DALY_REQUEST_BATTERY_LEVEL + N
static constexpr uint8_t DALY_REQUEST_BATTERY_LEVEL = 0x90;
static constexpr uint8_t DALY_REQUEST_MIN_MAX_VOLTAGE = 0x91;
static constexpr uint8_t DALY_REQUEST_MIN_MAX_TEMPERATURE = 0x92;
static constexpr uint8_t DALY_REQUEST_MOS = 0x93;
static constexpr uint8_t DALY_REQUEST_STATUS = 0x94;
static constexpr uint8_t DALY_REQUEST_CELL_VOLTAGE = 0x95;
static constexpr uint8_t DALY_REQUEST_TEMPERATURE = 0x96;
static_assert(DALY_REQUEST_MIN_MAX_VOLTAGE == DALY_REQUEST_BATTERY_LEVEL + 1 &&
                  DALY_REQUEST_MIN_MAX_TEMPERATURE == DALY_REQUEST_BATTERY_LEVEL + 2 &&
                  DALY_REQUEST_MOS == DALY_REQUEST_BATTERY_LEVEL + 3 &&
                  DALY_REQUEST_STATUS == DALY_REQUEST_BATTERY_LEVEL + 4 &&
                  DALY_REQUEST_CELL_VOLTAGE == DALY_REQUEST_BATTERY_LEVEL + 5 &&
                  DALY_REQUEST_TEMPERATURE == DALY_REQUEST_BATTERY_LEVEL + 6,
              "the request cycle needs consecutive request IDs in sending order");

static constexpr uint8_t DALY_REQUEST_COUNT = DALY_REQUEST_TEMPERATURE - DALY_REQUEST_BATTERY_LEVEL + 1;
// A response can span several frames (three cell voltages per frame). Once a frame is complete and the line
// stays quiet this long, the response is over and the next request can go out.
static constexpr uint32_t DALY_RESPONSE_GAP_MS = 50;
// Without a response, the next request goes out after this time.
static constexpr uint32_t DALY_RESPONSE_TIMEOUT_MS = 250;

void DalyBmsComponent::dump_config() { ESP_LOGCONFIG(TAG, "Daly BMS:"); }

void DalyBmsComponent::update() {
  // A cycle that is still running (update interval shorter than a cycle) finishes first, or the last requests
  // (cell voltages, temperatures) would never be sent. The new cycle then follows right after its last response.
  this->cycle_requested_ = true;
  if (!this->cycle_running_)
    this->trigger_next_ = true;
}

void DalyBmsComponent::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  const size_t avail = this->available();
  if (this->receiving_ && (now - this->last_transmission_ >= 200)) {
    // last transmission too long ago. Reset RX index.
    ESP_LOGW(TAG, "Last transmission too long ago. Reset RX index.");
    this->data_.clear();
    this->receiving_ = false;
  }
  if (this->response_complete_() && !this->receiving_ && avail == 0 &&
      (now - this->last_transmission_ >= DALY_RESPONSE_GAP_MS) && !this->trigger_next_) {
    // response complete and nothing more on the line -> trigger next request without waiting for the timeout
    this->last_transmission_ = now;
    this->trigger_next_ = true;
  }
  if ((now - this->last_transmission_ >= DALY_RESPONSE_TIMEOUT_MS) && !this->trigger_next_) {
    // no response -> trigger next request
    this->last_transmission_ = now;
    this->trigger_next_ = true;
  }
  if (avail > 0)
    this->last_transmission_ = now;
  this->read_available(
      [this](uint8_t c) {
        if (!this->receiving_) {
          if (c != 0xa5)
            return;
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
      },
      avail);

  if (!this->trigger_next_)
    return;
  this->trigger_next_ = false;
  this->frames_received_ = 0;
  if (this->next_request_ >= DALY_REQUEST_COUNT) {
    // the response to the last request is in or has timed out: the cycle is over
    this->cycle_running_ = false;
    this->next_request_ = 0;
  }
  if (!this->cycle_running_) {
    if (!this->cycle_requested_)
      return;  // idle until the next update()
    this->cycle_requested_ = false;
    this->cycle_running_ = true;
  }
  // the response timeout counts from the request
  this->last_transmission_ = now;
  // the request IDs are consecutive, see DALY_REQUEST_BATTERY_LEVEL
  this->last_request_ = DALY_REQUEST_BATTERY_LEVEL + this->next_request_++;
  this->request_data_(this->last_request_);
}

bool DalyBmsComponent::response_complete_() const {
  switch (this->last_request_) {
    case DALY_REQUEST_CELL_VOLTAGE:
      // until the status response gave the cell count, the cell voltages wait for the timeout
      return this->cell_frames_ != 0 && this->frames_received_ >= this->cell_frames_;
    case DALY_REQUEST_TEMPERATURE:
      return this->frames_received_ >= this->temperature_frames_;
    default:
      return this->frames_received_ != 0;
  }
}

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

  request_message[12] = sum_bytes(request_message, 4);  // Checksum

  ESP_LOGV(TAG, "Request datapacket Nr %x", data_id);
  this->write_array(request_message, sizeof(request_message));
  this->flush();
}

void DalyBmsComponent::decode_data_(std::vector<uint8_t> data) {
  auto it = data.begin();

  while ((it = std::find(it, data.end(), 0xA5)) != data.end()) {
    if (data.end() - it >= DALY_FRAME_SIZE && it[1] == 0x01) {
      const uint8_t checksum = sum_bytes(&*it, 12);

      if (checksum == it[12]) {
        // only a valid frame answering the last request counts; an echoed request or a late answer does not
        if (it[2] == this->last_request_)
          this->frames_received_++;
        if (it[2] == DALY_REQUEST_STATUS) {
          this->cell_frames_ = (it[4] + 2) / 3;  // three cells per cell voltage frame
          this->temperature_frames_ = std::max((it[5] + 6) / 7, 1);
        }
        switch (it[2]) {
#ifdef USE_SENSOR
          case DALY_REQUEST_BATTERY_LEVEL:
            PUBLISH_STATE(this->voltage_sensor_, (float) encode_uint16(it[4], it[5]) / 10);
            PUBLISH_STATE(this->current_sensor_, (float) (encode_uint16(it[8], it[9]) - DALY_CURRENT_OFFSET) / 10);
            PUBLISH_STATE(this->battery_level_sensor_, (float) encode_uint16(it[10], it[11]) / 10);
            break;

          case DALY_REQUEST_MIN_MAX_VOLTAGE:
            PUBLISH_STATE(this->max_cell_voltage_sensor_, (float) encode_uint16(it[4], it[5]) / 1000);
            PUBLISH_STATE(this->max_cell_voltage_number_sensor_, it[6]);
            PUBLISH_STATE(this->min_cell_voltage_sensor_, (float) encode_uint16(it[7], it[8]) / 1000);
            PUBLISH_STATE(this->min_cell_voltage_number_sensor_, it[9]);
            break;

          case DALY_REQUEST_MIN_MAX_TEMPERATURE:
            PUBLISH_STATE(this->max_temperature_sensor_, it[4] - DALY_TEMPERATURE_OFFSET);
            PUBLISH_STATE(this->max_temperature_probe_number_sensor_, it[5]);
            PUBLISH_STATE(this->min_temperature_sensor_, it[6] - DALY_TEMPERATURE_OFFSET);
            PUBLISH_STATE(this->min_temperature_probe_number_sensor_, it[7]);
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
            PUBLISH_STATE(this->charging_mos_enabled_binary_sensor_, it[5]);
            PUBLISH_STATE(this->discharging_mos_enabled_binary_sensor_, it[6]);
#endif
#ifdef USE_SENSOR
            PUBLISH_STATE(this->remaining_capacity_sensor_, (float) encode_uint32(it[8], it[9], it[10], it[11]) / 1000);
#endif
            break;

#ifdef USE_SENSOR
          case DALY_REQUEST_STATUS:
            PUBLISH_STATE(this->cells_number_sensor_, it[4]);
            break;

          case DALY_REQUEST_TEMPERATURE:
            if (it[4] == 1) {
              PUBLISH_STATE(this->temperature_1_sensor_, it[5] - DALY_TEMPERATURE_OFFSET);
              PUBLISH_STATE(this->temperature_2_sensor_, it[6] - DALY_TEMPERATURE_OFFSET);
            }
            break;

          case DALY_REQUEST_CELL_VOLTAGE:
            switch (it[4]) {
              case 1:
                PUBLISH_STATE(this->cell_1_voltage_sensor_, (float) encode_uint16(it[5], it[6]) / 1000);
                PUBLISH_STATE(this->cell_2_voltage_sensor_, (float) encode_uint16(it[7], it[8]) / 1000);
                PUBLISH_STATE(this->cell_3_voltage_sensor_, (float) encode_uint16(it[9], it[10]) / 1000);
                break;
              case 2:
                PUBLISH_STATE(this->cell_4_voltage_sensor_, (float) encode_uint16(it[5], it[6]) / 1000);
                PUBLISH_STATE(this->cell_5_voltage_sensor_, (float) encode_uint16(it[7], it[8]) / 1000);
                PUBLISH_STATE(this->cell_6_voltage_sensor_, (float) encode_uint16(it[9], it[10]) / 1000);
                break;
              case 3:
                PUBLISH_STATE(this->cell_7_voltage_sensor_, (float) encode_uint16(it[5], it[6]) / 1000);
                PUBLISH_STATE(this->cell_8_voltage_sensor_, (float) encode_uint16(it[7], it[8]) / 1000);
                PUBLISH_STATE(this->cell_9_voltage_sensor_, (float) encode_uint16(it[9], it[10]) / 1000);
                break;
              case 4:
                PUBLISH_STATE(this->cell_10_voltage_sensor_, (float) encode_uint16(it[5], it[6]) / 1000);
                PUBLISH_STATE(this->cell_11_voltage_sensor_, (float) encode_uint16(it[7], it[8]) / 1000);
                PUBLISH_STATE(this->cell_12_voltage_sensor_, (float) encode_uint16(it[9], it[10]) / 1000);
                break;
              case 5:
                PUBLISH_STATE(this->cell_13_voltage_sensor_, (float) encode_uint16(it[5], it[6]) / 1000);
                PUBLISH_STATE(this->cell_14_voltage_sensor_, (float) encode_uint16(it[7], it[8]) / 1000);
                PUBLISH_STATE(this->cell_15_voltage_sensor_, (float) encode_uint16(it[9], it[10]) / 1000);
                break;
              case 6:
                PUBLISH_STATE(this->cell_16_voltage_sensor_, (float) encode_uint16(it[5], it[6]) / 1000);
                PUBLISH_STATE(this->cell_17_voltage_sensor_, (float) encode_uint16(it[7], it[8]) / 1000);
                PUBLISH_STATE(this->cell_18_voltage_sensor_, (float) encode_uint16(it[9], it[10]) / 1000);
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

}  // namespace esphome::daly_bms
