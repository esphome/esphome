#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <cstring>

namespace esphome::mk2pvrouter {
/*
 * Buffer sizes based on mk2pvrouter telemetry protocol (from teleinfo.h):
 * - Tags: max 4 chars (S_MC is longest), most are 1-2 chars (P, V1, R2, etc.)
 * - Values: max 6 digits signed (-10000), typical 1-5 digits
 * - Frame: STX + multiple lines (LF+tag+TAB+value+TAB+crc+CR) + ETX
 * - Line format: \n<tag>\t<value>\t<crc>\r (8-15 bytes per line)
 * - Multi-phase with all features: ~150-200 bytes
 */
static const uint8_t MAX_TAG_SIZE = 8;     // S_MC (4) + digit (1) + null (1) + margin (2)
static const uint8_t MAX_VAL_SIZE = 8;     // -10000 (6) + null (1) + margin (1)
static const uint16_t MAX_BUF_SIZE = 256;  // Full frame with all features enabled

/**
 * @class Mk2PVRouterListener
 * @brief Listener interface for receiving updates from the Mk2PVRouter.
 *
 * This class allows other components to register as listeners to receive updates
 * for specific tags published by the Mk2PVRouter.
 */
class Mk2PVRouterListener {
 public:
  void set_tag(const char *tag) { this->tag_ = tag; }
  const char *get_tag() const { return this->tag_; }
  virtual void publish_val(const char *val) = 0;

 protected:
  const char *tag_{nullptr};
};

/**
 * @class Mk2PVRouter
 * @brief Main class for the Mk2PVRouter component.
 *
 * The Mk2PVRouter processes incoming data frames via UART, validates their CRC,
 * extracts tags and values, and publishes them to registered listeners.
 */
class Mk2PVRouter : public PollingComponent, public uart::UARTDevice {
 public:
  Mk2PVRouter() = default;
  void register_mk2pvrouter_listener(Mk2PVRouterListener *listener);
  void loop() override;
  void setup() override;
  void update() override;
  void dump_config() override;

 protected:
  static constexpr size_t CRC_SUFFIX_LEN = 1;
  static constexpr uint32_t BAUD_RATE = 9600;

  StaticVector<Mk2PVRouterListener *, MK2PVROUTER_MAX_LISTENERS> mk2pvrouter_listeners_{};
  char buf_[MAX_BUF_SIZE];
  size_t buf_index_{0};
  char tag_[MAX_TAG_SIZE];
  char val_[MAX_VAL_SIZE];

  enum class State {
    OFF,
    ON,
    START_FRAME_RECEIVED,
    END_FRAME_RECEIVED,
  };

  State state_{State::OFF};

  bool read_chars_until_(bool drop, uint8_t c);
  uint8_t calculate_crc_(const char *grp, size_t grp_len);
  bool check_crc_(const char *grp, const char *grp_end);
  void publish_value_(const char *tag, const char *val);
};
}  // namespace esphome::mk2pvrouter
