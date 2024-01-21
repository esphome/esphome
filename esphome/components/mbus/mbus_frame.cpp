
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "mbus_decoder.h"
#include "mbus_frame.h"
#include "mbus_frame_meta.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus_frame";

MBusFrame::MBusFrame(MBusFrameType frame_type) {
  this->frame_type = frame_type;
  switch (frame_type) {
    case MBUS_FRAME_TYPE_ACK:
      this->start = MBusFrameDefinition::ACK_FRAME.start_bit;
      break;

    case MBUS_FRAME_TYPE_SHORT:
      this->start = MBusFrameDefinition::SHORT_FRAME.start_bit;
      this->stop = MBusFrameDefinition::SHORT_FRAME.stop_bit;
      break;

    case MBUS_FRAME_TYPE_CONTROL:
      this->start = MBusFrameDefinition::CONTROL_FRAME.start_bit;
      this->length = MBusFrameDefinition::CONTROL_FRAME.lenght;
      this->stop = MBusFrameDefinition::CONTROL_FRAME.stop_bit;
      break;

    case MBUS_FRAME_TYPE_LONG:
      this->start = MBusFrameDefinition::LONG_FRAME.start_bit;
      this->length = MBusFrameDefinition::LONG_FRAME.lenght;
      this->stop = MBusFrameDefinition::LONG_FRAME.stop_bit;
      break;
  }
}

MBusFrame::MBusFrame(MBusFrame &frame) {
  this->address = frame.address;
  this->control = frame.control;
  this->control_information = frame.control_information;
  this->frame_type = frame.frame_type;
  this->start = frame.start;
  this->stop = frame.stop;
  this->length = frame.length;

  this->data.reserve(frame.data.size());
  this->data.insert(this->data.begin(), frame.data.begin(), frame.data.end());
}

uint8_t MBusFrame::serialize(MBusFrame &frame, std::vector<uint8_t> &buffer) {
  frame.length = calc_length(frame);
  frame.checksum = calc_checksum(frame);
  size_t buffer_size = buffer.size();

  uint8_t index = 0;
  switch (frame.frame_type) {
    case MBUS_FRAME_TYPE_ACK:
      if (buffer_size < MBusFrameDefinition::ACK_FRAME.base_frame_size) {
        ESP_LOGE(TAG, "serialize(): buffer size to low. Buffer size = %d ACK Frame Size = %d", buffer_size,
                 MBusFrameDefinition::ACK_FRAME.base_frame_size);
        return -1;
      }

      buffer[index++] = frame.start;
      return 0;

    case MBUS_FRAME_TYPE_SHORT:
      if (buffer_size < MBusFrameDefinition::SHORT_FRAME.base_frame_size) {
        ESP_LOGE(TAG, "serialize(): buffer size to low. Buffer size = %d SHORT Frame Size = %d", buffer_size,
                 MBusFrameDefinition::SHORT_FRAME.base_frame_size);
        return -1;
      }

      buffer[index++] = frame.start;
      buffer[index++] = frame.control;
      buffer[index++] = frame.address;
      buffer[index++] = frame.checksum;
      buffer[index++] = frame.stop;

      return 0;

    case MBUS_FRAME_TYPE_CONTROL:
      if (buffer_size < MBusFrameDefinition::CONTROL_FRAME.base_frame_size) {
        ESP_LOGE(TAG, "serialize(): buffer size to low. Buffer size = %d CONTROL Frame Size = %d", buffer_size,
                 MBusFrameDefinition::CONTROL_FRAME.base_frame_size);
        return -1;
      }

      buffer[index++] = frame.start;
      buffer[index++] = frame.length;
      buffer[index++] = frame.length;
      buffer[index++] = frame.start;
      buffer[index++] = frame.control;
      buffer[index++] = frame.address;
      buffer[index++] = frame.control_information;
      buffer[index++] = frame.checksum;
      buffer[index++] = frame.stop;

      return 0;

    case MBUS_FRAME_TYPE_LONG:
      auto frame_size = MBusFrameDefinition::LONG_FRAME.base_frame_size + frame.data.size();
      if (buffer_size < frame_size) {
        ESP_LOGE(TAG, "serialize(): buffer size to low. Buffer size = %d LONG Frame Size = %d", buffer_size,
                 frame_size);
        return -1;
      }

      buffer[index++] = frame.start;
      buffer[index++] = frame.length;
      buffer[index++] = frame.length;
      buffer[index++] = frame.start;
      buffer[index++] = frame.control;
      buffer[index++] = frame.address;
      buffer[index++] = frame.control_information;

      for (auto data : frame.data) {
        buffer[index++] = data;
      }

      buffer[index++] = frame.checksum;
      buffer[index++] = frame.stop;

      return 0;
  }

  return -2;  // Not supported Frame Type
}

