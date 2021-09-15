#include "i2c.h"
#include "esphome/core/log.h"
#include <memory>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c";

bool I2CDevice::write_bytes_16(uint8_t a_register, const uint16_t *data, uint8_t len) {
  // we have to copy in order to be able to change byte order
  std::unique_ptr<uint16_t[]> temp{new uint16_t[len]};
  for (size_t i = 0; i < len; i++)
    temp[i] = internal::htoi2cs(data[i]);
  return write_register(a_register, reinterpret_cast<uint8_t *>(data), len*2) == ERROR_OK;
}

}  // namespace i2c
}  // namespace esphome
