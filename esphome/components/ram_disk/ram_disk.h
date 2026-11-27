#pragma once

#include "esphome/components/storage/block_device.h"
#include "esphome/core/component.h"

namespace esphome::ram_disk {

/// A block device held in RAM, in PSRAM when there is some. Its contents are lost on reboot.
class RamDisk : public storage::BlockDevice, public Component {
 public:
  RamDisk(uint32_t block_size, uint32_t block_count) : block_count_(block_count) {
    this->geometry_.block_size = block_size;
  }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

 protected:
  storage::StorageError device_read(uint32_t block, uint32_t offset, void *buf, uint32_t len) override;
  storage::StorageError device_program(uint32_t block, uint32_t offset, const void *buf, uint32_t len) override;

  uint8_t *data_{nullptr};
  uint32_t block_count_;  // geometry_.block_count stays 0 until the buffer is allocated
};

}  // namespace esphome::ram_disk
