#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

namespace esphome {
namespace sd_mmc_card {

static const char *const TAG = "sd_mmc_card";

enum class CardType : uint8_t {
  UNKNOWN = 0,
  SDIO = 1,
  MMC = 2,
  SDHC = 3,
  SDXC = 3,
  SDSC = 4,
};

struct FileInfo {
  std::string path;
  uint32_t size;
  bool is_directory;
};

enum MemoryUnits : short { Byte = 0, KiloByte = 1, MegaByte = 2, GigaByte = 3, TeraByte = 4, PetaByte = 5 };

// Forward declaration for mount callback
using mount_ready_callback_t = std::function<void(const std::string &mount_path)>;

class SdMmc : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  // Pin configuration
  void set_clk_pin(uint8_t pin) { this->clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { this->cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { this->data0_pin_ = pin; }
  void set_data1_pin(uint8_t pin) { this->data1_pin_ = pin; }
  void set_data2_pin(uint8_t pin) { this->data2_pin_ = pin; }
  void set_data3_pin(uint8_t pin) { this->data3_pin_ = pin; }
  void set_mode_1bit(bool mode_1bit) { this->mode_1bit_ = mode_1bit; }
  void set_power_ctrl_pin(uint8_t pin) { this->power_ctrl_pin_ = pin; }
  void set_slot(uint8_t slot) { this->slot_ = slot; }
  void set_mount_path(const std::string &path) { this->mount_path_ = path; }

  // File operations
  bool write_file(const std::string &path, const std::string &data);
  bool append_file(const std::string &path, const std::string &data);
  std::string read_file(const std::string &path);
  bool delete_file(const std::string &path);

  // Directory operations
  bool create_directory(const std::string &path);
  bool remove_directory(const std::string &path);
  std::vector<FileInfo> list_directory(const std::string &path);

  // Utility functions
  bool is_directory(const std::string &path);
  uint32_t file_size(const std::string &path);

  // Card info
  CardType get_card_type() const { return this->card_type_; }
  bool is_mounted() const { return this->is_mounted_; }
  uint64_t get_total_bytes() const { return this->total_bytes_; }
  uint64_t get_used_bytes() const { return this->used_bytes_; }

  // NEW: Public mount/unmount methods for external control
  bool mount_card();
  void unmount_card();

  // NEW: Mount notification system for storage consumers
  void add_mount_ready_callback(const mount_ready_callback_t &callback) {
    this->mount_ready_callbacks_.push_back(callback);
  }
  const std::string &get_mount_path() const { return this->mount_path_; }

 private:
  // Pin configuration
  uint8_t clk_pin_{0};
  uint8_t cmd_pin_{0};
  uint8_t data0_pin_{0};
  uint8_t data1_pin_{0};
  uint8_t data2_pin_{0};
  uint8_t data3_pin_{0};
  uint8_t power_ctrl_pin_{0};
  bool mode_1bit_{false};
  uint8_t slot_{0};

  // Card state
  CardType card_type_{CardType::UNKNOWN};
  bool is_mounted_{false};
  uint64_t total_bytes_{0};
  uint64_t used_bytes_{0};

  // NEW: Mount configuration and callbacks
  std::string mount_path_{"/sdcard"};                          // Default mount path
  std::vector<mount_ready_callback_t> mount_ready_callbacks_;  // Callbacks to notify when mount is ready

  // Mount management (internal)
  bool update_card_info();
};

}  // namespace sd_mmc_card
}  // namespace esphome
