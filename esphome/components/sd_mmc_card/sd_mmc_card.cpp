#include "sd_mmc_card.h"
#include "esphome/core/log.h"
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace sd_mmc_card {

static sdmmc_host_t host = SDMMC_HOST_DEFAULT();
static sdmmc_card_t *card = nullptr;

void SdMmc::setup() {
  ESP_LOGI(TAG, "Initializing SD/MMC card");
  ESP_LOGI(TAG, "  CLK pin: %d, CMD pin: %d, DATA0 pin: %d", this->clk_pin_, this->cmd_pin_, this->data0_pin_);

  if (this->power_ctrl_pin_ != 0) {
    ESP_LOGI(TAG, "  Power control pin: %d", this->power_ctrl_pin_);
  }

  if (!this->mount_card()) {
    ESP_LOGE(TAG, "Failed to mount SD/MMC card");
    this->mark_failed();
  }
}

void SdMmc::loop() {
  // Nothing to do in loop
}

void SdMmc::dump_config() {
  ESP_LOGCONFIG(TAG, "SD/MMC Card:");
  ESP_LOGCONFIG(TAG, "  Mounted: %s", this->is_mounted_ ? "YES" : "NO");

  if (this->is_mounted_) {
    ESP_LOGCONFIG(TAG, "  Card Type: %d", static_cast<uint8_t>(this->card_type_));
    ESP_LOGCONFIG(TAG, "  Total bytes: %" PRIu64, this->total_bytes_);
    ESP_LOGCONFIG(TAG, "  Used bytes: %" PRIu64, this->used_bytes_);
  }
}

bool SdMmc::mount_card() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_0 + this->slot_;  // Utilise le slot configuré
  host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;     // 50MHz

  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.width = this->mode_1bit_ ? 1 : 4;

  // Configure SDMMC host
  host.flags = SDMMC_HOST_FLAG_4BIT;
  if (this->mode_1bit_) {
    host.flags = SDMMC_HOST_FLAG_1BIT;
  }
  host.slot = this->slot_;

#ifdef SOC_SDMMC_USE_GPIO_MATRIX
  // Configure pins
  slot_config.clk = static_cast<gpio_num_t>(this->clk_pin_);
  slot_config.cmd = static_cast<gpio_num_t>(this->cmd_pin_);
  slot_config.d0 = static_cast<gpio_num_t>(this->data0_pin_);
  if (!this->mode_1bit_) {
    slot_config.d1 = static_cast<gpio_num_t>(this->data1_pin_);
    slot_config.d2 = static_cast<gpio_num_t>(this->data2_pin_);
    slot_config.d3 = static_cast<gpio_num_t>(this->data3_pin_);
  }
#endif

  // Enable internal pull-ups for SD card communication
  slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  // Initialize host
  ESP_LOGI(TAG, "Initializing SDMMC slot %d", this->slot_);
  esp_err_t ret = sdmmc_host_init_slot(host.slot, &slot_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to init SDMMC slot %d: %s", this->slot_, esp_err_to_name(ret));
    return false;
  }

  // Mount filesystem with retry logic
  const char *mount_point = "/sdcard";
  const esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 16,
      .allocation_unit_size = 256 * 1024,
  };

  // Allocate card structure
  card = (sdmmc_card_t *) malloc(sizeof(sdmmc_card_t));

  // Attempt to mount with retries
  ret = ESP_FAIL;
  for (int attempt = 1; attempt <= 3; attempt++) {
    ESP_LOGI(TAG, "Mounting SD card on slot %d (attempt %d/3)...", this->slot_, attempt);
    ret = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (ret == ESP_OK) {
      ESP_LOGI(TAG, "SD card mounted successfully on slot %d!", this->slot_);
      break;
    }
    ESP_LOGW(TAG, "Mount attempt %d failed: %s", attempt, esp_err_to_name(ret));
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait 100ms between attempts
  }

  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to mount filesystem: %s", esp_err_to_name(ret));
    free(card);
    card = nullptr;
    return false;
  }

  // Determine card type
  if (card->is_mmc) {
    this->card_type_ = CardType::MMC;
  } else if (card->is_sdio) {
    this->card_type_ = CardType::SDIO;
  } else {
    // Check if it's SDHC/SDXC (High Capacity) by looking at OCR register
    // Bit 30 of OCR indicates if card is SDHC/SDXC
    if (card->ocr & (1 << 30)) {
      this->card_type_ = CardType::SDHC;
    } else {
      this->card_type_ = CardType::SDSC;
    }
  }

  this->is_mounted_ = true;
  this->update_card_info();

  ESP_LOGI(TAG, "SD/MMC card mounted successfully at %s", mount_point);
  return true;
}

void SdMmc::unmount_card() {
  if (this->is_mounted_ && card != nullptr) {
    esp_vfs_fat_sdcard_unmount("/sdcard", card);
    free(card);
    card = nullptr;
    this->is_mounted_ = false;
  }
}

