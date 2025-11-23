#pragma once

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "constants.h"

namespace esphome {
namespace sml {

using bytes = std::vector<uint8_t>;

class BytesView {
 public:
  BytesView() noexcept = default;

  explicit BytesView(const uint8_t *first, size_t count) noexcept : data_{first}, count_{count} {}

  explicit BytesView(const bytes &bytes) noexcept : data_{bytes.data()}, count_{bytes.size()} {}

  size_t size() const noexcept { return count_; }

  uint8_t operator[](size_t index) const noexcept {
    assert(index < count_);
    return data_[index];
  }

  BytesView subview(size_t offset, size_t count) const noexcept {
    assert(offset + count <= count_);
    return BytesView{data_ + offset, count};
  }

  const uint8_t *begin() const noexcept { return data_; }

  const uint8_t *end() const noexcept { return data_ + count_; }

 private:
  const uint8_t *data_ = nullptr;
  size_t count_ = 0;
};

class SmlNode {
 public:
  uint8_t type;
  BytesView value_bytes;
  std::vector<SmlNode> nodes;
};

class ObisInfo {
 public:
  ObisInfo(const BytesView &server_id, const SmlNode &val_list_entry);
  BytesView server_id;
  BytesView code;
  BytesView status;
  char unit;
  char scaler;
  BytesView value;
  uint16_t value_type;
  std::string code_repr() const;
};

class SmlFile {
 public:
  SmlFile(const BytesView &buffer);
  bool setup_node(SmlNode *node);
  std::vector<SmlNode> messages;
  std::vector<ObisInfo> get_obis_info();

 protected:
  const BytesView buffer_;
  size_t pos_;
};

std::string bytes_repr(const BytesView &buffer);

uint64_t bytes_to_uint(const BytesView &buffer);

int64_t bytes_to_int(const BytesView &buffer);

std::string bytes_to_string(const BytesView &buffer);
}  // namespace sml
}  // namespace esphome
