#pragma once

#if defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3) || \
    defined(USE_ESP32_VARIANT_ESP32S31) || defined(USE_ESP32_VARIANT_ESP32H4)

#include "esphome/components/usb_host/usb_host.h"
#include "esphome/core/component.h"

#include <atomic>
#include <cstdint>
#include <vector>

#ifdef USE_USB_BLUETOOTH_BLUEDROID
struct esp_bluedroid_hci_driver_callbacks;
#endif

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

  void loop() override;
  void dump_config() override;

  /// A Realtek patch for one ROM version of the chip with this lmp_subver, prepared by realtek.py.
  void add_realtek_firmware(uint16_t lmp_subver, uint8_t rom_version, const uint8_t *data, size_t length) {
    this->realtek_images_.push_back({data, length, lmp_subver, rom_version});
  }

#ifdef USE_USB_BLUETOOTH_BLUEDROID
  /// This dongle is the controller of Bluedroid (esp32_ble)
  void set_bluedroid();
  // Bluedroid's host-only transport (esp_bluedroid_hci.h); its tasks call these
  void hci_send(const uint8_t *h4, uint16_t len);
  bool hci_send_available() const;
  void hci_attach(const ::esp_bluedroid_hci_driver_callbacks *callbacks);
#endif

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

  struct RealtekImage {
    const uint8_t *data;
    size_t length;
    uint16_t lmp_subver;
    uint8_t rom_version;
  };

  // An event is code, parameter length and up to 255 parameter bytes.
  static constexpr size_t EVENT_MAX = 2 + 255;
  // An ACL packet is handle, length and up to 1021 bytes (the largest BR/EDR packet, 3-DH5)
  static constexpr size_t ACL_MAX = 4 + 1021;

  void on_connected() override;
  void on_disconnected() override;
  bool reports_connection_itself() const override { return true; }

  bool find_interface_();
  // USB task.
  void read_event_(uint32_t generation);
#ifdef USE_USB_BLUETOOTH_BLUEDROID
  void read_acl_(uint32_t generation);
  // USB task: a packet for Bluedroid, its H4 type in byte 0
  void to_host_(uint8_t *h4, size_t len);
  // USB task: a command or ACL transfer to the dongle finished
  void send_done_();
  // Bluedroid's HCI task: answer a command while there is no dongle
  void answer_without_controller_(uint16_t opcode);
#endif
  // Main loop.
  void send_command_(uint16_t opcode, const uint8_t *params, size_t len);
  void handle_command_complete_(const uint8_t *ret, size_t len);
  void next_fragment_();
  void finish_probe_(bool ok);

  std::vector<RealtekImage> realtek_images_{};
  const RealtekImage *download_{nullptr};

  // Bumped on connect and disconnect, so completions of an earlier device are dropped.
  std::atomic<uint32_t> generation_{0};
  // The USB task stores the Command Complete of the pending command here and sets the flag;
  // the main loop handles it and clears the flag.
  std::atomic<bool> complete_ready_{false};
  uint8_t complete_[EVENT_MAX]{};
  size_t complete_len_{0};
  // Event reassembly, USB task only. Byte 0 is kept free for the H4 packet type.
  uint8_t event_h4_[1 + EVENT_MAX]{};
  uint8_t *const event_{event_h4_ + 1};
  size_t event_len_{0};
#ifdef USE_USB_BLUETOOTH_BLUEDROID
  // ACL reassembly, USB task only; byte 0 holds the H4 packet type
  uint8_t acl_h4_[1 + ACL_MAX]{};
  size_t acl_len_{0};
  const ::esp_bluedroid_hci_driver_callbacks *host_{nullptr};
  // The command Bluedroid is sending, reserved once so a command allocates nothing
  std::vector<uint8_t> hci_command_{};
  // Bluedroid owns the HCI traffic: events go to it rather than to the probe
  std::atomic<bool> host_attached_{false};
  // A send was refused for want of transfer slots; Bluedroid is told once one is free
  mutable std::atomic<bool> send_blocked_{false};
  // Bluedroid was running on this dongle when it went away: bring esp32_ble back once it is ready again
  bool restart_host_{false};
#endif

  uint32_t fragment_{0};
  uint32_t fragment_count_{0};
  std::atomic<uint16_t> pending_opcode_{0};
  uint16_t evt_mps_{0};
  uint16_t acl_in_mps_{0};
  uint16_t acl_out_mps_{0};
  uint16_t manufacturer_{0};
  uint16_t lmp_subver_{0};
  uint16_t hci_revision_{0};
  uint8_t address_[6]{};
  uint8_t hci_version_{0};
  uint8_t intf_{0xFF};
  uint8_t evt_ep_{0};
  uint8_t acl_in_ep_{0};
  uint8_t acl_out_ep_{0};
  ProbeStep step_{ProbeStep::PROBE_STEP_IDLE};
};

#ifdef USE_USB_BLUETOOTH_BLUEDROID
/// For esp32_ble: hand Bluedroid the dongle's transport; false when there is no dongle to hand over.
bool attach_bluedroid();
/// For esp32_ble: take the transport back once Bluedroid is down.
void detach_bluedroid();
/// For esp32_ble: whether the dongle is plugged in and set up, so Bluedroid can start on it.
bool bluedroid_controller_ready();
#endif

}  // namespace esphome::usb_bluetooth

#endif
