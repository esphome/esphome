#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"
#include "sml_parser.h"

namespace esphome {
namespace sml {

class SmlListener {
 public:
  std::string server_id;
  std::string obis_code;
  SmlListener(std::string server_id, std::string obis_code);
  virtual void publish_val(const ObisInfo &obis_info){};
};

class Sml : public Component, public uart::UARTDevice {
 public:
  void register_sml_listener(SmlListener *listener);
  void loop() override;
  void dump_config() override;
  std::vector<SmlListener *> sml_listeners_{};
  void add_on_data_callback(std::function<void(std::vector<uint8_t>, bool)> &&callback);

 protected:
  void process_sml_file_(const BytesView &sml_data);
  void log_obis_info_(const std::vector<ObisInfo> &obis_info_vec);
  void publish_obis_info_(const std::vector<ObisInfo> &obis_info_vec);
  char check_start_end_bytes_(uint8_t byte);
  void publish_value_(const ObisInfo &obis_info);

  // Serial parser
  bool record_ = false;
  uint16_t incoming_mask_ = 0;
  bytes sml_data_;

  CallbackManager<void(const std::vector<uint8_t> &, bool)> data_callbacks_{};
};

bool check_sml_data(const bytes &buffer);

uint8_t get_code(uint8_t byte);
}  // namespace sml
}  // namespace esphome
