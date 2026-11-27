#include "scan_response_merger.h"

#ifdef USE_BLE_SCAN_RESPONSE_MERGER

#include "esphome/core/helpers.h"

#include <cstring>

namespace esphome::ble_device_base {

void ScanResponseMerger::deliver_(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data,
                                  uint8_t data_len, bool raw_only) {
  // A partial bind is treated as unbound; never dereference half a binding.
  if (this->dispatcher_ == nullptr || this->scan_continuous_ == nullptr)
    return;
  this->dispatcher_->dispatch(mac, rssi, addr_type, data, data_len, raw_only,
                              *this->scan_continuous_ ? nullptr : this->log_tag_);
}

void ScanResponseMerger::stash_adv(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data,
                                   uint8_t data_len, uint32_t now) {
  // One pass: find a same-device entry (deliver + reuse) while remembering the
  // first free slot as the fallback.
  PendingAdv *slot = nullptr;
  PendingAdv *free_slot = nullptr;
  for (auto &p : this->pending_adv_) {
    if (!p.used) {
      if (free_slot == nullptr)
        free_slot = &p;
      continue;
    }
    if (p.addr_type == addr_type && memcmp(p.mac, mac, MAC_ADDRESS_SIZE) == 0) {
      // Same device advertised again before its scan response arrived — deliver
      // the previous advertisement (its scan response is not coming) and reuse
      // the slot, so no frame is ever lost.
      p.used = false;
      this->pending_count_--;
      this->deliver_(p.mac, p.rssi, p.addr_type, p.data, p.data_len, /*raw_only=*/false);
      slot = &p;
      break;
    }
  }
  if (slot == nullptr)
    slot = free_slot;
  if (slot == nullptr) {
    // Table full — degrade gracefully: deliver the advertisement unmerged.
    this->deliver_(mac, rssi, addr_type, data, data_len, /*raw_only=*/false);
    return;
  }
  slot->used = true;
  this->pending_count_++;
  memcpy(slot->mac, mac, MAC_ADDRESS_SIZE);
  slot->addr_type = addr_type;
  slot->rssi = rssi;
  slot->data_len = (data_len <= sizeof(slot->data)) ? data_len : sizeof(slot->data);
  memcpy(slot->data, data, slot->data_len);
  slot->stored_ms = now;
}

void ScanResponseMerger::submit_scan_rsp(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data,
                                         uint8_t data_len) {
  // Fast-out on the empty table (sweep/flush use the same guard); this is the
  // hottest caller.
  if (this->pending_count_ != 0) {
    for (auto &p : this->pending_adv_) {
      if (p.used && p.addr_type == addr_type && memcmp(p.mac, mac, MAC_ADDRESS_SIZE) == 0) {
        // Append in place: the slot is released on delivery, so its 62-byte
        // buffer (legacy adv + scan response) holds the merged frame directly.
        const uint8_t room = sizeof(p.data) - p.data_len;
        const uint8_t add = (data_len <= room) ? data_len : room;
        memcpy(p.data + p.data_len, data, add);
        p.used = false;
        this->pending_count_--;
        // The advertisement's RSSI, not the scan response's (header contract).
        this->deliver_(mac, p.rssi, addr_type, p.data, p.data_len + add, /*raw_only=*/false);
        return;
      }
    }
  }
  // Unmatched scan-response: goes out on the raw callback only (HA merges per
  // address); local listeners/triggers receive each advertisement exactly once
  // via the merged/plain path above.
  this->deliver_(mac, rssi, addr_type, data, data_len, /*raw_only=*/true);
}

void ScanResponseMerger::sweep(uint32_t now) {
  if (this->pending_count_ == 0)
    return;
  for (auto &p : this->pending_adv_) {
    if (p.used && now - p.stored_ms > PENDING_ADV_TIMEOUT_MS) {
      p.used = false;
      this->pending_count_--;
      this->deliver_(p.mac, p.rssi, p.addr_type, p.data, p.data_len, /*raw_only=*/false);
    }
  }
}

void ScanResponseMerger::flush() {
  if (this->pending_count_ == 0)
    return;
  for (auto &p : this->pending_adv_) {
    if (p.used) {
      p.used = false;
      this->pending_count_--;
      this->deliver_(p.mac, p.rssi, p.addr_type, p.data, p.data_len, /*raw_only=*/false);
    }
  }
}

void AdvDispatcher::dispatch(const uint8_t *mac, int8_t rssi, uint8_t addr_type, const uint8_t *data, uint8_t data_len,
                             bool raw_only, const char *log_unclaimed_tag) {
  // Raw callback (the raw-advertisement path). Both full advertisements and
  // unmatched scan responses (raw_only) are forwarded.
  if (this->raw_callback_.is_set()) {
    const RawAdvertisement adv{.address = mac_lsb_first_to_uint64(mac),
                               .data = data,
                               .data_len = data_len,
                               .rssi = rssi,
                               .addr_type = addr_type};
    this->raw_callback_.invoke(adv);
  }

#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  // Scan-response-only frames are never parsed for local sensors/triggers.
  if (raw_only)
    return;
  ESPBTDevice device;
  device.from_scan_result(mac, rssi, addr_type, data, data_len);
  // The listener list holds sensors AND the tracker's automation triggers
  // (the triggers are listeners, exactly like esp32_ble_tracker), so one
  // loop feeds both and ORs into `found`.
  bool found = false;
  for (auto *listener : this->listeners_) {
    if (listener->parse_device(device)) {
      found = true;
    }
  }
  if (!found && log_unclaimed_tag != nullptr)
    this->discovered_log_.log_device(log_unclaimed_tag, device);
#endif  // ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
}

void AdvDispatcher::on_scan_end() {
#ifdef ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT
  for (auto *listener : this->listeners_)
    listener->on_scan_end();
  this->discovered_log_.clear();  // reset per-scan "Found device" dedup (esp32_ble_tracker parity)
#endif
}

}  // namespace esphome::ble_device_base

#endif  // USE_BLE_SCAN_RESPONSE_MERGER
