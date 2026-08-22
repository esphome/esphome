#include "inplace_kv.h"

#ifdef USE_BINARY_STORAGE_INPLACE_KV

#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace binary_storage {

static const char *const TAG = "binary_storage.inplace_kv";

// Half header: magic(4) generation(4). Entries follow. The half with the highest valid generation
// is active; generation 0 (or bad magic) means the half is unused/incomplete.
static const uint32_t HALF_MAGIC = 0x48564B49;  // "IKVH"
static const uint64_t HALF_HDR = 8;

// Entry: committed(1) live(1) key(4) len(2) value(len). The commit marker is written LAST.
static const uint64_t ENTRY_HDR = 8;
static const uint8_t COMMITTED = 0xA5;  // 0 marks free space (a half is zeroed before use)

static const uint64_t MIN_WINDOW = 256;

static uint32_t rd_u32(const uint8_t *p) {
  return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}
static void wr_u32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}
static uint16_t rd_u16(const uint8_t *p) { return (uint16_t) p[0] | ((uint16_t) p[1] << 8); }
static void wr_u16(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
}

bool InplaceKVStore::read_(uint64_t rel_offset, uint8_t *buf, size_t len) {
  if (this->device_ == nullptr || rel_offset + len > this->size_)
    return false;
  size_t done = 0;
  while (done < len) {
    size_t got = 0;
    if (this->device_->read(this->offset_ + rel_offset + done, buf + done, len - done, &got) !=
        storage::StorageError::STORAGE_ERROR_OK)
      return false;
    if (got == 0)
      return false;
    done += got;
  }
  return true;
}

bool InplaceKVStore::write_(uint64_t rel_offset, const uint8_t *buf, size_t len) {
  if (this->device_ == nullptr || rel_offset + len > this->size_)
    return false;
  size_t done = 0;
  while (done < len) {
    size_t put = 0;
    if (this->device_->write(this->offset_ + rel_offset + done, buf + done, len - done, &put) !=
        storage::StorageError::STORAGE_ERROR_OK)
      return false;
    if (put == 0)
      return false;
    done += put;
  }
  return true;
}

bool InplaceKVStore::zero_(uint64_t rel_offset, uint64_t len) {
  uint8_t chunk[64];
  memset(chunk, 0, sizeof(chunk));
  uint64_t done = 0;
  while (done < len) {
    size_t n = (size_t) ((len - done) < sizeof(chunk) ? (len - done) : sizeof(chunk));
    if (!this->write_(rel_offset + done, chunk, n))
      return false;
    done += n;
  }
  return true;
}

uint32_t InplaceKVStore::half_generation_(uint64_t half_off) {
  uint8_t hh[HALF_HDR];
  if (!this->read_(half_off, hh, HALF_HDR))
    return 0;
  if (rd_u32(hh) != HALF_MAGIC)
    return 0;
  return rd_u32(hh + 4);  // 0 here means incomplete/unused
}

bool InplaceKVStore::write_half_header_(uint64_t half_off, uint32_t generation) {
  uint8_t magic[4];
  wr_u32(magic, HALF_MAGIC);
  if (!this->write_(half_off, magic, 4))  // magic first
    return false;
  uint8_t gen[4];
  wr_u32(gen, generation);
  return this->write_(half_off + 4, gen, 4);  // generation LAST -> activation commit
}

bool InplaceKVStore::find_in_(uint64_t half_off, uint32_t key, uint64_t *entry_offset, uint16_t *value_len,
                              uint64_t *entries_end) {
  const uint64_t half_end = half_off + this->half_size_();
  uint64_t pos = half_off + HALF_HDR;
  bool found = false;
  uint64_t found_off = 0;
  uint16_t found_len = 0;
  while (pos + ENTRY_HDR <= half_end) {
    uint8_t eh[ENTRY_HDR];
    if (!this->read_(pos, eh, ENTRY_HDR))
      break;
    if (eh[0] != COMMITTED)
      break;
    uint8_t live = eh[1];
    uint32_t ekey = rd_u32(eh + 2);
    uint16_t elen = rd_u16(eh + 6);
    uint64_t total = ENTRY_HDR + elen;
    if (pos + total > half_end)
      break;
    if (live == 1 && ekey == key) {
      found = true;
      found_off = pos;
      found_len = elen;
    }
    pos += total;
  }
  if (entries_end != nullptr)
    *entries_end = pos;
  if (found) {
    if (entry_offset != nullptr)
      *entry_offset = found_off;
    if (value_len != nullptr)
      *value_len = found_len;
  }
  return found;
}

