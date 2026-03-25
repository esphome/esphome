#pragma once

#include "esphome/core/automation.h"
#include "emontx.h"

namespace esphome::emontx {

class EmonTxJsonTrigger : public Trigger<JsonObject, std::string> {
 public:
  explicit EmonTxJsonTrigger(EmonTx *parent) {
    parent->add_on_json_callback(
        [this](JsonObject json, StringRef raw_json) -> void { this->trigger(json, raw_json.str()); });
  }
};

class EmonTxDataTrigger : public Trigger<std::string> {
 public:
  explicit EmonTxDataTrigger(EmonTx *parent) {
    parent->add_on_data_callback([this](StringRef data) { this->trigger(data.str()); });
  }
};

}  // namespace esphome::emontx
