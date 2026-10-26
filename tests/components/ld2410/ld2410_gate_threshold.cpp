#include <algorithm>
#include <vector>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "esphome/components/ld2410/ld2410.h"
#include "esphome/components/uart/uart_component.h"

namespace esphome::ld2410::testing {

namespace {
class CapturingUARTComponent : public uart::UARTComponent {
 public:
  void write_array(const uint8_t *data, size_t len) override {
    this->written.insert(this->written.end(), data, data + len);
  }
  MOCK_METHOD(bool, read_array, (uint8_t * data, size_t len), (override));
  MOCK_METHOD(bool, peek_byte, (uint8_t * data), (override));
  MOCK_METHOD(size_t, available, (), (override));
  MOCK_METHOD(uart::UARTFlushResult, flush, (), (override));
  MOCK_METHOD(void, check_logger_conflict, (), (override));
#if defined(USE_ESP8266) || defined(USE_ESP32)
  void load_settings(bool dump_config) override {}
#endif  // USE_ESP8266 || USE_ESP32
  std::vector<uint8_t> written;
};

LD2410Component *hub = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Same body codegen emits for publish="before": publish the new value, then hand the number to the hub.
void apply_threshold(number::Number *entity, float value) {
  entity->publish_state(value);
  hub->set_gate_threshold(entity);
}
using ThresholdNumber = number::ApplyNumber<apply_threshold>;

// Only these gates get numbers; gate 8 is the last one.
constexpr uint8_t GATES[] = {4, 8};

// Header, length 20, command 0x64 (set gate sensitivity): the 18 byte payload follows.
constexpr uint8_t GATE_SENS_PREFIX[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x14, 0x00, 0x64, 0x00};

std::vector<uint8_t> gate_sens_payload(uint8_t gate, uint8_t move, uint8_t still) {
  return {0x00, 0x00, gate, 0x00, 0x00, 0x00, 0x01, 0x00, move, 0x00, 0x00, 0x00, 0x02, 0x00, still, 0x00, 0x00, 0x00};
}
}  // namespace

class LD2410GateThresholdTest : public ::testing::Test {
 protected:
  void SetUp() override {
    hub = &this->ld2410_;
    this->ld2410_.set_uart_parent(&this->uart_);
    // Gates 0 to 3 and 5 to 7 have no numbers, so the lookup must walk past the empty slots.
    for (size_t i = 0; i < 2; i++) {
      const uint8_t gate = GATES[i];
      this->ld2410_.set_gate_move_threshold_number(gate, &this->move_[i]);
      this->ld2410_.set_gate_still_threshold_number(gate, &this->still_[i]);
      for (ThresholdNumber *n : {&this->move_[i], &this->still_[i]}) {
        n->traits.set_min_value(0);
        n->traits.set_max_value(100);
        n->traits.set_step(1);
      }
      this->move_[i].publish_state(10 + gate);
      this->still_[i].publish_state(20 + gate);
    }
  }
  void TearDown() override { hub = nullptr; }

  // Every gate sensitivity command written since the last clear, as its payload.
  std::vector<std::vector<uint8_t>> sent_gate_commands_() const {
    std::vector<std::vector<uint8_t>> out;
    const auto &w = this->uart_.written;
    auto it = w.begin();
    while ((it = std::search(it, w.end(), std::begin(GATE_SENS_PREFIX), std::end(GATE_SENS_PREFIX))) != w.end()) {
      it += sizeof(GATE_SENS_PREFIX);
      if (static_cast<size_t>(w.end() - it) < 18)
        break;
      out.emplace_back(it, it + 18);
      it += 18;
    }
    return out;
  }

  CapturingUARTComponent uart_;
  LD2410Component ld2410_;
  ThresholdNumber move_[2];
  ThresholdNumber still_[2];
};

TEST_F(LD2410GateThresholdTest, DirectCallSendsTheNumbersGateWithBothValues) {
  for (size_t i = 0; i < 2; i++) {
    for (ThresholdNumber *n : {&this->move_[i], &this->still_[i]}) {
      this->uart_.written.clear();
      this->ld2410_.set_gate_threshold(n);
      auto sent = this->sent_gate_commands_();
      ASSERT_EQ(sent.size(), 1u) << "gate " << +GATES[i];
      EXPECT_EQ(sent[0], gate_sens_payload(GATES[i], 10 + GATES[i], 20 + GATES[i])) << "gate " << +GATES[i];
    }
  }
}

TEST_F(LD2410GateThresholdTest, PublishBeforeApplySendsTheNewValue) {
  this->move_[1].make_call().set_value(55).perform();
  auto sent = this->sent_gate_commands_();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0], gate_sens_payload(8, 55, 28));

  this->uart_.written.clear();
  this->still_[0].make_call().set_value(66).perform();
  sent = this->sent_gate_commands_();
  ASSERT_EQ(sent.size(), 1u);
  EXPECT_EQ(sent[0], gate_sens_payload(4, 14, 66));
}

TEST_F(LD2410GateThresholdTest, UnregisteredNumberSendsNothing) {
  ThresholdNumber other;
  other.publish_state(42);
  this->ld2410_.set_gate_threshold(&other);
  EXPECT_TRUE(this->uart_.written.empty());
}

}  // namespace esphome::ld2410::testing