bool SdMmc::update_card_info() {
  if (!this->is_mounted_ || card == nullptr) {
    return false;
  }

  // Get card info
  this->total_bytes_ = (uint64_t) card->csd.capacity * card->csd.sector_size;

  // Get filesystem usage (simplified)
  // Note: In production, you'd use statfs() for accurate usage
  this->used_bytes_ = 0;

  return true;
}

bool SdMmc::write_file(const std::string &path, const std::string &data) {
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "Card not mounted, cannot write file");
    return false;
  }

  std::string full_path = "/sdcard/" + path;
  std::ofstream file(full_path, std::ios::binary);

  if (!file.is_open()) {
    ESP_LOGW(TAG, "Failed to open file for writing: %s", full_path.c_str());
    return false;
  }

  file.write(data.c_str(), data.length());
  file.close();

  ESP_LOGD(TAG, "Wrote %d bytes to %s", data.length(), full_path.c_str());
  return true;
}

bool SdMmc::append_file(const std::string &path, const std::string &data) {
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "Card not mounted, cannot append to file");
    return false;
  }

  std::string full_path = "/sdcard/" + path;
  std::ofstream file(full_path, std::ios::binary | std::ios::app);

  if (!file.is_open()) {
    ESP_LOGW(TAG, "Failed to open file for appending: %s", full_path.c_str());
    return false;
  }

  file.write(data.c_str(), data.length());
  file.close();

  ESP_LOGD(TAG, "Appended %d bytes to %s", data.length(), full_path.c_str());
  return true;
}

std::string SdMmc::read_file(const std::string &path) {
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "Card not mounted, cannot read file");
    return "";
  }

  std::string full_path = "/sdcard/" + path;
  std::ifstream file(full_path, std::ios::binary);

  if (!file.is_open()) {
    ESP_LOGW(TAG, "Failed to open file for reading: %s", full_path.c_str());
    return "";
  }

  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();

  ESP_LOGD(TAG, "Read %d bytes from %s", content.length(), full_path.c_str());
  return content;
}

bool SdMmc::delete_file(const std::string &path) {
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "Card not mounted, cannot delete file");
    return false;
  }

  std::string full_path = "/sdcard/" + path;

  if (remove(full_path.c_str()) == 0) {
    ESP_LOGD(TAG, "Deleted file: %s", full_path.c_str());
    return true;
  } else {
    ESP_LOGW(TAG, "Failed to delete file: %s", full_path.c_str());
    return false;
  }
}

bool SdMmc::create_directory(const std::string &path) {
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "Card not mounted, cannot create directory");
    return false;
  }

  std::string full_path = "/sdcard/" + path;

  if (mkdir(full_path.c_str(), 0755) == 0) {
    ESP_LOGD(TAG, "Created directory: %s", full_path.c_str());
    return true;
  } else {
    ESP_LOGW(TAG, "Failed to create directory: %s (errno: %d)", full_path.c_str(), errno);
    return false;
  }
}

bool SdMmc::remove_directory(const std::string &path) {
  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "Card not mounted, cannot remove directory");
    return false;
  }

  std::string full_path = "/sdcard/" + path;

  if (rmdir(full_path.c_str()) == 0) {
    ESP_LOGD(TAG, "Removed directory: %s", full_path.c_str());
    return true;
  } else {
    ESP_LOGW(TAG, "Failed to remove directory: %s", full_path.c_str());
    return false;
  }
}

bool SdMmc::is_directory(const std::string &path) {
  if (!this->is_mounted_) {
    return false;
  }

  std::string full_path = "/sdcard/" + path;
  struct stat path_stat;

  if (stat(full_path.c_str(), &path_stat) != 0) {
    return false;
  }

  return S_ISDIR(path_stat.st_mode);
}

uint32_t SdMmc::file_size(const std::string &path) {
  if (!this->is_mounted_) {
    return 0;
  }

  std::string full_path = "/sdcard/" + path;
  struct stat path_stat;

  if (stat(full_path.c_str(), &path_stat) != 0) {
    return 0;
  }

  return path_stat.st_size;
}

std::vector<FileInfo> SdMmc::list_directory(const std::string &path) {
  std::vector<FileInfo> files;

  if (!this->is_mounted_) {
    ESP_LOGW(TAG, "Card not mounted, cannot list directory");
    return files;
  }

  std::string full_path = "/sdcard/" + path;
  DIR *dir = opendir(full_path.c_str());

  if (dir == nullptr) {
    ESP_LOGW(TAG, "Failed to open directory: %s (errno: %d)", full_path.c_str(), errno);
    return files;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] != '.') {  // Skip hidden files
      FileInfo info;
      info.path = entry->d_name;
      info.is_directory = entry->d_type == DT_DIR;

      if (info.is_directory) {
        info.size = 0;
      } else {
        info.size = file_size(path + "/" + entry->d_name);
      }

      files.push_back(info);
    }
  }

  closedir(dir);
  ESP_LOGD(TAG, "Listed %d entries in %s", files.size(), full_path.c_str());
  return files;
}

}  // namespace sd_mmc_card
}  // namespace esphome
