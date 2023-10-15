#pragma once
#include "ota_component.h"

namespace esphome {
namespace ota {

class OTABackend {
 public:
  virtual ~OTABackend() = default;
  virtual OTAResponseTypes begin(OTABinType bin_type, size_t image_size) = 0;
  virtual void set_update_md5(const char *md5) = 0;
  virtual OTAResponseTypes write(uint8_t *data, size_t len) = 0;
  virtual OTAResponseTypes end() = 0;
  virtual void abort() = 0;
  virtual bool supports_compression() = 0;
  virtual bool supports_writing_bootloader() { return false; }
  virtual bool supports_writing_partition_table() { return false; }
  virtual bool supports_writing_partitions() { return false; }
};

}  // namespace ota
}  // namespace esphome
