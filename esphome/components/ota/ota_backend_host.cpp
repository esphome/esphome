#ifdef USE_HOST
#include "ota_backend_host.h"

#include "esphome/components/host/core.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <elf.h>
#include <endian.h>
#endif

#ifdef __APPLE__
#include <mach-o/loader.h>
#endif

namespace esphome::ota {

namespace {

const char *const TAG = "ota.host";

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
  // e_type @ 16, e_machine @ 18, both in EI_DATA endianness.
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

#ifdef __APPLE__
struct MachOIdent {
  bool valid;
  uint32_t cputype;
  uint32_t cpusubtype;
};

MachOIdent parse_macho_(const uint8_t *buf, size_t len) {
  MachOIdent out{};
  // mach_header is the common prefix of mach_header and mach_header_64;
  // cputype/cpusubtype/filetype have identical offsets in both.
  if (len < sizeof(struct mach_header))
    return out;
  uint32_t magic;
  std::memcpy(&magic, buf, sizeof(magic));
  bool swap;
  if (magic == MH_MAGIC || magic == MH_MAGIC_64) {
    swap = false;
  } else if (magic == MH_CIGAM || magic == MH_CIGAM_64) {
    swap = true;
  } else {
    return out;
  }
  struct mach_header hdr;
  std::memcpy(&hdr, buf, sizeof(hdr));
  if (swap) {
    hdr.cputype = OSSwapInt32(hdr.cputype);
    hdr.cpusubtype = OSSwapInt32(hdr.cpusubtype);
    hdr.filetype = OSSwapInt32(hdr.filetype);
  }
  if (hdr.filetype != MH_EXECUTE)
    return out;
  out.cputype = hdr.cputype;
  out.cpusubtype = hdr.cpusubtype;
  out.valid = true;
  return out;
}

bool validate_macho_(const char *staging_path, const std::string &exe_path) {
  uint8_t new_buf[HEADER_PEEK_SIZE];
  uint8_t cur_buf[HEADER_PEEK_SIZE];
  ssize_t new_n = read_header_(staging_path, new_buf, sizeof(new_buf));
  ssize_t cur_n = read_header_(exe_path.c_str(), cur_buf, sizeof(cur_buf));
  if (new_n < static_cast<ssize_t>(sizeof(struct mach_header)) ||
      cur_n < static_cast<ssize_t>(sizeof(struct mach_header))) {
    ESP_LOGE(TAG, "Mach-O header read failed");
    return false;
  }
  MachOIdent new_id = parse_macho_(new_buf, new_n);
  MachOIdent cur_id = parse_macho_(cur_buf, cur_n);
  if (!new_id.valid) {
    ESP_LOGE(TAG, "Uploaded payload is not a valid thin Mach-O executable");
    return false;
  }
  if (!cur_id.valid) {
    ESP_LOGE(TAG, "Could not parse running exe Mach-O header");
    return false;
  }
  if (new_id.cputype != cur_id.cputype || new_id.cpusubtype != cur_id.cpusubtype) {
    ESP_LOGE(TAG, "Mach-O arch mismatch (uploaded=0x%x/0x%x, running=0x%x/0x%x)", new_id.cputype, new_id.cpusubtype,
             cur_id.cputype, cur_id.cpusubtype);
    return false;
  }
  return true;
}
#endif  // __APPLE__

bool validate_executable_(const char *staging_path, const std::string &exe_path) {
#ifdef __linux__
  return validate_elf_(staging_path, exe_path);
#elif defined(__APPLE__)
  return validate_macho_(staging_path, exe_path);
#else
  (void) staging_path;
  (void) exe_path;
  ESP_LOGE(TAG, "Host OTA validation not implemented for this OS");
  return false;
#endif
}

}  // namespace

std::unique_ptr<HostOTABackend> make_ota_backend() { return make_unique<HostOTABackend>(); }

OTAResponseTypes HostOTABackend::begin(size_t image_size, OTAType ota_type) {
  if (ota_type != OTA_TYPE_UPDATE_APP)
    return OTA_RESPONSE_ERROR_UNSUPPORTED_OTA_TYPE;
  // 0 = unknown size (web_server multipart); cap at MAX_OTA_SIZE.
  if (image_size > MAX_OTA_SIZE) {
    ESP_LOGE(TAG, "Refusing OTA of size %zu (exceeds %zu)", image_size, MAX_OTA_SIZE);
    return OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }

  const std::string &exe = host::get_exe_path();
  if (exe.empty()) {
    ESP_LOGE(TAG, "Could not resolve running executable path; cannot stage OTA");
    return OTA_RESPONSE_ERROR_UPDATE_PREPARE;
  }
  this->final_path_ = exe;
  this->staging_path_ = exe + ".ota.new";

  // Clean up any leftover from a prior aborted OTA.
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

void HostOTABackend::set_update_md5(const char *md5) {
  if (parse_hex(md5, this->expected_md5_, 16))
    this->md5_set_ = true;
}

OTAResponseTypes HostOTABackend::write(uint8_t *data, size_t len) {
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

OTAResponseTypes HostOTABackend::end() {
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

  // arch_restart() (via App::safe_reboot) will execv this path with the original argv.
  host::arm_reexec(this->final_path_);
  this->staging_path_.clear();
  ESP_LOGI(TAG, "OTA staged at %s; will re-exec on reboot", this->final_path_.c_str());
  return OTA_RESPONSE_OK;
}

void HostOTABackend::abort() {
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

}  // namespace esphome::ota
#endif
