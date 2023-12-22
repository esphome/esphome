#include "mbus-frame.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mbus {

static const char *const TAG = "mbus-protocol";

MBusFrame::MBusFrame(MBusFrameType frame_type) {
  this->frame_type = frame_type;
  switch (frame_type) {
    case MBUS_FRAME_TYPE_ACK:
      this->start = MBUS_FRAME_ACK_START;
      this->length = 0;
      break;

    case MBUS_FRAME_TYPE_SHORT:
      this->start = MBUS_FRAME_SHORT_START;
      this->length = 0;
      this->stop = MBUS_FRAME_STOP;
      break;

    case MBUS_FRAME_TYPE_CONTROL:
      this->start = MBUS_FRAME_CONTROL_START;
      this->length = 3;
      this->stop = MBUS_FRAME_STOP;
      break;

    case MBUS_FRAME_TYPE_LONG:
      this->start = MBUS_FRAME_LONG_START;
      this->stop = MBUS_FRAME_STOP;
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

      if (buffer_size < MBusFrameBaseSize::ACK_SIZE) {
        ESP_LOGE(TAG, "serialize(): buffer size to low. Buffer size = %d ACK Frame Size = %d", buffer_size,
                 MBusFrameBaseSize::ACK_SIZE);
        return -1;
      }

      buffer[index++] = frame.start;
      return 0;

    case MBUS_FRAME_TYPE_SHORT:

      if (buffer_size < MBusFrameBaseSize::SHORT_SIZE) {
        ESP_LOGE(TAG, "serialize(): buffer size to low. Buffer size = %d SHORT Frame Size = %d", buffer_size,
                 MBusFrameBaseSize::SHORT_SIZE);
        return -1;
      }

      buffer[index++] = frame.start;
      buffer[index++] = frame.control;
      buffer[index++] = frame.address;
      buffer[index++] = frame.checksum;
      buffer[index++] = frame.stop;

      return 0;

    case MBUS_FRAME_TYPE_CONTROL:

      if (buffer_size < MBusFrameBaseSize::CONTROL_SIZE) {
        ESP_LOGE(TAG, "serialize(): buffer size to low. Buffer size = %d CONTROL Frame Size = %d", buffer_size,
                 MBusFrameBaseSize::CONTROL_SIZE);
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