uint8_t MBusFrame::calc_length(MBusFrame &frame) {
  switch (frame.frame_type) {
    case MBUS_FRAME_TYPE_ACK:
    case MBUS_FRAME_TYPE_SHORT:
    case MBUS_FRAME_TYPE_CONTROL:
      return frame.length;

    case MBUS_FRAME_TYPE_LONG:
      return frame.data.size() + frame.length;

    default:
      return 0;
  }
}

uint8_t MBusFrame::calc_checksum(MBusFrame &frame) {
  uint8_t checksum;

  switch (frame.frame_type) {
    case MBUS_FRAME_TYPE_ACK:
      checksum = 0;
      break;

    case MBUS_FRAME_TYPE_SHORT:
      checksum = frame.control;
      checksum += frame.address;
      break;

    case MBUS_FRAME_TYPE_CONTROL:
      checksum = frame.control;
      checksum += frame.address;
      checksum += frame.control_information;
      break;

    case MBUS_FRAME_TYPE_LONG:
      checksum = frame.control;
      checksum += frame.address;
      checksum += frame.control_information;

      for (auto data : frame.data) {
        checksum += data;
      }

      break;

    default:
      checksum = 0;
  }

  return checksum;
}

// https://www.miller-alex.de/Mbus
void MBusFrame::dump() const {
  dump_frame_type();
  dump_frame();
}

void MBusFrame::dump_frame_type() const {
  ESP_LOGV(TAG, "MBusFrame");
  switch (this->frame_type) {
    case MBusFrameType::MBUS_FRAME_TYPE_ACK:
      ESP_LOGV(TAG, "\tframe_type = ACK");
      break;
    case MBusFrameType::MBUS_FRAME_TYPE_SHORT:
      ESP_LOGV(TAG, "\tframe_type = SHORT");
      break;
    case MBusFrameType::MBUS_FRAME_TYPE_CONTROL:
      ESP_LOGV(TAG, "\tframe_type = CONTROL");
      break;
    case MBusFrameType::MBUS_FRAME_TYPE_LONG:
      ESP_LOGV(TAG, "\tframe_type = LONG");
      break;
    default:
      ESP_LOGV(TAG, "\tunknown frame type = %d", this->frame_type);
      break;
  }
}

void MBusFrame::dump_frame() const {
  switch (this->frame_type) {
    case MBusFrameType::MBUS_FRAME_TYPE_ACK:
      return;

    case MBusFrameType::MBUS_FRAME_TYPE_SHORT:
      ESP_LOGV(TAG, "\tstart = 0x%.2X", this->start);
      ESP_LOGV(TAG, "\tcontrol = 0x%.2X", this->control);
      ESP_LOGV(TAG, "\taddress = 0x%.2X", this->address);
      ESP_LOGV(TAG, "\tstop = 0x%.2X", this->stop);
      return;

    case MBusFrameType::MBUS_FRAME_TYPE_CONTROL:
      ESP_LOGV(TAG, "\tstart = 0x%.2X", this->start);
      ESP_LOGV(TAG, "\tcontrol = 0x%.2X", this->control);
      ESP_LOGV(TAG, "\taddress = 0x%.2X", this->address);
      ESP_LOGV(TAG, "\tcontrol information = 0x%.2X", this->control_information);
      ESP_LOGV(TAG, "\tstop = 0x%.2X", this->stop);
      return;

    case MBusFrameType::MBUS_FRAME_TYPE_LONG:
      ESP_LOGV(TAG, "\tstart = 0x%.2X", this->start);
      ESP_LOGV(TAG, "\tcontrol = 0x%.2X", this->control);
      ESP_LOGV(TAG, "\taddress = 0x%.2X", this->address);
      ESP_LOGV(TAG, "\tcontrol information = 0x%.2X", this->control_information);
      ESP_LOGV(TAG, "\tdata = %s", format_hex_pretty(this->data).c_str());
      ESP_LOGV(TAG, "\tstop = 0x%.2X", this->stop);
      if (this->variable_data) {
        this->variable_data->dump();
      }
      return;
  }

  ESP_LOGD(TAG, "\tunknown frame type");
}

