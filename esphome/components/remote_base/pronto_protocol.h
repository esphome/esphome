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
  /// Length of the Pronto text decode() builds from these timings.
  static size_t text_length(RawTimingsView data);
  /// Writes characters [offset, offset + len) of that text; offset + len must not exceed text_length(data).
  static void write_text(RawTimingsView data, size_t offset, char *out, size_t len);

  void encode(RemoteTransmitData *dst, const ProntoData &data);
  optional<ProntoData> decode(RemoteReceiveData src);
  void dump(const ProntoData &data);
};

using ProntoBinarySensor = RemoteReceiverBinarySensor<ProntoProtocol>;
using ProntoTrigger = RemoteReceiverTrigger<ProntoProtocol>;

/// Logs the Pronto text straight from the timings in chunks, so a dump never allocates.
class ProntoDumper : public RemoteReceiverDumperBase {
 public:
  bool dump(RemoteReceiveData src) override;
};

}  // namespace esphome::remote_base
