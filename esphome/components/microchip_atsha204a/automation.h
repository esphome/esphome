#pragma once

#include "esphome/core/automation.h"
#include "microchip_atsha204a.h"

#include <utility>

namespace esphome::microchip_atsha204a {

template<typename... Ts> class CommandAction final : public Action<Ts...> {
 public:
  CommandAction(ATSHA204A *parent, Opcode opcode, EndMode end_mode, bool expect_response)
      : parent_(parent), opcode_(opcode), end_mode_(end_mode), expect_response_(expect_response) {}

  TEMPLATABLE_VALUE(uint8_t, param1)
  TEMPLATABLE_VALUE(uint16_t, param2)
  TEMPLATABLE_BYTES(data)
  TEMPLATABLE_VALUE(uint32_t, session)

  template<typename F> void set_result_callback(F &&callback) {
    this->result_callback_ = Callback<void(int, CommandResponse, uint32_t)>::create(std::forward<F>(callback));
  }

  void play(const Ts &...x) override {
    this->data_.template visit<MAX_COMMAND>(
        [&](const uint8_t *data, size_t size) {
          this->parent_->command(this->opcode_, this->param1_.value(x...), this->param2_.value(x...), {data, size},
                                 this->end_mode_, this->session_.value(x...), this->expect_response_,
                                 [this](int status, const CommandResponse &response, uint32_t session) {
                                   if (this->result_callback_.fn_ != nullptr)
                                     this->result_callback_.call(status, response, session);
                                 });
        },
        x...);
  }

 protected:
  ATSHA204A *parent_;
  Callback<void(int, CommandResponse, uint32_t)> result_callback_;
  Opcode opcode_;
  EndMode end_mode_;
  bool expect_response_;
};

}  // namespace esphome::microchip_atsha204a