void InplaceKVStore::clear_live_before_(uint64_t half_off, uint32_t key, uint64_t keep_offset) {
  uint64_t pos = half_off + HALF_HDR;
  while (pos + ENTRY_HDR <= keep_offset) {
    uint8_t eh[ENTRY_HDR];
    if (!this->read_(pos, eh, ENTRY_HDR))
      break;
    if (eh[0] != COMMITTED)
      break;
    uint16_t elen = rd_u16(eh + 6);
    if (eh[1] == 1 && rd_u32(eh + 2) == key) {
      uint8_t dead = 0;
      this->write_(pos + 1, &dead, 1);
    }
    pos += ENTRY_HDR + elen;
  }
}

storage::StorageError InplaceKVStore::append_(uint32_t key, const uint8_t *data, size_t len) {
  uint64_t total = ENTRY_HDR + len;
  uint64_t end = 0;
  this->find_in_(this->active_off_, key, nullptr, nullptr, &end);
  if (end + total > this->active_off_ + this->half_size_()) {
    storage::StorageError c = this->compact_();
    if (c != storage::StorageError::STORAGE_ERROR_OK)
      return c;
    this->find_in_(this->active_off_, key, nullptr, nullptr, &end);
    if (end + total > this->active_off_ + this->half_size_())
      return storage::StorageError::STORAGE_ERROR_NO_SPACE;
  }

  uint8_t eh[ENTRY_HDR];
  eh[0] = 0;  // not yet committed
  eh[1] = 1;  // live
  wr_u32(eh + 2, key);
  wr_u16(eh + 6, (uint16_t) len);
  if (!this->write_(end, eh, ENTRY_HDR))
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
  if (len > 0 && !this->write_(end + ENTRY_HDR, data, len))
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
  uint8_t commit = COMMITTED;
  if (!this->write_(end, &commit, 1))  // commit marker last (atomic single byte)
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;

  this->clear_live_before_(this->active_off_, key, end);
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError InplaceKVStore::compact_() {
  const uint64_t src = this->active_off_;
  const uint64_t dst = this->inactive_half_();
  const uint64_t src_end = src + this->half_size_();
  const uint64_t dst_limit = dst + this->half_size_();

  if (!this->zero_(dst, this->half_size_()))
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;

  uint64_t wpos = dst + HALF_HDR;
  uint64_t pos = src + HALF_HDR;
  while (pos + ENTRY_HDR <= src_end) {
    uint8_t eh[ENTRY_HDR];
    if (!this->read_(pos, eh, ENTRY_HDR))
      break;
    if (eh[0] != COMMITTED)
      break;
    uint16_t elen = rd_u16(eh + 6);
    uint64_t total = ENTRY_HDR + elen;
    if (pos + total > src_end)
      break;
    if (eh[1] == 1) {
      uint32_t ekey = rd_u32(eh + 2);
      // Keep only the last live copy of a key: skip if a later live entry supersedes it.
      bool superseded = false;
      uint64_t sp = pos + total;
      while (sp + ENTRY_HDR <= src_end) {
        uint8_t sh[ENTRY_HDR];
        if (!this->read_(sp, sh, ENTRY_HDR) || sh[0] != COMMITTED)
          break;
        uint16_t sl = rd_u16(sh + 6);
        if (sh[1] == 1 && rd_u32(sh + 2) == ekey) {
          superseded = true;
          break;
        }
        sp += ENTRY_HDR + sl;
      }
      if (!superseded) {
        if (wpos + total > dst_limit)
          return storage::StorageError::STORAGE_ERROR_NO_SPACE;  // live set does not fit
        // Copy header (committed already set -- this half is not active yet) + value.
        uint8_t ne[ENTRY_HDR];
        ne[0] = COMMITTED;
        ne[1] = 1;
        wr_u32(ne + 2, ekey);
        wr_u16(ne + 6, elen);
        if (!this->write_(wpos, ne, ENTRY_HDR))
          return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
        if (elen > 0) {
          uint8_t vbuf[256];
          uint64_t vdone = 0;
          while (vdone < elen) {
            size_t chunk = (size_t) ((elen - vdone) < sizeof(vbuf) ? (elen - vdone) : sizeof(vbuf));
            if (!this->read_(pos + ENTRY_HDR + vdone, vbuf, chunk) ||
                !this->write_(wpos + ENTRY_HDR + vdone, vbuf, chunk))
              return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
            vdone += chunk;
          }
        }
        wpos += total;
      }
    }
    pos += total;
  }

  // All live entries are durable in dst; activate it by writing its generation last.
  if (!this->write_half_header_(dst, this->active_gen_ + 1))
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
  this->active_off_ = dst;
  this->active_gen_ += 1;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError InplaceKVStore::ensure_initialized() {
  if (this->initialized_)
    return storage::StorageError::STORAGE_ERROR_OK;
  if (this->device_ == nullptr || this->size_ < MIN_WINDOW)
    return storage::StorageError::STORAGE_ERROR_NOT_READY;
  storage::RawGeometry geo{};
  this->device_->get_raw_geometry(&geo);
  if ((geo.caps & storage::RAW_WRITE_NEEDS_ERASE) != 0) {
    ESP_LOGE(TAG, "Backing device needs erase; not an in-place medium");
    return storage::StorageError::STORAGE_ERROR_NOT_SUPPORTED;
  }
  uint32_t gen_a = this->half_generation_(0);
  uint32_t gen_b = this->half_generation_(this->half_size_());
  if (gen_a == 0 && gen_b == 0) {
    ESP_LOGW(TAG, "No valid KV half, formatting window");
    return this->format();
  }
  if (gen_a >= gen_b) {
    this->active_off_ = 0;
    this->active_gen_ = gen_a;
  } else {
    this->active_off_ = this->half_size_();
    this->active_gen_ = gen_b;
  }
  this->initialized_ = true;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError InplaceKVStore::format() {
  if (!this->zero_(0, this->size_))
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
  if (!this->write_half_header_(0, 1))  // half A active at generation 1; half B stays zeroed (gen 0)
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
  this->active_off_ = 0;
  this->active_gen_ = 1;
  this->initialized_ = true;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError InplaceKVStore::set(uint32_t key, const uint8_t *data, size_t len) {
  if (len > 0xFFFF)
    return storage::StorageError::STORAGE_ERROR_INVALID_ARGS;
  storage::StorageError err = this->ensure_initialized();
  if (err != storage::StorageError::STORAGE_ERROR_OK)
    return err;
  return this->append_(key, data, len);
}

storage::StorageError InplaceKVStore::get(uint32_t key, uint8_t *buf, size_t len, size_t *got) {
  *got = 0;
  storage::StorageError err = this->ensure_initialized();
  if (err != storage::StorageError::STORAGE_ERROR_OK)
    return err;
  uint64_t off = 0;
  uint16_t vlen = 0;
  if (!this->find_in_(this->active_off_, key, &off, &vlen, nullptr))
    return storage::StorageError::STORAGE_ERROR_NOT_FOUND;
  if (vlen > len)
    return storage::StorageError::STORAGE_ERROR_INVALID_ARGS;
  if (vlen > 0 && !this->read_(off + ENTRY_HDR, buf, vlen))
    return storage::StorageError::STORAGE_ERROR_READ_ERROR;
  *got = vlen;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError InplaceKVStore::get_size(uint32_t key, size_t *out) {
  *out = 0;
  storage::StorageError err = this->ensure_initialized();
  if (err != storage::StorageError::STORAGE_ERROR_OK)
    return err;
  uint16_t vlen = 0;
  if (!this->find_in_(this->active_off_, key, nullptr, &vlen, nullptr))
    return storage::StorageError::STORAGE_ERROR_NOT_FOUND;
  *out = vlen;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError InplaceKVStore::list_keys(bool (*callback)(uint32_t key, size_t size, void *ctx), void *ctx) {
  storage::StorageError err = this->ensure_initialized();
  if (err != storage::StorageError::STORAGE_ERROR_OK)
    return err;
  const uint64_t half_off = this->active_off_;
  const uint64_t half_end = half_off + this->half_size_();
  uint64_t pos = half_off + HALF_HDR;
  while (pos + ENTRY_HDR <= half_end) {
    uint8_t eh[ENTRY_HDR];
    if (!this->read_(pos, eh, ENTRY_HDR))
      break;
    if (eh[0] != COMMITTED)
      break;  // first uncommitted slot marks the end of the log
    uint8_t live = eh[1];
    uint32_t ekey = rd_u32(eh + 2);
    uint16_t elen = rd_u16(eh + 6);
    uint64_t total = ENTRY_HDR + elen;
    if (pos + total > half_end)
      break;
    if (live == 1) {
      // A key rewritten in place appears multiple times in the log; report it once, at the entry
      // find_in_ resolves as current (last-wins), so the length reported is the live one.
      uint64_t last_off = 0;
      if (this->find_in_(half_off, ekey, &last_off, nullptr, nullptr) && last_off == pos) {
        if (!callback(ekey, elen, ctx))
          return storage::StorageError::STORAGE_ERROR_OK;  // callback asked to stop
      }
    }
    pos += total;
  }
  return storage::StorageError::STORAGE_ERROR_OK;
}

bool InplaceKVStore::has(uint32_t key) {
  if (this->ensure_initialized() != storage::StorageError::STORAGE_ERROR_OK)
    return false;
  return this->find_in_(this->active_off_, key, nullptr, nullptr, nullptr);
}

storage::StorageError InplaceKVStore::erase(uint32_t key) {
  storage::StorageError err = this->ensure_initialized();
  if (err != storage::StorageError::STORAGE_ERROR_OK)
    return err;
  uint64_t off = 0;
  if (!this->find_in_(this->active_off_, key, &off, nullptr, nullptr))
    return storage::StorageError::STORAGE_ERROR_OK;  // idempotent
  uint8_t dead = 0;
  if (!this->write_(off + 1, &dead, 1))
    return storage::StorageError::STORAGE_ERROR_WRITE_ERROR;
  return storage::StorageError::STORAGE_ERROR_OK;
}

storage::StorageError InplaceKVStore::get_info(storage::StorageInfo *info) {
  info->id = this->storage_id_;
  info->name = this->storage_name_ != nullptr ? this->storage_name_ : "inplace_kv";
  info->kind = "kv";
  info->total_bytes = this->half_size_();  // usable capacity is one half
  info->free_bytes = 0;
  info->block_size = 0;
  info->is_mounted = this->initialized_;
  info->is_removable = false;
  info->is_read_only = false;
  return storage::StorageError::STORAGE_ERROR_OK;
}

void InplaceKVStore::setup() {
  if (storage::global_storage_registry != nullptr) {
    if (storage::global_storage_registry->register_storage(this) != storage::StorageError::STORAGE_ERROR_OK) {
      ESP_LOGE(TAG, "Storage registration failed");
      this->mark_failed();
      return;
    }
  }
  // Best-effort: the backing bus device may not be ready yet at setup time. If so, initialization
  // is retried lazily on the first access (ensure_initialized() runs before every operation).
  this->ensure_initialized();
}

void InplaceKVStore::dump_config() {
  ESP_LOGCONFIG(TAG, "In-place key-value store:");
  ESP_LOGCONFIG(TAG, "  Window size: %llu bytes (usable %llu)", (unsigned long long) this->size_,
                (unsigned long long) this->half_size_());
}

}  // namespace binary_storage
}  // namespace esphome

#endif  // USE_BINARY_STORAGE_INPLACE_KV