void MBusDataVariable::dump() const {
  ESP_LOGD(TAG, "\tVariable Data:");
  ESP_LOGD(TAG, "\t Header:");

  const auto *header = &this->header;
  auto id = MBusDecoder::decode_bcd_uint32(header->id);
  ESP_LOGD(TAG, "\t  id = %s (0x%.8X)", format_hex_pretty(header->id, 4).c_str(), id);

  auto manufacturer = MBusDecoder::decode_manufacturer(header->manufacturer);
  ESP_LOGD(TAG, "\t  manufacturer = %s", manufacturer.c_str());
  ESP_LOGD(TAG, "\t  version = 0x%.2X", header->version);
  auto medium = MBusDecoder::decode_medium(header->medium);
  ESP_LOGD(TAG, "\t  medium = %s", medium.c_str());
  ESP_LOGD(TAG, "\t  access no = 0x%.2X", header->access_no);
  ESP_LOGD(TAG, "\t  status = 0x%.2X", header->status);
  ESP_LOGD(TAG, "\t  signature = %s", format_hex_pretty(header->signature, 2).c_str());
  ESP_LOGD(TAG, "\t Records:");

  const auto *records = &this->records;
  for (auto i = 0; i < records->size(); i++) {
    auto record = records->at(i);
    auto mbus_data = record.parse(i);
    ESP_LOGD(TAG,
             "\t  DIF: 0x%.2X DIFE: %s VIF: 0x%.2X VIFE: %s Data: %s. (ID: %d, Function: %s, Unit: %s, Tariff: %d, "
             "Type: %s, %f)",
             record.drh.dib.dif, format_hex_pretty(record.drh.dib.dife).c_str(), record.drh.vib.vif,
             format_hex_pretty(record.drh.vib.vife).c_str(), format_hex_pretty(record.data).c_str(), mbus_data->id,
             mbus_data->function.c_str(), mbus_data->unit.c_str(), mbus_data->tariff,
             mbus_data->get_data_type_str().c_str(), mbus_data->value);
  }
}

std::unique_ptr<MBusValue> MBusDataRecord::parse(const uint8_t id) {
  auto mbus_value = make_unique<MBusValue>();

  mbus_value->id = id;
  mbus_value->tariff = parse_tariff_(this);
  mbus_value->function = parse_function_(this);
  mbus_value->unit = parse_unit_(this);

  auto data_type = parse_data_type_(this);
  mbus_value->data_type = data_type;
  mbus_value->value = parse_value_(this, data_type);

  return mbus_value;
}

uint32_t MBusDataRecord::parse_tariff_(const MBusDataRecord *record) {
  int bit_index = 0;
  long result = 0;
  int i;

  auto dife_size = record->drh.dib.dife.size();
  if (record && (dife_size > 0)) {
    for (i = 0; i < dife_size; i++) {
      result |= ((record->drh.dib.dife[i] & MBusDataDifMask::TARIFF) >> 4) << bit_index;
      bit_index += 2;
    }

    return result;
  }

  return -1;
}

std::string MBusDataRecord::parse_function_(const MBusDataRecord *record) {
  if (record) {
    switch (record->drh.dib.dif & MBusDataDifMask::FUNCTION) {
      case 0x00:
        return "Instantaneous value";

      case 0x10:
        return "Maximum value";

      case 0x20:
        return "Minimum value";

      case 0x30:
        return "Value during error state";
        break;

      default:
        return "unknown";
    }
  }

  return "record is null";
}

