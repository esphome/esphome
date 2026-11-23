#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <vector>

namespace esphome::remote_base {

std::vector<uint16_t> encode_pronto(const std::string &str);

struct ProntoData {
  std::string data;
  int delta;

  bool operator==(const ProntoData &rhs) const;
};

class ProntoProtocol : public RemoteProtocol<ProntoData> {
 private:
  void send_pronto_(RemoteTransmitData *dst, const std::vector<uint16_t> &data);
  void send_pronto_(RemoteTransmitData *dst, const std::string &str);

 public:
  /// Characters per number of the Pronto text: four hex digits and a space.
  static constexpr size_t TEXT_NUMBER_LENGTH = 5;
  /// Count of numbers in the Pronto text decode() builds from these timings.
  static size_t number_count(RawTimingsView data);
  /// Writes numbers [first, first + count) of that text, TEXT_NUMBER_LENGTH characters each.
  static void write_numbers(RawTimingsView data, size_t first, size_t count, char *out);

  void encode(RemoteTransmitData *dst, const ProntoData &data);
  optional<ProntoData> decode(RemoteReceiveData src);
};

using ProntoBinarySensor = RemoteReceiverBinarySensor<ProntoProtocol>;
using ProntoTrigger = RemoteReceiverTrigger<ProntoProtocol>;

/// Logs the Pronto text straight from the timings in chunks, so a dump never allocates.
class ProntoDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override;
};

}  // namespace esphome::remote_base
