#pragma once

#include "esphome/components/nfc/automation.h"
#include "esphome/components/nfc/nci_core.h"
#include "esphome/components/nfc/nci_message.h"
#include "esphome/components/nfc/nfc.h"
#include "esphome/components/nfc/nfc_helpers.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"

#include <functional>
#include <span>

namespace esphome::pn71xx {

// Time to wait for the NFCC to answer. NXP's reference stack waits 1 s for a response; 10 ms was short enough
// that a slow RF_DEACTIVATE_RSP, for example when a tag leaves the field, caused a full NFCC reset.
static constexpr uint16_t NFCC_DEFAULT_TIMEOUT = 100;
static constexpr uint16_t NFCC_INIT_TIMEOUT = 50;
static constexpr uint16_t NFCC_TAG_WRITE_TIMEOUT = 100;
// Time to wait for IRQ to drop after a read; it drops within microseconds unless another message is queued
static constexpr uint16_t NFCC_IRQ_CLEAR_TIMEOUT = 5;
// Length of the VEN and DWL_REQ pulses when resetting the NFCC
static constexpr uint16_t NFCC_RESET_DELAY = 10;
// Time to wait before resending a frame the NFCC refused, e.g. while waking from standby
static constexpr uint16_t NFCC_WRITE_RETRY_DELAY = 5;
// Longest time the FSM may wait for a notification that ends a transitional state before resetting the NFCC
static constexpr uint32_t NFCC_STATE_TIMEOUT = 1000;

static constexpr uint8_t NFCC_MAX_COMM_FAILS = 3;
static constexpr uint8_t NFCC_MAX_ERROR_COUNT = 10;

static constexpr uint8_t XCHG_DATA_OID = 0x10;
static constexpr uint8_t MF_SECTORSEL_OID = 0x32;
static constexpr uint8_t MFC_AUTHENTICATE_OID = 0x40;
static constexpr uint8_t TEST_PRBS_OID = 0x30;
static constexpr uint8_t TEST_ANTENNA_OID = 0x3D;
static constexpr uint8_t TEST_GET_REGISTER_OID = 0x33;

static constexpr uint8_t MFC_AUTHENTICATE_PARAM_KS_A = 0x00;  // key select A
static constexpr uint8_t MFC_AUTHENTICATE_PARAM_KS_B = 0x80;  // key select B
static constexpr uint8_t MFC_AUTHENTICATE_PARAM_EMBED_KEY = 0x10;

static constexpr uint8_t CARD_EMU_T4T_APP_SELECT[] = {0x00, 0xA4, 0x04, 0x00, 0x07, 0xD2, 0x76,
                                                      0x00, 0x00, 0x85, 0x01, 0x01, 0x00};
// MLe is 0xFD so a full-length READ BINARY response plus the two status bytes fits in one NCI data packet
static constexpr uint8_t CARD_EMU_T4T_CC[] = {0x00, 0x0F, 0x20, 0x00, 0xFD, 0x00, 0xFF, 0x04,
                                              0x06, 0xE1, 0x04, 0x00, 0xFF, 0x00, 0x00};
// Largest NDEF message that fits in the emulated NDEF file (max file size in the CC, less the 2-byte length)
static constexpr uint16_t CARD_EMU_T4T_MAX_NDEF_SIZE = 0xFF - 2;
static constexpr uint8_t CARD_EMU_T4T_CC_SELECT[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x03};
static constexpr uint8_t CARD_EMU_T4T_NDEF_SELECT[] = {0x00, 0xA4, 0x00, 0x0C, 0x02, 0xE1, 0x04};
static constexpr uint8_t CARD_EMU_T4T_READ[] = {0x00, 0xB0};
static constexpr uint8_t CARD_EMU_T4T_WRITE[] = {0x00, 0xD6};
static constexpr uint8_t CARD_EMU_T4T_OK[] = {0x90, 0x00};
static constexpr uint8_t CARD_EMU_T4T_NOK[] = {0x6A, 0x82};

static constexpr uint8_t CORE_CONFIG_SOLO[] = {0x01,   // Number of parameter fields
                                               0x00,   // config param identifier (TOTAL_DURATION)
                                               0x02,   // length of value
                                               0x01,   // TOTAL_DURATION (low)...
                                               0x00};  // TOTAL_DURATION (high): 1 ms

static constexpr uint8_t CORE_CONFIG_RW_CE[] = {0x01,   // Number of parameter fields
                                                0x00,   // config param identifier (TOTAL_DURATION)
                                                0x02,   // length of value
                                                0xF8,   // TOTAL_DURATION (low)...
                                                0x02};  // TOTAL_DURATION (high): 760 ms

static constexpr uint8_t RF_DISCOVER_MAP_CONFIG[] = {
    // poll modes
    nfc::PROT_T1T,    nfc::RF_DISCOVER_MAP_MODE_POLL,
    nfc::INTF_FRAME,  // poll mode
    nfc::PROT_T2T,    nfc::RF_DISCOVER_MAP_MODE_POLL,
    nfc::INTF_FRAME,  // poll mode
    nfc::PROT_T3T,    nfc::RF_DISCOVER_MAP_MODE_POLL,
    nfc::INTF_FRAME,  // poll mode
    nfc::PROT_ISODEP, nfc::RF_DISCOVER_MAP_MODE_POLL | nfc::RF_DISCOVER_MAP_MODE_LISTEN,
    nfc::INTF_ISODEP,  // poll & listen mode
    nfc::PROT_MIFARE, nfc::RF_DISCOVER_MAP_MODE_POLL,
    nfc::INTF_TAGCMD};  // poll mode

static constexpr uint8_t RF_DISCOVERY_LISTEN_CONFIG[] = {
    nfc::MODE_LISTEN_MASK | nfc::TECH_PASSIVE_NFCA,   // listen mode
    nfc::MODE_LISTEN_MASK | nfc::TECH_PASSIVE_NFCB,   // listen mode
    nfc::MODE_LISTEN_MASK | nfc::TECH_PASSIVE_NFCF};  // listen mode

static constexpr uint8_t RF_DISCOVERY_POLL_CONFIG[] = {nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCA,   // poll mode
                                                       nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCB,   // poll mode
                                                       nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCF};  // poll mode

static constexpr uint8_t RF_DISCOVERY_CONFIG[] = {nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCA,          // poll mode
                                                  nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCB,          // poll mode
                                                  nfc::MODE_POLL | nfc::TECH_PASSIVE_NFCF,          // poll mode
                                                  nfc::MODE_LISTEN_MASK | nfc::TECH_PASSIVE_NFCA,   // listen mode
                                                  nfc::MODE_LISTEN_MASK | nfc::TECH_PASSIVE_NFCB,   // listen mode
                                                  nfc::MODE_LISTEN_MASK | nfc::TECH_PASSIVE_NFCF};  // listen mode

enum class CardEmulationState : uint8_t {
  CARD_EMU_IDLE,
  CARD_EMU_NDEF_APP_SELECTED,
  CARD_EMU_CC_SELECTED,
  CARD_EMU_NDEF_SELECTED,
  CARD_EMU_DESFIRE_PROD,
};

enum class NCIState : uint8_t {
  NONE = 0x00,
  NFCC_RESET,
  NFCC_INIT,
  NFCC_CONFIG,
  NFCC_SET_DISCOVER_MAP,
  NFCC_SET_LISTEN_MODE_ROUTING,
  RFST_IDLE,
  RFST_DISCOVERY,
  RFST_W4_ALL_DISCOVERIES,
  RFST_W4_HOST_SELECT,
  RFST_LISTEN_ACTIVE,
  RFST_LISTEN_SLEEP,
  RFST_POLL_ACTIVE,
  EP_DEACTIVATING,
  EP_SELECTING,
  TEST = 0xFE,
  FAILED = 0xFF,
};

enum class TestMode : uint8_t {
  TEST_NONE = 0x00,
  TEST_PRBS,
  TEST_ANTENNA,
  TEST_GET_REGISTER,
};

struct DiscoveredEndpoint {
  uint32_t last_seen;
  std::unique_ptr<nfc::NfcTag> tag;
  uint8_t id;
  uint8_t protocol;
  bool trig_called;
};

/// Common driver for the NXP PN71xx family of NCI NFC controllers. The chip classes (PN7150, PN7160) supply the parts
/// that differ between chips; the bus classes supply read_nfcc() and write_nfcc().
class PN71xx : public nfc::Nfcc, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_irq_pin(GPIOPin *irq_pin) { this->irq_pin_ = irq_pin; }
  void set_ven_pin(GPIOPin *ven_pin) { this->ven_pin_ = ven_pin; }