std::string MBusDataRecord::parse_unit_(const MBusDataRecord *record) {
  const auto *vib = &(record->drh.vib);
  auto vibe_size = vib->vife.size();
  auto unit_and_multiplier = vib->vif & MBusDataVifMask::UNIT_AND_MULTIPLIER;
  auto extension_bit = vib->vif & MBusDataVifMask::EXTENSION_BIT;

  // Primary VIF
  if (unit_and_multiplier >= 0 & unit_and_multiplier <= 0x7B) {
    auto unit_mask = 0b01111000;
    auto multiplier_mask = 0b00000111;

    auto unit = (unit_and_multiplier & unit_mask) >> 3;
    auto exponent = unit_and_multiplier & multiplier_mask;

    switch (unit) {
      case 0b0000:
        return str_sprintf("Energy (10^%d Wh)", exponent - 3);
      case 0b0001:
        return str_sprintf("Energy (10^%d J)", exponent);
      case 0b0010:
        return str_sprintf("Volume (10^%d m^3)", exponent - 6);
      case 0b0011:
        return str_sprintf("Mass (10^%d kg)", exponent - 3);
      case 0b0100: {
        auto data_time_unit = parse_date_time_unit_(exponent);
        if ((exponent & 0b100) == 0) {
          return str_sprintf("Time (%s)", data_time_unit.c_str());
        }
        return str_sprintf("Operating Time (%s)", data_time_unit.c_str());
      }
      case 0b0101:
        return str_sprintf("Power (10^%d W)", exponent - 3);
      case 0b0110:
        return str_sprintf("Power (10^%d J/h)", exponent);
      case 0b0111:
        return str_sprintf("Volume Flow (10^%d m^3/h)", exponent - 6);
      case 0b1000:
        return str_sprintf("Volume Flow ext. (10^%d m^3/min)", exponent - 7);
      case 0b1001:
        return str_sprintf("Volume Flow ext (10^%d m^3/sec)", exponent - 9);
      case 0b1010:
        return str_sprintf("Mass Flow (10^%d kg/h)", exponent - 3);
      case 0b1011: {
        switch (exponent & 0b100) {
          case 0b000:
            return str_sprintf("Flow Temperatur (10^%d °C)", exponent - 3);
          case 0b100:
            return str_sprintf("Return Temperatur (10^%d °C)", exponent - 3);
        }
      }
      case 0b1100: {
        switch (exponent & 0b100) {
          case 0b000:
            return str_sprintf("Temperatur Difference (10^%d K)", exponent - 3);
          case 0b100:
            return str_sprintf("External Temperatur (10^%d °C)", exponent - 3);
        }
      }
      case 0b1101: {
        switch (exponent & 0b100) {
          case 0b000:
            return str_sprintf("Pressure (10^%d bar)", exponent - 3);
          case 0b100:
            return str_sprintf("Time Point (%s)", (exponent & 0b000) == 0 ? "Date" : "Date & Time");
          case 0b110:
            return "Units for H.C.A.";
          case 0b111:
            return "Reserved";
        }
      }
      case 0b1110: {
        auto data_time_unit = parse_date_time_unit_(exponent);
        switch (exponent & 0b100) {
          case 0b000:
            return str_sprintf("Averaging Duration (%s)", data_time_unit.c_str());
          case 0b100:
            return str_sprintf("Actuality Duration (%s)", data_time_unit.c_str());
        }
      }
      case 0b1111:
        ESP_LOGV(TAG, "Unsupported unit.");
        return "";
    }
  }
  // Plain-text VIF (VIF = 7Ch / FCh)
  else if (unit_and_multiplier == 0x7C) {
    ESP_LOGV(TAG, "Plain-text units not supported.");
    return "";
  }
  // Any VIF: 7Eh / FEh
  else if (unit_and_multiplier == 0x7E) {
    ESP_LOGV(TAG, "Any units not supported.");
    return "";
  }
  // Manufacturer specific: 7Fh / FFh
  else if (unit_and_multiplier == 0x7F) {
    return "Manufacturer specific";
  }
  return "";
}

