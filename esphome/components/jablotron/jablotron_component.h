#pragma once
#include <deque>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "jablotron_device.h"
#include "response_awaiter.h"
#include "response_handler.h"
#include "uart_line_device.h"
#include "string_view.h"

namespace esphome {
namespace jablotron {

class JablotronComponent : public UARTLineDevice, public PollingComponent {
 public:
  JablotronComponent();

  void setup() override;
  void loop() override;
  void update() override;

  void queue_request(std::string request);
  void queue_request_access_code(std::string request, const std::string &access_code);

  void register_info(InfoDevice *device);
  void register_peripheral(PeripheralDevice *device);
  void register_pg(PGDevice *device);
  void register_section(SectionDevice *device);
  void register_section_flag(SectionFlagDevice *device);

  void set_access_code(std::string access_code);

 private:
  ResponseHandler *handle_response_(StringView response);

  void queue_peripheral_request_();
  void queue_pg_request_();
  void queue_section_request_();
  void queue_section_flag_request_();

  void send_queued_request_();
  void send_request_(const std::string &request);

  template<typename T> std::string get_index_string(const std::vector<T *> &items) {
    std::set<int32_t> indices;
    std::transform(std::begin(items), std::end(items), std::inserter(indices, indices.begin()),
                   [](const T *item) { return item->get_index(); });
    std::stringstream stream;
    for (int32_t index : indices) {
      stream << ' ' << index;
    }
    return stream.str();
  }

  std::string access_code_;

  InfoDeviceVector infos_;
  PeripheralDeviceVector peripherals_;
  PGDeviceVector pgs_;
  SectionDeviceVector sections_;
  SectionFlagDeviceVector section_flags_;
  std::deque<std::string> request_queue_;

  ResponseAwaiter response_awaiter_;
  bool pending_update_ = false;

  ResponseHandlerError error_handler_;
  ResponseHandlerOK ok_handler_;
  ResponseHandlerPGState pgstate_handler_;
  ResponseHandlerPrfState prfstate_handler_;
  ResponseHandlerState state_handler_;
  ResponseHandlerVer ver_handler_;
  ResponseHandlerSectionFlag section_flag_handler_;
};

}  // namespace jablotron
}  // namespace esphome
