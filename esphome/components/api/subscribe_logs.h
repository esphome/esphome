#pragma once

#include "esphome/core/component.h"
#include "api_message.h"

namespace esphome {
namespace api {

class SubscribeLogsRequest : public APIMessage {
 public:
  bool decode_varint(uint32_t field_id, uint32_t value) override;
  APIMessageType message_type() const override;
  uint32_t get_level() const;
  void set_level(uint32_t level);
  bool get_dump_config() const;
  void set_dump_config(bool dump_config);

 protected:
  uint32_t level_{6};
  bool dump_config_{false};
};

}  // namespace api
}  // namespace esphome
