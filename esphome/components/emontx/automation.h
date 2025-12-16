#pragma once

#include "esphome/core/automation.h"
#include "emontx.h"

namespace esphome {
namespace emontx {

class EmonTxJsonTrigger : public Trigger<JsonObject, std::string> {
 public:
  explicit EmonTxJsonTrigger(EmonTx *parent) {
    parent->add_on_json_callback([this, parent](JsonObject json) {
      // Trigger with both the JSON object and the raw string buffer
      this->trigger(json, parent->get_buffer());
    });
  }
};

class EmonTxDataTrigger : public Trigger<std::string> {
 public:
  explicit EmonTxDataTrigger(EmonTx *parent) {
    parent->add_on_data_callback([this](const std::string &data) {
      // Trigger with the received data (plain text or JSON)
      this->trigger(data);
    });
  }
};

}  // namespace emontx
}  // namespace esphome
