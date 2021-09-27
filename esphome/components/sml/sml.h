#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "sml_parser.h"

namespace esphome {
namespace sml {

class SmlListener {
 public:
  std::string server_id;
  std::string obis;
  SmlListener(std::string server_id, std::string obis);
  virtual void publish_val(const ObisInfo &obis_info){};
};

class Sml : public Component, public uart::UARTDevice {
 public:
  void register_sml_listener(SmlListener *listener);
  void loop() override;
  void dump_config() override;
  std::vector<SmlListener *> sml_listeners_{};

 protected:
  void process_sml_file_(const bytes &sml_data);
  void log_obis_info_(const std::vector<ObisInfo> &obis_info_vec);
  void publish_obis_info_(const std::vector<ObisInfo> &obis_info_vec);
  char check_start_end_bytes_(char c);
  void publish_value_(const ObisInfo &obis_info);

  // Serial parser
  bool record_ = false;
  char incoming_buffer_[8]{0};
  bytes sml_data_;
};
}  // namespace sml
}  // namespace esphome