  void set_tag_ttl(uint32_t ttl) { this->tag_ttl_ = ttl; }
  void set_tag_emulation_message(const std::shared_ptr<nfc::NdefMessage> &message);
  void set_tag_emulation_message(const std::string &message, bool include_android_app_record = true);
  void set_tag_emulation_message(const char *message, bool include_android_app_record = true);
  void set_tag_emulation_off();
  void set_tag_emulation_on();
  bool tag_emulation_enabled() { return this->listening_enabled_; }

  void set_polling_off();
  void set_polling_on();
  bool polling_enabled() { return this->polling_enabled_; }

  void register_ontag_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontag_.push_back(trig); }
  void register_ontagremoved_trigger(nfc::NfcOnTagTrigger *trig) { this->triggers_ontagremoved_.push_back(trig); }

  template<typename F> void add_on_emulated_tag_scan_callback(F &&callback) {
    this->on_emulated_tag_scan_callback_.add(std::forward<F>(callback));
  }

  template<typename F> void add_on_finished_write_callback(F &&callback) {
    this->on_finished_write_callback_.add(std::forward<F>(callback));
  }

  bool is_writing() { return this->next_task_ != EP_READ; };

  void read_mode();
  void clean_mode();
  void format_mode();
  void write_mode();
  void set_tag_write_message(std::shared_ptr<nfc::NdefMessage> message);
  void set_tag_write_message(const std::string &message, bool include_android_app_record = true);

  uint8_t set_test_mode(TestMode test_mode, const std::vector<uint8_t> &data, std::vector<uint8_t> &result);

 protected:
  uint8_t reset_core_(bool reset_config, bool power);
  uint8_t init_core_();

  /// Chip hooks
  /// Called before the NFCC is reset, e.g. to make sure it will not start in firmware download mode
  virtual void prepare_reset() {}
  /// Validates what follows a successful CORE_RESET_RSP (in `rx`); may read more messages from the NFCC
  virtual uint8_t verify_reset(nfc::NciMessage &rx, bool reset_config) = 0;
  /// Logs chip information from a successful CORE_INIT_RSP
  virtual uint8_t process_init_response(nfc::NciMessage &rx) = 0;
  /// Parameters for the CORE_SET_CONFIG_CMD that configures the power management unit
  virtual std::span<const uint8_t> pmu_config() const = 0;
  /// Payload of the RF_SET_LISTEN_MODE_ROUTING_CMD
  virtual std::span<const uint8_t> listen_mode_routing_config() const = 0;

  uint8_t send_init_config_();
  uint8_t send_core_config_();
  uint8_t refresh_core_config_();

  uint8_t set_discover_map_();

  uint8_t set_listen_mode_routing_();

  uint8_t start_discovery_();
  uint8_t stop_discovery_();
  uint8_t deactivate_(uint8_t type, uint16_t timeout = NFCC_DEFAULT_TIMEOUT);

  void select_endpoint_();

  uint8_t read_endpoint_data_(uint8_t protocol, nfc::NfcTag &tag);
  uint8_t clean_endpoint_(uint8_t protocol);
  uint8_t format_endpoint_(uint8_t protocol);
  uint8_t write_endpoint_(uint8_t protocol, nfc::NfcTagUid &uid, std::shared_ptr<nfc::NdefMessage> &message);

  /// Reads the UID from the RF technology parameters of a discovery or activation notification
  bool parse_uid_(uint8_t mode_tech, std::span<const uint8_t> rf_tech_params, nfc::NfcTagUid &uid);
  std::unique_ptr<nfc::NfcTag> build_tag_(uint8_t protocol, const nfc::NfcTagUid &uid);
  /// Finds a cached endpoint by UID, or caches a new one; returns nullopt if the cache is full
  optional<size_t> find_or_add_tag_(uint8_t protocol, const nfc::NfcTagUid &uid);
  optional<size_t> find_tag_uid_(const nfc::NfcTagUid &uid);
  void purge_old_tags_();
  void erase_tag_(uint8_t tag_index);

  /// advance controller state as required
  void nci_fsm_transition_();
  /// set new controller state
  void nci_fsm_set_state_(NCIState new_state);
  /// setting controller to this state caused an error; returns true if too many errors/failures
  bool nci_fsm_set_error_state_(NCIState new_state);
  /// parse & process incoming messages from the NFCC
  void process_message_();
  void process_rf_intf_activated_oid_(nfc::NciMessage &rx);
  void process_rf_discover_oid_(nfc::NciMessage &rx);
  void process_rf_deactivate_oid_(nfc::NciMessage &rx);
  void process_data_message_(nfc::NciMessage &rx);

  void card_emu_t4t_get_response_(std::span<const uint8_t> response, std::vector<uint8_t> &ndef_response);
  bool card_emu_t4t_read_ndef_(uint16_t offset, uint8_t length, std::vector<uint8_t> &ndef_response);

  uint8_t transceive_(nfc::NciMessage &tx, nfc::NciMessage &rx, uint16_t timeout = NFCC_DEFAULT_TIMEOUT,
                      bool expect_notification = true);
  virtual uint8_t read_nfcc(nfc::NciMessage &rx, uint16_t timeout) = 0;
  virtual uint8_t write_nfcc(nfc::NciMessage &tx) = 0;

  uint8_t wait_for_irq_(uint16_t timeout = NFCC_DEFAULT_TIMEOUT, bool pin_state = true);

  uint8_t read_mifare_classic_tag_(nfc::NfcTag &tag);
  uint8_t read_mifare_classic_block_(uint8_t block_num, std::vector<uint8_t> &data);
  uint8_t write_mifare_classic_block_(uint8_t block_num, const uint8_t *data, size_t len);
  uint8_t auth_mifare_classic_block_(uint8_t block_num, uint8_t key_num, const uint8_t *key);
  uint8_t sect_to_auth_(uint8_t block_num);
  uint8_t format_mifare_classic_mifare_();
  uint8_t format_mifare_classic_ndef_();
  uint8_t write_mifare_classic_tag_(const std::shared_ptr<nfc::NdefMessage> &message);
  uint8_t halt_mifare_classic_tag_();

  uint8_t read_mifare_ultralight_tag_(nfc::NfcTag &tag);
  uint8_t read_mifare_ultralight_bytes_(uint8_t start_page, uint16_t num_bytes, std::vector<uint8_t> &data);
  bool is_mifare_ultralight_formatted_(const std::vector<uint8_t> &page_3_to_6);
  uint16_t read_mifare_ultralight_capacity_();
  uint8_t find_mifare_ultralight_ndef_(const std::vector<uint8_t> &page_3_to_6, uint8_t &message_length,
                                       uint8_t &message_start_index);
  uint8_t write_mifare_ultralight_page_(uint8_t page_num, const uint8_t *write_data, size_t len);
  uint8_t write_mifare_ultralight_tag_(nfc::NfcTagUid &uid, const std::shared_ptr<nfc::NdefMessage> &message);
  uint8_t clean_mifare_ultralight_();

  enum NfcTask : uint8_t {
    EP_READ = 0,
    EP_CLEAN,
    EP_FORMAT,
    EP_WRITE,
  };

  // members are ordered by alignment, widest first, to minimize padding
  CallbackManager<void()> on_emulated_tag_scan_callback_;
  CallbackManager<void()> on_finished_write_callback_;

  std::vector<DiscoveredEndpoint> discovered_endpoint_;
  std::vector<uint8_t> card_emulation_ndef_;  // encoded emulation message; empty when none is set
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontag_;
  std::vector<nfc::NfcOnTagTrigger *> triggers_ontagremoved_;
  std::shared_ptr<nfc::NdefMessage> next_task_message_to_write_;

  GPIOPin *irq_pin_{nullptr};
  GPIOPin *ven_pin_{nullptr};

  uint32_t last_nci_state_change_{0};
  uint32_t tag_ttl_{250};

  NfcTask next_task_{EP_READ};
  CardEmulationState ce_state_{CardEmulationState::CARD_EMU_IDLE};
  NCIState nci_state_{NCIState::NFCC_RESET};
  NCIState nci_state_error_{NCIState::NONE};
  uint8_t error_count_{0};
  uint8_t selecting_endpoint_{0};

  bool config_refresh_pending_{false};
  bool core_config_is_solo_{false};
  bool listening_enabled_{false};
  bool polling_enabled_{true};
};

}  // namespace esphome::pn71xx
