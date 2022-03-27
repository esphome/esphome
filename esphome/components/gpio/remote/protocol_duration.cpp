#include "protocol_duration.h"
#include "esphome/core/log.h"

namespace esphome {
using namespace remote;
namespace gpio {

static const char *const TAG = "remote.gpio.raw";

void RemoteProtocolCodecDuration::encode(RemoteSignalData *dst, const std::vector<arg_t> &args) {
  dst->set_carrier_frequency(args.back());
  dst->set_data(args, args.size() - 1);
}

void RemoteProtocolCodecDuration::dump(const std::vector<arg_t> &args) {
  std::string buf = "duration:";

  buf.reserve(9 + 7 * args.size());

  auto last = args.end() - 1;

  for (auto it = args.begin(); it != args.end(); it++) {
    if (it != args.begin() && it != last) {
      buf.append(", ");
    } else if (it == last) {
      buf.append(":");
    }
    buf.append(to_string(*it));
  }

  ESP_LOGD(TAG, "Command: %s", buf.c_str());
}

}  // namespace gpio
}  // namespace esphome
