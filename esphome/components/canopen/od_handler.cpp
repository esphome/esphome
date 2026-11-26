// OD handlers: ObjectDictionary::set_object() and write().
#include "od.h"

#include "esphome/core/log.h"

namespace esphome::canopen {

ESPHOME_LOG_TAG(TAG, "canopen.od");

void ObjectDictionary::set_extension_(uint16_t index, const OD_extension_t &extension) {
  Pending *p = this->pending_(index);
  if (p == nullptr)
    return;
  for (auto &entry : p->entries) {
    if (entry.index == index) {
      p->extensions.push_back(extension);
      entry.extension = p->extensions.size();
      return;
    }
  }
  ESP_LOGE(TAG, "OD 0x%04x: handler for a missing entry, ignored", index);
}

OdResult ObjectDictionary::read(uint16_t index, uint8_t sub, void *data, size_t max_len, size_t *len) {
  *len = 0;
  if (!this->finalized())
    return OdResult::OD_RESULT_DEVICE_STATE;
  OD_IO_t io;
  auto ret = static_cast<OdResult>(OD_getSub(OD_find(&this->od_, index), sub, &io, false));
  if (ret != OdResult::OD_RESULT_OK)
    return ret;
  if ((io.stream.attribute & ODA_SDO_R) == 0)
    return OdResult::OD_RESULT_WRITE_ONLY;
  auto *out = static_cast<uint8_t *>(data);
  while (true) {
    OD_size_t count = 0;
    ret = static_cast<OdResult>(io.read(&io.stream, out + *len, max_len - *len, &count));
    *len += count;
    if (ret != OdResult::OD_RESULT_PARTIAL)
      return ret;
    if (*len >= max_len)
      return OdResult::OD_RESULT_DATA_LONG;  // more data, no room for it
    if (count == 0)
      return OdResult::OD_RESULT_GENERAL;  // a handler asking for another call without making progress
  }
}

size_t ObjectDictionary::value_size(uint16_t index, uint8_t sub) {
  if (!this->finalized())
    return 0;
  OD_IO_t io;
  if (OD_getSub(OD_find(&this->od_, index), sub, &io, false) != ODR_OK)
    return 0;
  return io.stream.dataLength;
}

OdResult ObjectDictionary::write(uint16_t index, uint8_t sub, const void *data, size_t len) {
  if (!this->finalized())
    return OdResult::OD_RESULT_DEVICE_STATE;
  OD_IO_t io;
  auto ret = static_cast<OdResult>(OD_getSub(OD_find(&this->od_, index), sub, &io, false));
  if (ret != OdResult::OD_RESULT_OK)
    return ret;
  if ((io.stream.attribute & ODA_SDO_W) == 0)
    return OdResult::OD_RESULT_READ_ONLY;
  if (io.stream.dataLength == 0) {
    // no fixed size (domain): the whole write, as an SDO client indicates it
    io.stream.dataLength = len;
  } else if (len > io.stream.dataLength) {
    return OdResult::OD_RESULT_DATA_LONG;
  } else if (len < io.stream.dataLength) {
    return OdResult::OD_RESULT_DATA_SHORT;
  }
  OD_size_t written = 0;
  ret = static_cast<OdResult>(io.write(&io.stream, data, len, &written));
  // all data was given: a handler asking for more means the value was too short
  return ret == OdResult::OD_RESULT_PARTIAL ? OdResult::OD_RESULT_DATA_SHORT : ret;
}

}  // namespace esphome::canopen
