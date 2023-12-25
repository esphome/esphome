#include "esphome/core/log.h"
#include "mbus-frame.h"
#include "mbus-frame-meta.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus-protocol";

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

      // if (buffer_size < frame.data_size + MBusFrameBaseSize::LONG_SIZE) {
      //   return -4;
      // }

      // data[offset++] = frame.start1;
      // data[offset++] = frame.length1;
      // data[offset++] = frame.length2;
      // data[offset++] = frame.start2;

      // data[offset++] = frame.control;
      // data[offset++] = frame.address;
      // data[offset++] = frame.control_information;

      // for (i = 0; i < frame.data_size; i++) {
      //   data[offset++] = frame.data[i];
      // }

      // data[offset++] = frame.checksum;
      // data[offset++] = frame.stop;

      return -2;

    default:
      return -2;
  }
}

uint8_t MBusFrame::calc_length(MBusFrame &frame) {
  switch (frame.frame_type) {
    case MBUS_FRAME_TYPE_ACK:
    case MBUS_FRAME_TYPE_SHORT:
    case MBUS_FRAME_TYPE_CONTROL:
      return frame.length;

    // case MBUS_FRAME_TYPE_LONG:
    //   return frame.data_size + 3;
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

      // case MBUS_FRAME_TYPE_LONG:

      //   checksum = frame.control;
      //   checksum += frame.address;
      //   checksum += frame.control_information;

      //   for (size_t i = 0; i < frame.data_size; i++) {
      //     checksum += frame.data[i];
      //   }

      //   break;

    default:
      checksum = 0;
  }

  return checksum;
}

}  // namespace mbus
}  // namespace esphome
