#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/components/storage/storage.h"
#include <cstdint>

#ifdef USE_ESP_IDF
#include <esp_vfs.h>
#include <ff.h>

namespace esphome::sd_storage {

// Note: SDHC and SDXC cards cannot be distinguished by the OCR capacity bit alone (that only
// separates SDSC from "high/extended capacity"); doing so would require checking the card's
// actual reported capacity. No driver currently assigns SDXC, so it's omitted here rather than
// kept as a value nothing ever produces.
enum class CardType : uint8_t {
  UNKNOWN = 0,
  SDIO = 1,
  MMC = 2,
  SDHC = 3,
  SDSC = 4,
};

struct SdFileHandle : public storage::FileHandle {
  char path_buf[(ESP_VFS_PATH_MAX + CONFIG_FATFS_MAX_LFN + 1)]{};
};

template<typename... Ts> class MountCardAction;
template<typename... Ts> class UnmountCardAction;
template<typename... Ts> class ListFilesAction;

// Base class for both SDMMC and SPI implementations.
// Extends FilesystemStorage so both SdMmc and SdSpi satisfy the storage interface.
class SdStorageBase : public storage::FilesystemStorage {
 public:
  void set_mount_path(const char *path) { this->set_mount_path_(path); }
  void set_id(const char *id) { this->storage_id_ = id; }
  void set_cd_pin(GPIOPin *pin) { this->cd_pin_ = pin; }
  bool is_mounted() const { return this->is_mounted_; }

  template<typename F> void add_on_mounted_callback(F &&cb) { this->on_mounted_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_removed_callback(F &&cb) { this->on_removed_.add(std::forward<F>(cb)); }
  template<typename F> void add_on_inserted_callback(F &&cb) { this->on_inserted_.add(std::forward<F>(cb)); }

  // Storage base interface
  storage::StorageError get_info(storage::StorageInfo *info) override;

  // FilesystemStorage virtuals — common POSIX implementations (subclasses provide mount/unmount)
  storage::StorageError format() override;
  storage::StorageError sync() override;
  storage::StorageError open(const char *path, storage::FileHandle *&handle, storage::OpenMode mode) override;
  storage::StorageError close(storage::FileHandle *handle) override;
  storage::StorageError read(storage::FileHandle *handle, uint8_t *buf, size_t len, size_t *bytes_transferred) override;
  storage::StorageError write(storage::FileHandle *handle, const uint8_t *buf, size_t len,
                              size_t *bytes_transferred) override;
  storage::StorageError seek(storage::FileHandle *handle, int64_t offset, storage::SeekMode mode) override;
  storage::StorageError tell(storage::FileHandle *handle, uint64_t *position) override;
  storage::StorageError stat(const char *path, storage::FileStat *stat) override;
  storage::StorageError list_dir(const char *path, bool (*callback)(const storage::FileStat *entry, void *ctx),
                                 void *ctx) override;
  storage::StorageError mkdir(const char *path) override;
  storage::StorageError rmdir(const char *path) override;
  storage::StorageError remove(const char *path) override;
  storage::StorageError rename(const char *old_path, const char *new_path) override;

 protected:
  static constexpr int MAX_OPEN_FILES = 4;

  // Subclasses provide handle pool and card capacity/space info
  virtual SdFileHandle *get_handle_pool() = 0;
  virtual uint64_t get_free_bytes_impl() const = 0;
  virtual uint32_t get_block_size_impl() const = 0;

  // Build absolute VFS path from a relative path into caller-supplied buffer.
  // Returns false if the result would exceed buf_size.
  bool build_full_path_(const char *rel_path, char *buf, size_t buf_size) const;

  // Builds a FATFS-native path ("N:/dir/file") from a path relative to this device's mount
  // point, using fatfs_drive_ (set by the subclass's mount() via set_fatfs_drive_() below).
  // Returns false if the result would exceed buf_size.
  bool build_fatfs_path_(const char *rel_path, char *buf, size_t buf_size) const;

  // Captures the FATFS drive string ("N:") for this card. Called by SdMmc::mount()/SdSpi::mount()
  // after a successful mount, since only the subclass holds the sdmmc_card_t* needed to look it
  // up via ff_diskio_get_pdrv_card() (diskio_sdmmc.h).
  void set_fatfs_drive_(BYTE pdrv) { snprintf(this->fatfs_drive_, sizeof(this->fatfs_drive_), "%u:", pdrv); }

  // Closes every still-open handle before unmount, so nothing is left holding a FILE* into a
  // filesystem that's about to disappear. Best-effort: keeps going even if a flush/close fails
  // partway through, and returns the first error seen (StorageError::OK if none did).
  storage::StorageError flush_open_handles_();

  // Debounced card-detect read. Always true if no CD pin is configured (cd_pin_ == nullptr),
  // matching the behavior of a card that's simply always present as far as this feature is
  // concerned. Active-low by convention (a CD switch pulls the pin low when a card is seated);
  // cd_pin_'s own GPIOPin-level inverted: flag (from the pin schema) overrides this for
  // hardware wired the other way — no separate inversion setting exists on top of that.
  //
  // Must be called every loop() iteration (not just at the ~100ms poll interval — see CD3):
  // it tracks the raw reading continuously so a debounce window measured from the first
  // observed change is accurate regardless of when the caller last polled.
  bool card_present_();

  // True at most once every CD_POLL_MS — gates how often a driver's loop() acts on
  // card_present_()'s result (auto-mount/unmount), independent of how often loop() itself
  // runs. card_present_() itself must still be called every iteration regardless (see above).
  bool should_poll_cd_();

  // Friended so automation.h's action templates (separate classes) can call the protected
  // logging helpers below without making them part of the public interface.
  template<typename... Ts> friend class MountCardAction;
  template<typename... Ts> friend class UnmountCardAction;
  template<typename... Ts> friend class ListFilesAction;

  void log_mount_result_(bool success) const;
  void log_unmount_() const;
  void log_list_dir_start_(const char *path) const;
  // Matches the list_dir() callback signature (bool return = keep enumerating).
  static bool log_list_dir_entry(const storage::FileStat *entry, void *ctx);
  static const char *card_type_to_string(CardType type);

  CardType card_type_{CardType::UNKNOWN};
  bool is_mounted_{false};
  uint64_t total_bytes_{0};
  uint64_t used_bytes_{0};
  const char *storage_id_{nullptr};
  GPIOPin *cd_pin_{nullptr};
  char fatfs_drive_[3]{};  // "N:" — set via set_fatfs_drive_() after a successful mount

  LazyCallbackManager<void(const char *)> on_mounted_;
  LazyCallbackManager<void()> on_removed_;
  LazyCallbackManager<void()> on_inserted_;

 private:
  // Debounce state for card_present_(). last_confirmed_ is the last debounced, accepted
  // reading; candidate_ is the raw reading currently under observation, timestamped by when it
  // first differed from last_confirmed_ so the debounce window is wall-clock based rather than
  // counted in loop() iterations (whose frequency varies with the rest of the node's config).
  bool cd_debounce_started_{false};
  bool last_confirmed_present_{true};
  bool candidate_present_{true};
  uint32_t candidate_since_ms_{0};
  static constexpr uint32_t CD_DEBOUNCE_MS = 50;

  // Poll-interval gate for should_poll_cd_() — separate from the debounce window above, since
  // the debounce tracking itself must run every loop() call regardless of this interval.
  bool cd_poll_started_{false};
  uint32_t last_cd_poll_ms_{0};
  static constexpr uint32_t CD_POLL_MS = 100;
};

}  // namespace esphome::sd_storage

#endif  // USE_ESP_IDF