std::string MBusDataRecord::parse_date_time_unit_(const uint8_t exponent) {
  switch (exponent) {
    case 0b00:
      return "seconds";
    case 0b01:
      return "minutes";
    case 0b10:
      return "hours";
    case 0b11:
      return "days";
  }

  return "";
}

MBusDataType MBusDataRecord::parse_data_type_(const MBusDataRecord *record) {
  // 6.3.2 Table 5
  auto data_field = record->drh.dib.dif & MBusDataDifMask::DATA_CODING;
  auto unit_and_multiplier = record->drh.vib.vif & MBusDataVifMask::UNIT_AND_MULTIPLIER;

  switch (data_field) {
    case 0x00:
      return MBusDataType::NO_DATA;
    case 0x01:
      return MBusDataType::INT8;
    case 0x02: {
      // E110 1100  Time Point (date)
      if (unit_and_multiplier & 0b01101100) {
        return MBusDataType::DATE_16;
      }
      return MBusDataType::INT16;
    }
    case 0x03:
      return MBusDataType::INT24;
    case 0x04: {
      // E110 1101  Time Point (date/time)
      if (unit_and_multiplier & 0b01101101) {
        return MBusDataType::DATE_TIME_32;
      }
      if (unit_and_multiplier == 0xFD) {
        ESP_LOGV(TAG, "Linear VIF-Extension Data Type is not supported");
        return MBusDataType::NO_DATA;
      }
      return MBusDataType::INT32;
    }
    case 0x05:
      return MBusDataType::FLOAT;
    case 0x06: {
      // E110 1101  Time Point (date/time)
      if (unit_and_multiplier & 0b01101101) {
        return MBusDataType::DATE_TIME_48;
      }
      if (unit_and_multiplier == 0xFD) {
        ESP_LOGV(TAG, "Linear VIF-Extension Data Type is not supported");
        return MBusDataType::NO_DATA;
      }
      return MBusDataType::INT48;
    }
    case 0x07:
      return MBusDataType::INT64;
    case 0x08: {
      ESP_LOGV(TAG, "Selection for Readout Data Type not supported");
      return MBusDataType::NO_DATA;
    }
    case 0x09:
      return MBusDataType::BCD_8;
    case 0x0A:
      return MBusDataType::BCD_16;
    case 0x0B:
      return MBusDataType::BCD_24;
    case 0x0C:
      return MBusDataType::BCD_32;
    case 0x0D: {
      ESP_LOGV(TAG, "Variable Length Data Type not supported");
      return MBusDataType::NO_DATA;
    }
    case 0x0E:
      return MBusDataType::BCD_48;
    case 0x0F: {
      ESP_LOGV(TAG, "Special Function Data Type not supported");
      return MBusDataType::NO_DATA;
    }
  }

  ESP_LOGV(TAG, "Unknow DIF Data Type %d", data_field);
  return MBusDataType::NO_DATA;
}

float MBusDataRecord::parse_value_(const MBusDataRecord *record, const MBusDataType &data_type) {
  switch (data_type) {
    case MBusDataType::BCD_8:
      return (uint8_t) MBusDecoder::decode_bcd_int(record->data);
    case MBusDataType::BCD_16:
      return (uint16_t) MBusDecoder::decode_bcd_int(record->data);
    case MBusDataType::BCD_24:
      return (uint32_t) MBusDecoder::decode_bcd_int(record->data);
    case MBusDataType::BCD_32:
      return (uint32_t) MBusDecoder::decode_bcd_int(record->data);
    case MBusDataType::BCD_48:
      return (float) MBusDecoder::decode_bcd_int(record->data);
    case MBusDataType::INT16:
    case MBusDataType::INT24:
    case MBusDataType::INT32:
    case MBusDataType::INT64:
    case MBusDataType::INT8:
    case MBusDataType::FLOAT:
    default:
      ESP_LOGV(TAG, "Unsupported data type '%d'", data_type);
      return 0;
  }
  return 0;
}

}  // namespace mbus
}  // namespace esphome
