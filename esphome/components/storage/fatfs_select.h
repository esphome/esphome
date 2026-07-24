#pragma once
// file_system selection for FatFs-backed media (sd_storage / usb_storage).
//
// Only exists in builds with esp32 enable_exfat -- codegen defines
// USE_STORAGE_FILE_SYSTEM_SELECT together with the YAML option (see
// storage/__init__.py: file_system_to_code). Builds without exFAT contain none of this and
// their mount paths are untouched.
//
// Header-only on purpose: compiled where included, so the storage component itself takes no
// FatFs dependency.

#ifdef USE_STORAGE_FILE_SYSTEM_SELECT

#include "diskio.h"  // NOLINT(modernize-deprecated-headers) - FatFs's own header
#include "ff.h"

#include <cstring>
#include <memory>

namespace esphome::storage {

// Values match the codegen mapping in storage/__init__.py file_system_to_code().
static constexpr uint8_t FS_SELECT_AUTO = 0;
static constexpr uint8_t FS_SELECT_FAT32 = 1;
static constexpr uint8_t FS_SELECT_EXFAT = 2;

// Logging for this header. Defined in storage.cpp so logging stays out of headers; the
// signatures name no FatFs type, so storage.cpp needs no FatFs include.
void fatfs_log_probe_read_failed(const char *tag);
void fatfs_log_reformat_no_filesystem(const char *tag, bool want_exfat);
void fatfs_log_reformat_mismatch(const char *tag, bool found_exfat, bool want_exfat);
void fatfs_log_format_failed(const char *tag, bool want_exfat, int result);
void fatfs_log_format_done(const char *tag, bool want_exfat);

// Internal helpers for boot-sector/partition probing, kept inline in this header (they are
// used only by ensure_requested_filesystem below). Names are prefixed to avoid clashing with
// any other storage-scope symbols.
// NONE means the medium was readable and carries nothing this code recognises. UNREADABLE
// means the question could not be answered at all -- the two must stay apart, because the
// caller reformats on the first and must not on the second.
enum class FatfsDetected : uint8_t { NONE, UNREADABLE, FAT, EXFAT };

inline bool fatfs_has_boot_signature(const uint8_t *sec) { return sec[510] == 0x55 && sec[511] == 0xAA; }

// What filesystem sits in this boot sector? The same first-bytes FatFs itself keys on:
// the exFAT name field ("EXFAT   " at offset 3) and the FAT jump instruction + signature.
inline FatfsDetected fatfs_classify_boot_sector(const uint8_t *sec) {
  if (!fatfs_has_boot_signature(sec))
    return FatfsDetected::NONE;
  if (memcmp(sec + 3, "EXFAT   ", 8) == 0)
    return FatfsDetected::EXFAT;
  if (sec[0] == 0xEB || sec[0] == 0xE9)
    return FatfsDetected::FAT;
  return FatfsDetected::NONE;
}

// Probe the medium BEFORE any mount: sector 0 directly, and -- when sector 0 is a partition
// table instead of a boot sector -- one level of indirection: the first MBR partition, or
// for a protective MBR (0xEE) the first GPT entry. Exactly the volumes FatFs would mount.
inline FatfsDetected fatfs_probe(const char *tag, uint8_t pdrv) {
  auto sec = std::make_unique<uint8_t[]>(FF_MAX_SS);
  if (disk_read(pdrv, sec.get(), 0, 1) != RES_OK) {
    fatfs_log_probe_read_failed(tag);
    return FatfsDetected::UNREADABLE;
  }
  FatfsDetected direct = fatfs_classify_boot_sector(sec.get());
  if (direct != FatfsDetected::NONE)
    return direct;
  if (!fatfs_has_boot_signature(sec.get()))
    return FatfsDetected::NONE;
  // Sector 0 is a partition table. First MBR entry: type at 0x1BE+4, start LBA at 0x1BE+8.
  const uint8_t *entry = sec.get() + 0x1BE;
  uint8_t part_type = entry[4];
  uint64_t start_lba = static_cast<uint32_t>(entry[8]) | (static_cast<uint32_t>(entry[9]) << 8) |
                       (static_cast<uint32_t>(entry[10]) << 16) | (static_cast<uint32_t>(entry[11]) << 24);
  if (part_type == 0xEE) {
    // Protective MBR -> GPT. Header at LBA 1: entry array start LBA at offset 72; the first
    // entry's first LBA at offset 32 within the entry.
    if (disk_read(pdrv, sec.get(), 1, 1) != RES_OK) {
      fatfs_log_probe_read_failed(tag);
      return FatfsDetected::UNREADABLE;
    }
    if (memcmp(sec.get(), "EFI PART", 8) != 0)
      return FatfsDetected::NONE;  // protective MBR without a GPT behind it
    uint64_t entries_lba = 0;
    memcpy(&entries_lba, sec.get() + 72, sizeof(entries_lba));
    if (disk_read(pdrv, sec.get(), static_cast<LBA_t>(entries_lba), 1) != RES_OK) {
      fatfs_log_probe_read_failed(tag);
      return FatfsDetected::UNREADABLE;
    }
    memcpy(&start_lba, sec.get() + 32, sizeof(start_lba));
  }
  if (start_lba == 0)
    return FatfsDetected::NONE;  // no first partition to look into
  if (disk_read(pdrv, sec.get(), static_cast<LBA_t>(start_lba), 1) != RES_OK) {
    fatfs_log_probe_read_failed(tag);
    return FatfsDetected::UNREADABLE;
  }
  return fatfs_classify_boot_sector(sec.get());
}

// Make the medium carry the requested filesystem BEFORE the one and only mount happens.
// AUTO does nothing at all: f_mount's own boot-sector detection is the automatic mode.
// fat32/exfat probe the first sectors; a different (or no recognizable) filesystem is
// reformatted to the requested one right here -- destructive by configured contract, and the
// subsequent mount is then already on the correct filesystem. Returns false only when the
// reformat itself failed.
inline bool ensure_requested_filesystem(const char *tag, uint8_t pdrv, const char *drive, uint8_t requested) {
  if (requested == FS_SELECT_AUTO)
    return true;
  const bool want_exfat = requested == FS_SELECT_EXFAT;
  FatfsDetected found = fatfs_probe(tag, pdrv);
  if (found == FatfsDetected::UNREADABLE) {
    // The probe could not read the medium, which says nothing about what is on it. Formatting
    // here would destroy a perfectly good filesystem over a card that was not ready yet or a
    // connector that flickered. Leave it alone and let the mount below report the trouble.
    return true;
  }
  if ((found == FatfsDetected::EXFAT) == want_exfat && found != FatfsDetected::NONE)
    return true;
  if (found == FatfsDetected::NONE) {
    fatfs_log_reformat_no_filesystem(tag, want_exfat);
  } else {
    fatfs_log_reformat_mismatch(tag, found == FatfsDetected::EXFAT, want_exfat);
  }
  auto work = std::make_unique<uint8_t[]>(FF_MAX_SS);
  MKFS_PARM parm{};
  // FAT side deliberately FM_FAT | FM_FAT32: the request means "the FAT family, not exFAT";
  // FatFs picks the FAT width the medium size allows (forcing FAT32 on tiny media fails).
  parm.fmt = want_exfat ? FM_EXFAT : (FM_FAT | FM_FAT32);
  FRESULT res = f_mkfs(drive, &parm, work.get(), FF_MAX_SS);
  if (res != FR_OK) {
    fatfs_log_format_failed(tag, want_exfat, static_cast<int>(res));
    return false;
  }
  fatfs_log_format_done(tag, want_exfat);
  return true;
}

}  // namespace esphome::storage

#endif  // USE_STORAGE_FILE_SYSTEM_SELECT
