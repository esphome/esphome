#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "esphome/components/usb_host/usb_host.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

namespace esphome::usb_bluetooth {

/// A USB Bluetooth dongle (Bluetooth Core Specification, USB transport) as a usb_host client.
///
/// HCI commands are class requests on endpoint 0, events arrive on the interrupt IN endpoint of
/// the controller interface (class E0, subclass 01, protocol 01) and ACL data on its bulk pair.
/// On connect the controller is reset, a Realtek controller still on its ROM gets the configured
/// firmware, and its version, address and name are read and logged.
class USBBluetooth final : public usb_host::USBClient {
 public:
  using usb_host::USBClient::USBClient;

  /// One Realtek patch: the bytes downloaded into a controller with this lmp_subver and ROM version.
  struct RealtekImage {
    const uint8_t *data;
    size_t length;
    uint16_t lmp_subver;
    uint8_t rom_version;
  };

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// Storage for the Realtek patches, sized by code generation.
  void set_realtek_firmware_storage(std::span<RealtekImage> storage) { this->realtek_images_.init(storage); }
  /// A Realtek patch for one ROM version of the chip with this lmp_subver, prepared by realtek.py.
  void add_realtek_firmware(uint16_t lmp_subver, uint8_t rom_version, const uint8_t *data, size_t length) {
    this->realtek_images_.push_back({data, length, lmp_subver, rom_version});
  }

  /// Whether a controller is connected and answered the start-up commands.
  bool is_ready() const { return this->step_ == ProbeStep::PROBE_STEP_READY; }
  /// The controller's public address, most significant byte first; all zero until it is ready.
  const uint8_t *address() const { return this->address_; }

 protected:
  enum class ProbeStep : uint8_t {
    PROBE_STEP_IDLE,
    PROBE_STEP_RESET,
    PROBE_STEP_READ_VERSION,
    PROBE_STEP_RTL_READ_ROM_VERSION,
    PROBE_STEP_RTL_DOWNLOAD,
    PROBE_STEP_READ_VERSION_PATCHED,
    PROBE_STEP_READ_ADDRESS,
    PROBE_STEP_READ_NAME,
    PROBE_STEP_READY,
    PROBE_STEP_FAILED,
  };

  // An event is code, parameter length and up to 255 parameter bytes.
  static constexpr size_t EVENT_MAX = 2 + 255;

  void on_connected() override;
  void on_disconnected() override;
  bool reports_connection_itself() const override { return true; }

  enum class FindResult : uint8_t { FIND_RESULT_FOUND, FIND_RESULT_NONE, FIND_RESULT_ERROR };
  FindResult find_interface_();
  void decline_();
  // Main loop: open the next device that is plugged in and not yet looked at, after one was closed
  // without being claimed.
  void try_next_device_();
  // USB task (main loop for the first read). False when no transfer slot was free.
  bool read_event_(uint32_t generation);
  // Main loop.
  void send_command_(uint16_t opcode, const uint8_t *params, size_t len);
  void handle_command_complete_(const uint8_t *ret, size_t len);
  void next_fragment_();
  void finish_probe_(bool ok);

  StaticFixedVector<RealtekImage> realtek_images_;
  // The HCI command being sent, reserved in setup() so sending allocates nothing
  std::vector<uint8_t> command_{};
  // Device addresses (0..127) already looked at and not to be opened again while they stay plugged in
  uint32_t declined_[4]{};
  const RealtekImage *download_{nullptr};

  // Bumped on connect and disconnect, so completions of an earlier device are dropped.
  std::atomic<uint32_t> generation_{0};
  // The USB task stores the Command Complete of the pending command here and sets the flag;
  // the main loop handles it and clears the flag.
  std::atomic<bool> complete_ready_{false};
  // Set by the USB task when the event read stopped and could not be queued again.
  std::atomic<bool> events_stopped_{false};
  uint8_t complete_[EVENT_MAX]{};
  size_t complete_len_{0};
  // Event reassembly, USB task only. Byte 0 is kept free for the H4 packet type.
  uint8_t event_h4_[1 + EVENT_MAX]{};
  uint8_t *const event_{event_h4_ + 1};
  size_t event_len_{0};

  uint32_t fragment_{0};
  uint32_t fragment_count_{0};
  std::atomic<uint16_t> pending_opcode_{0};
  uint16_t evt_mps_{0};
  uint16_t acl_in_mps_{0};
  uint16_t acl_out_mps_{0};
  uint16_t manufacturer_{0};
  uint16_t lmp_subver_{0};
  // The lmp_subver reported before the firmware download, to check that the download took
  uint16_t rom_lmp_subver_{0};
  uint16_t hci_revision_{0};
  uint8_t address_[6]{};
  uint8_t hci_version_{0};
  uint8_t intf_{0xFF};
  uint8_t evt_ep_{0};
  uint8_t acl_in_ep_{0};
  uint8_t acl_out_ep_{0};
  ProbeStep step_{ProbeStep::PROBE_STEP_IDLE};
};

}  // namespace esphome::usb_bluetooth

#endif
