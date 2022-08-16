#pragma once

#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "q3da_parser.h"

namespace esphome {
namespace q3da {

class Q3DAListener {
 public:
  std::string server_id;
  std::string obis_code;
  Q3DAListener(std::string server_id, std::string obis_code);
  virtual void publish_val(const ObisInfo &obis_info){};
};

class q3da : public Component, public uart::UARTDevice {
 public:
  void register_q3da_listener(Q3DAListener *listener);
  void loop() override;
  void dump_config() override;
  std::vector<Q3DAListener *> q3da_listeners_{};

 protected:
  void process_q3da_telegram_(const bytes &q3da_data);
  void log_obis_info_(const std::vector<ObisInfo> &obis_info_vec);
  void publish_obis_info_(const std::vector<ObisInfo> &obis_info_vec);
  char check_start_end_bytes_(uint8_t byte);
  void publish_value_(const ObisInfo &obis_info);

  // Serial parser
  bool record_ = false;
  bytes q3da_data_;
};

bool check_q3da_data(const bytes &buffer);
}  // namespace q3da
}  // namespace esphome
