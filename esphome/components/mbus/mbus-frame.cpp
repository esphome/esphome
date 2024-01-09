#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "mbus-frame.h"
#include "mbus-frame-meta.h"
#include "mbus-decoder.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus-frame";

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

      for (auto data_i = 0; data_i < frame.data.size(); data_i++) {
        buffer[index++] = frame.data[data_i];
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

      for (auto i = 0; i < frame.data.size(); i++) {
        checksum += frame.data[i];
      }

      break;

    default:
      checksum = 0;
  }

  return checksum;
}

// https://www.miller-alex.de/Mbus
void MBusFrame::dump() {
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

  switch (this->frame_type) {
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

  ESP_LOGV(TAG, "\tunknown frame type");
}

void MBusDataVariable::dump() {
  ESP_LOGV(TAG, "\tVariable Data:");
  ESP_LOGV(TAG, "\t Header:");

  auto header = &this->header;
  auto id = MBusDecoder::decode_bcd_hex(header->id);
  ESP_LOGV(TAG, "\t  id = %s (%.4d)", format_hex_pretty(header->id, 4).c_str(), id);

  auto manufacturer = MBusDecoder::decode_manufacturer(header->manufacturer);
  ESP_LOGV(TAG, "\t  manufacturer = %s", manufacturer.c_str());
  ESP_LOGV(TAG, "\t  version = 0x%.2X", header->version);
  auto medium = MBusDecoder::decode_medium(header->medium);
  ESP_LOGV(TAG, "\t  medium = %s", medium.c_str());
  ESP_LOGV(TAG, "\t  access no = 0x%.2X", header->access_no);
  ESP_LOGV(TAG, "\t  status = 0x%.2X", header->status);
  ESP_LOGV(TAG, "\t  signature = %s", format_hex_pretty(header->signature, 2).c_str());
  ESP_LOGV(TAG, "\t Records:");
  auto records = &this->records;
  for (auto it = records->begin(); it < records->end(); it++) {
    ESP_LOGV(TAG, "\t  DIF: 0x%.2X DIFE: %s VIF: 0x%.2X VIFE: %s Data: %s", it->drh.dib.dif,
             format_hex_pretty(it->drh.dib.dife).c_str(), it->drh.vib.vif, format_hex_pretty(it->drh.vib.vife).c_str(),
             format_hex_pretty(it->data).c_str());
  }
}

std::string MBusDataVariableHeader::get_secondary_address() {
  auto id = MBusDecoder::decode_bcd_hex(this->id);
  return str_snprintf("%08lX%02X%02X%02X%02X", 33, id, this->manufacturer[0], this->manufacturer[1], this->version,
                      this->medium);
}

}  // namespace mbus
}  // namespace esphome
