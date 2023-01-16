#include "chargery_bms.h"
#include "esphome/core/log.h"

namespace esphome {
namespace chargery_bms {

static const char *const TAG = "chargery_bms";

static const uint8_t MAX_CHARGERY_PACKET_SIZE = 61;
static const uint8_t PACKET_HEADER = 0x24;
static const uint8_t PACKET_STATUS_CELLS = 0x56;
static const uint8_t PACKET_STATUS_BMS = 0x57;
static const uint8_t PACKET_STATUS_IMPEDANCES = 0x58;
static const uint8_t PACKET_STATUS_CELLS_CONSTANT_FIELDS_SIZE = 13;  
static const uint8_t PACKET_STATUS_IMPEDANCS_CONSTANT_FIELDS_SIZE = 8;

static const uint8_t PACKET_LENGTH_MINIMUM = 10;
static const std::string current_modes[] = {"DISCHARGE", "CHARGE", "STORAGE"};

void ChargeryBmsComponent::setup() { 
  this->packet_.reserve(MAX_CHARGERY_PACKET_SIZE); 
  this->packet_.resize(0);
}

void ChargeryBmsComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Chargery BMS:");
  this->check_uart_settings(115200);
}

void ChargeryBmsComponent::loop() {
  this->read_packet_();
  if (this->packet_.size()>=PACKET_LENGTH_MINIMUM) {
    this->get_in_sync_();
    this->decode_packet_();
  }
}

void ChargeryBmsComponent::read_packet_() {
  int available_data = this->available();
  int current_pos = this->packet_.size();
  int data_to_read = std::min(available_data, static_cast<int>(this->packet_.capacity() - current_pos));
  if (data_to_read>0) {
    this->packet_.resize(current_pos + data_to_read);
    this->read_array(this->packet_.data() + current_pos, data_to_read);
  }
}

void ChargeryBmsComponent::decode_packet_() {
  // verify that whole packet is read and has correct length
  if (this->packet_.size() < PACKET_LENGTH_MINIMUM)
    return;
  auto packet_length = this->packet_[3];
  if (packet_length > MAX_CHARGERY_PACKET_SIZE) {
    this->skip_malformed_packet_();
    return;
  }
  if (this->packet_.size() < packet_length)
    return;
  // verify packet chksum
  auto chksum_offset = packet_length - 1;
  uint8_t chksum = 0;
  for (auto pos = this->packet_.begin(); pos < this->packet_.begin() + chksum_offset; ++pos) {
    chksum += *pos;
  }
  if (chksum != this->packet_[chksum_offset]) {
    this->skip_malformed_packet_();
    return;
  }

  // decode command
  auto packet_cmd = this->packet_[2];
  switch (packet_cmd) {
    case PACKET_STATUS_CELLS:
      this->decode_status_cells_((packet_length - PACKET_STATUS_CELLS_CONSTANT_FIELDS_SIZE) / 2);
      this->packet_consumed_(packet_length);
      break;
    case PACKET_STATUS_BMS:
      this->decode_status_bms_();
      this->packet_consumed_(packet_length);
      break;
    case PACKET_STATUS_IMPEDANCES:
      this->decode_status_impedances_((packet_length - PACKET_STATUS_IMPEDANCS_CONSTANT_FIELDS_SIZE) / 2);
      this->packet_consumed_(packet_length);
      break;
    default:
      this->skip_malformed_packet_();
  }
}

void ChargeryBmsComponent::packet_consumed_(uint8_t len) {
  this->packet_.erase(this->packet_.begin(), this->packet_.begin() + len);
}

void ChargeryBmsComponent::decode_status_cells_(uint8_t cells) {
  auto pos = this->packet_.begin() + 4;
  uint16_t min = 0xffff, max = 0;
  uint8_t min_i = 0, max_i = 0;
  uint32_t battery_voltage=0;
  for (uint8_t i = 0; i < cells; i++) {
    auto b1 = *pos++;
    auto b2 = *pos++;
    auto val = encode_uint16(b1, b2);  // high byte 1st
    if (i < this->battery_num_cells_) {
      if (val < min) {
        min = val;
        min_i = i;
      }
      if (val > max) {
        max = val;
        max_i = i;
      }
    }
    battery_voltage += val;
    if (this->cell_voltages_[i] != nullptr)
      this->cell_voltages_[i]->publish_state((float) val / 1000);
  }
  if (this->voltage_sensor_ != nullptr) {
    this->voltage_sensor_->publish_state((float)battery_voltage/1000);
  }

  if (cells > 0) {
    if (this->min_cell_voltage_ != nullptr)
      this->min_cell_voltage_->publish_state((float) min / 1000);
    if (this->min_cell_voltage_number_ != nullptr)
      this->min_cell_voltage_number_->publish_state(min_i + 1);
    if (this->max_cell_voltage_ != nullptr)
      this->max_cell_voltage_->publish_state((float) max / 1000);
    if (this->max_cell_voltage_number_ != nullptr)
      this->max_cell_voltage_number_->publish_state(max_i + 1);
  }
  {
    auto b1 = *pos++;
    auto b2 = *pos++;
    auto b3 = *pos++;
    auto b4 = *pos++;
    if (this->remaining_capacity_wh_)
      this->remaining_capacity_wh_->publish_state((float) encode_uint32(b4, b3, b2, b1) / 1000);  // low byte 1st
  }
  {
    auto b1 = *pos++;
    auto b2 = *pos++;
    auto b3 = *pos++;
    auto b4 = *pos++;
    if (this->remaining_capacity_ah_)
      this->remaining_capacity_ah_->publish_state((float) encode_uint32(b4, b3, b2, b1) / 1000);  // low byte 1st
  }
}

void ChargeryBmsComponent::decode_status_bms_() {
  auto pos = this->packet_.begin() + 4;
  {  // charge_end_voltage
    auto b1 = *pos++;
    auto b2 = *pos++;
    if (this->charge_end_voltage_sensor_ != nullptr)
      this->charge_end_voltage_sensor_->publish_state((float) encode_uint16(b1, b2) / 1000);  // high byte 1st
  }
  {  // current_mode
    auto current_mode = *pos++;
    if (this->current_mode_sensor_ != nullptr)
      this->current_mode_sensor_->publish_state(current_modes[current_mode]);
  }
  {  // current
    auto b1 = *pos++;
    auto b2 = *pos++;
    if (this->current_sensor_ != nullptr)
      this->current_sensor_->publish_state((float) encode_uint16(b1, b2) / 10);  // high byte 1st
  }
  uint16_t t1, t2;
  {  // battery temp1
    auto b1 = *pos++;
    auto b2 = *pos++;
    t1 = encode_uint16(b1, b2);  // high byte 1st
    if (this->temperature_1_sensor_ != nullptr)
      this->temperature_1_sensor_->publish_state((float) t1 / 10);
  }
  {  // battery temp2
    auto b1 = *pos++;
    auto b2 = *pos++;
    t2 = encode_uint16(b1, b2);  // high byte 1st
    if (this->temperature_2_sensor_ != nullptr)
      this->temperature_2_sensor_->publish_state((float) t2 / 10);
  }
  if (this->min_temperature_ != nullptr)
    this->min_temperature_->publish_state((float) ((t1 < t2) ? t1 : t2) / 10);
  if (this->max_temperature_ != nullptr)
    this->max_temperature_->publish_state((float) ((t1 > t2) ? t1 : t2) / 10);
  if (this->min_temperature_probe_number_ != nullptr)
    this->min_temperature_probe_number_->publish_state(((t1 < t2) ? 1 : 2));
  if (this->max_temperature_probe_number_ != nullptr)
    this->max_temperature_probe_number_->publish_state(((t1 > t2) ? 1 : 2));

  {  // SOC
    auto soc = *pos++;
    if (this->battery_level_sensor_ != nullptr)
      this->battery_level_sensor_->publish_state((float) soc);
  }
}

void ChargeryBmsComponent::decode_status_impedances_(uint8_t cells) {
  auto pos = this->packet_.begin() + 4;
  {
    auto current_mode = *pos++;
    if (this->current1_mode_sensor_ != nullptr)
      this->current1_mode_sensor_->publish_state(current_modes[current_mode]);
  }
  {
    auto b1 = *pos++;
    auto b2 = *pos++;
    if (this->current1_sensor_ != nullptr)
      this->current1_sensor_->publish_state((float) encode_uint16(b2, b1) / 10);  // low byte 1st
  }
  uint16_t min = 0xffff, max = 0;
  uint8_t min_i = 0, max_i = 0;
  for (uint8_t i = 0; i < cells; i++) {
    auto b1 = *pos++;
    auto b2 = *pos++;
    auto val = encode_uint16(b2, b1);  // low byte 1st
    if (val < min) {
      min = val;
      min_i = i;
    }
    if (val > max) {
      max = val;
      max_i = i;
    }
    if (this->cell_impedances_[i] != nullptr)
      this->cell_impedances_[i]->publish_state((float) val / 10);  // low byte 1st
  }
  if (cells == 0)
    return;
  if (this->min_cell_impedance_ != nullptr)
    this->min_cell_impedance_->publish_state((float) min / 10);
  if (this->min_cell_impedance_number_ != nullptr)
    this->min_cell_impedance_number_->publish_state(min_i + 1);
  if (this->max_cell_impedance_ != nullptr)
    this->max_cell_impedance_number_->publish_state((float) max / 10);
  if (this->max_cell_impedance_number_ != nullptr)
    this->max_cell_impedance_number_->publish_state(max_i + 1);
}

void ChargeryBmsComponent::skip_malformed_packet_() {
  this->packet_[0] = 0;  // we should only skip first byte and try to resync
  this->get_in_sync_();
}

void ChargeryBmsComponent::get_in_sync_() {
  int header_seen = 0;
  auto pos = this->packet_.begin();
  for (; pos < this->packet_.end() && header_seen < 2; ++pos) {
    if (*pos == PACKET_HEADER)
      ++header_seen;
    else
      header_seen = 0;
  }
  this->packet_.erase(this->packet_.begin(), pos - header_seen);
}

float ChargeryBmsComponent::get_setup_priority() const { return setup_priority::DATA; }

}  // namespace chargery_bms
}  // namespace esphome
