#if defined(USE_ZEPHYR) && !defined(USE_NRF52)
#include "ota_backend_zephyr.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cstdint>
#include <cstring>

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
#include <cerrno>
#include <cstdio>
#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <elf.h>
#include <endian.h>
#endif

namespace esphome::zephyr {
const std::string &get_exe_path();
void arm_reexec(const std::string &path);
}  // namespace esphome::zephyr
#else
#include <zephyr/dfu/mcuboot.h>
#endif

namespace esphome::ota {

namespace {

const char *const TAG = "ota.zephyr";

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM
constexpr size_t MAX_OTA_SIZE = 256u * 1024u * 1024u;  // 256 MiB
constexpr size_t HEADER_PEEK_SIZE = 64;

ssize_t read_header_(const char *path, uint8_t *buf, size_t len) {
  int fd = ::open(path, O_RDONLY);
  if (fd < 0)
    return -1;
  ssize_t got = ::read(fd, buf, len);
  ::close(fd);
  return got;
}

#ifdef __linux__
struct ElfIdent {
  bool valid;
  uint8_t ei_class;
  uint8_t ei_data;
  uint16_t e_machine;
  uint16_t e_type;
};

ElfIdent parse_elf_(const uint8_t *buf, size_t len) {
  ElfIdent out{};
  if (len < EI_NIDENT + 4)
    return out;
  if (buf[EI_MAG0] != ELFMAG0 || buf[EI_MAG1] != ELFMAG1 || buf[EI_MAG2] != ELFMAG2 || buf[EI_MAG3] != ELFMAG3)
    return out;
  out.ei_class = buf[EI_CLASS];
  out.ei_data = buf[EI_DATA];
  uint16_t e_type;
  uint16_t e_machine;
  std::memcpy(&e_type, buf + 16, sizeof(e_type));
  std::memcpy(&e_machine, buf + 18, sizeof(e_machine));
  if (out.ei_data == ELFDATA2LSB) {
    out.e_type = le16toh(e_type);
    out.e_machine = le16toh(e_machine);
  } else if (out.ei_data == ELFDATA2MSB) {
    out.e_type = be16toh(e_type);
    out.e_machine = be16toh(e_machine);
  } else {
    return out;
  }
  out.valid = true;
  return out;
}

bool validate_elf_(const char *staging_path, const std::string &exe_path) {
  uint8_t new_buf[HEADER_PEEK_SIZE];
  uint8_t cur_buf[HEADER_PEEK_SIZE];
  ssize_t new_n = read_header_(staging_path, new_buf, sizeof(new_buf));
  ssize_t cur_n = read_header_(exe_path.c_str(), cur_buf, sizeof(cur_buf));
  if (new_n < static_cast<ssize_t>(EI_NIDENT + 4) || cur_n < static_cast<ssize_t>(EI_NIDENT + 4)) {
    ESP_LOGE(TAG, "ELF header read failed");
    return false;
  }
  ElfIdent new_id = parse_elf_(new_buf, new_n);
  ElfIdent cur_id = parse_elf_(cur_buf, cur_n);
  if (!new_id.valid) {
    ESP_LOGE(TAG, "Uploaded payload is not a valid ELF");
    return false;
  }
  if (!cur_id.valid) {
    ESP_LOGE(TAG, "Could not parse running exe ELF header");
    return false;
  }
  if (new_id.ei_class != cur_id.ei_class) {
    ESP_LOGE(TAG, "ELF class mismatch (uploaded=%u, running=%u)", new_id.ei_class, cur_id.ei_class);
    return false;
  }
  if (new_id.ei_data != cur_id.ei_data) {
    ESP_LOGE(TAG, "ELF endianness mismatch");
    return false;
  }
  if (new_id.e_machine != cur_id.e_machine) {
    ESP_LOGE(TAG, "ELF e_machine mismatch (uploaded=0x%04x, running=0x%04x)", new_id.e_machine, cur_id.e_machine);
    return false;
  }
  if (new_id.e_type != ET_EXEC && new_id.e_type != ET_DYN) {
    ESP_LOGE(TAG, "ELF e_type=%u is not executable", new_id.e_type);
    return false;
  }
  return true;
}
#endif  // __linux__

bool validate_executable_(const char *staging_path, const std::string &exe_path) {
#ifdef __linux__
  return validate_elf_(staging_path, exe_path);
#else
  (void) staging_path;
  (void) exe_path;
  ESP_LOGE(TAG, "Zephyr OTA validation not implemented for this OS");
  return false;
#endif
}
#endif  // USE_ZEPHYR_VARIANT_NATIVE_SIM

}  // namespace

std::unique_ptr<ZephyrOTABackend> make_ota_backend() { return make_unique<ZephyrOTABackend>(); }

void ZephyrOTABackend::set_update_md5(const char *md5) {
  if (parse_hex(md5, this->expected_md5_, 16))
    this->md5_set_ = true;
}

#ifdef USE_ZEPHYR_VARIANT_NATIVE_SIM

OTAResponseTypes ZephyrOTABackend::begin(size_t image_size, OTAType ota_type) {
  if (ota_type != OTA_TYPE_UPDATE_APP)
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  if (image_size > MAX_OTA_SIZE) {
    ESP_LOGE(TAG, "Refusing OTA of size %zu (exceeds %zu)", image_size, MAX_OTA_SIZE);
    return OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }

  const std::string &exe = zephyr::get_exe_path();
  if (exe.empty()) {
    ESP_LOGE(TAG, "Could not resolve running executable path; cannot stage OTA");
    return OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }
  this->final_path_ = exe;
  this->staging_path_ = exe + ".ota.new";

  ::unlink(this->staging_path_.c_str());

  this->fd_ = ::open(this->staging_path_.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0755);
  if (this->fd_ < 0) {
    ESP_LOGE(TAG, "Open '%s' failed: %s", this->staging_path_.c_str(), std::strerror(errno));
    return OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }

  this->expected_size_ = image_size;
  this->bytes_written_ = 0;
  this->md5_set_ = false;
  this->md5_.init();

  ESP_LOGD(TAG, "OTA begin: staging=%s, size=%zu", this->staging_path_.c_str(), image_size);
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ZephyrOTABackend::write(const uint8_t *data, size_t len) {
  if (this->fd_ < 0)
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  size_t limit = this->expected_size_ != 0 ? this->expected_size_ : MAX_OTA_SIZE;
  if (this->bytes_written_ + len > limit) {
    ESP_LOGE(TAG, "Write past size limit (%zu)", limit);
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }

  size_t remaining = len;
  const uint8_t *p = data;
  while (remaining > 0) {
    ssize_t n = ::write(this->fd_, p, remaining);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      ESP_LOGE(TAG, "Write failed: %s", std::strerror(errno));
      return OTA_RESPONSE_ERROR_WRITING_FLASH;
    }
    p += n;
    remaining -= n;
  }
  this->md5_.add(data, len);
  this->bytes_written_ += len;
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ZephyrOTABackend::end() {
  if (this->fd_ < 0)
    return OTA_RESPONSE_ERROR_UPDATE_END;

  if (this->bytes_written_ == 0) {
    ESP_LOGE(TAG, "OTA ended with no data written");
    this->abort();
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }
  if (this->expected_size_ != 0 && this->bytes_written_ != this->expected_size_) {
    ESP_LOGE(TAG, "Size mismatch: got %zu, expected %zu", this->bytes_written_, this->expected_size_);
    this->abort();
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_bytes(this->expected_md5_)) {
      ESP_LOGE(TAG, "MD5 mismatch");
      this->abort();
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  }

  if (::fsync(this->fd_) != 0) {
    ESP_LOGW(TAG, "fsync failed: %s", std::strerror(errno));
  }
  ::close(this->fd_);
  this->fd_ = -1;

  if (!validate_executable_(this->staging_path_.c_str(), this->final_path_)) {
    ::unlink(this->staging_path_.c_str());
    this->staging_path_.clear();
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  if (::chmod(this->staging_path_.c_str(), 0755) != 0) {
    ESP_LOGW(TAG, "chmod failed: %s", std::strerror(errno));
  }

  if (::rename(this->staging_path_.c_str(), this->final_path_.c_str()) != 0) {
    ESP_LOGE(TAG, "rename '%s' -> '%s' failed: %s", this->staging_path_.c_str(), this->final_path_.c_str(),
             std::strerror(errno));
    ::unlink(this->staging_path_.c_str());
    this->staging_path_.clear();
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  zephyr::arm_reexec(this->final_path_);
  this->staging_path_.clear();
  ESP_LOGI(TAG, "OTA staged at %s; will re-exec on reboot", this->final_path_.c_str());
  return OTA_RESPONSE_OK;
}

void ZephyrOTABackend::abort() {
  if (this->fd_ >= 0) {
    ::close(this->fd_);
    this->fd_ = -1;
  }
  if (!this->staging_path_.empty()) {
    ::unlink(this->staging_path_.c_str());
    this->staging_path_.clear();
  }
  this->expected_size_ = 0;
  this->bytes_written_ = 0;
  this->md5_set_ = false;
}

#else  // real Zephyr hardware under MCUboot

// flash_img_init() targets the secondary (slot1) partition, which is the
// standard MCUboot upload target on every dual-slot Zephyr board -- no
// per-chip area id needed here, same as esp_idf's esp_ota_get_next_update_partition().
OTAResponseTypes ZephyrOTABackend::begin(size_t image_size, OTAType ota_type) {
  if (ota_type != OTA_TYPE_UPDATE_APP)
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;

  int err = flash_img_init(&this->img_ctx_);
  if (err != 0) {
    ESP_LOGE(TAG, "flash_img_init failed: %d", err);
    return OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }

  this->expected_size_ = image_size;
  this->bytes_written_ = 0;
  this->md5_set_ = false;
  this->active_ = true;
  this->md5_.init();

  ESP_LOGD(TAG, "OTA begin: size=%zu", image_size);
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ZephyrOTABackend::write(const uint8_t *data, size_t len) {
  if (!this->active_)
    return OTA_RESPONSE_ERROR_WRITING_FLASH;

  int err = flash_img_buffered_write(&this->img_ctx_, data, len, false);
  if (err != 0) {
    ESP_LOGE(TAG, "flash_img_buffered_write failed: %d", err);
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }
  this->md5_.add(data, len);
  this->bytes_written_ += len;
  return OTA_RESPONSE_OK;
}

OTAResponseTypes ZephyrOTABackend::end() {
  if (!this->active_)
    return OTA_RESPONSE_ERROR_UPDATE_END;

  int err = flash_img_buffered_write(&this->img_ctx_, nullptr, 0, true);
  this->active_ = false;
  if (err != 0) {
    ESP_LOGE(TAG, "flash_img_buffered_write flush failed: %d", err);
    return OTA_RESPONSE_ERROR_WRITING_FLASH;
  }

  if (this->expected_size_ != 0 && this->bytes_written_ != this->expected_size_) {
    ESP_LOGE(TAG, "Size mismatch: got %zu, expected %zu", this->bytes_written_, this->expected_size_);
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  if (this->md5_set_) {
    this->md5_.calculate();
    if (!this->md5_.equals_bytes(this->expected_md5_)) {
      ESP_LOGE(TAG, "MD5 mismatch");
      return OTA_RESPONSE_ERROR_MD5_MISMATCH;
    }
  }

  // Mark the secondary slot pending as a test boot: MCUboot will swap it in
  // on the next reset, and it must be explicitly confirmed after a verified-good
  // boot (safe_mode does this under USE_OTA_ROLLBACK) or it reverts automatically.
  err = boot_request_upgrade(BOOT_UPGRADE_TEST);
  if (err != 0) {
    ESP_LOGE(TAG, "boot_request_upgrade failed: %d", err);
    return OTA_RESPONSE_ERROR_UPDATE_END;
  }

  ESP_LOGI(TAG, "OTA staged; will swap and boot-test on reboot");
  return OTA_RESPONSE_OK;
}

void ZephyrOTABackend::abort() {
  // No explicit cancel API: as long as boot_request_upgrade() was never reached, MCUboot
  // never sees a pending-swap trailer, so a partially-written image is just ignored on
  // next boot -- same trust model as esp_idf's esp_ota_abort().
  this->active_ = false;
  this->expected_size_ = 0;
  this->bytes_written_ = 0;
  this->md5_set_ = false;
}

#endif  // USE_ZEPHYR_VARIANT_NATIVE_SIM

}  // namespace esphome::ota
#endif  // USE_ZEPHYR && !USE_NRF52
