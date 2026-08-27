// The host test build gets this from the manifest override; clang-tidy does not.
#ifndef USE_BLE_SCAN_RESPONSE_MERGER
#define USE_BLE_SCAN_RESPONSE_MERGER
#endif

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "esphome/components/ble_device_base/scan_response_merger.h"

namespace esphome::ble_device_base::testing {
namespace {

// Pins the merge policy three trackers share (ln882h, rp2, bk72xx): slot
// bookkeeping, the same-device reuse path, the table-full fallback, the
// 62-byte truncation, the advertisement-RSSI choice and the raw_only gate.
// Delivery is observed through a real AdvDispatcher: the raw callback sees
// every frame (including raw_only), a listener only the parsed ones.

struct DeliveredFrame {
  uint64_t address;
  std::vector<uint8_t> data;
  int8_t rssi;
};

struct RawCapture {
  std::vector<DeliveredFrame> frames;

  static void trampoline(void *self, const RawAdvertisement &adv) {
    auto *capture = static_cast<RawCapture *>(self);
    capture->frames.push_back({adv.address, std::vector<uint8_t>(adv.data, adv.data + adv.data_len), adv.rssi});
  }
};

class CountingListener : public ESPBTDeviceListener {
 public:
  bool parse_device(const ESPBTDevice &device) override {
    this->parsed++;
    return true;  // claimed: keeps the discovered log quiet
  }
  int parsed{0};
};

class ScanResponseMergerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    this->dispatcher_.set_raw_advertisement_callback({&this->raw_, &RawCapture::trampoline});
    this->dispatcher_.register_listener(&this->listener_);
    this->merger_.bind(&this->dispatcher_, &this->scan_continuous_, "test");
  }

  void stash_(const uint8_t (&mac)[6], int8_t rssi, uint8_t data_len, uint8_t fill, uint32_t now = 0) {
    std::vector<uint8_t> data(data_len, fill);
    this->merger_.stash_adv(mac, rssi, 0, data.data(), data_len, now);
  }

  void scan_rsp_(const uint8_t (&mac)[6], int8_t rssi, uint8_t data_len, uint8_t fill) {
    std::vector<uint8_t> data(data_len, fill);
    this->merger_.submit_scan_rsp(mac, rssi, 0, data.data(), data_len);
  }

  ScanResponseMerger merger_;
  AdvDispatcher dispatcher_;
  RawCapture raw_;
  CountingListener listener_;
  bool scan_continuous_{true};
};

constexpr uint8_t MAC_A[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
constexpr uint8_t MAC_B[6] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};

TEST_F(ScanResponseMergerTest, MatchedPairDeliversOneMergedFrameWithAdvRssi) {
  this->stash_(MAC_A, -40, 20, 0xAA);
  EXPECT_TRUE(this->raw_.frames.empty());  // held, not delivered

  this->scan_rsp_(MAC_A, -70, 10, 0xBB);
  ASSERT_EQ(this->raw_.frames.size(), 1u);
  const auto &frame = this->raw_.frames[0];
  ASSERT_EQ(frame.data.size(), 30u);  // adv + response as ONE frame
  EXPECT_EQ(frame.data[0], 0xAA);
  EXPECT_EQ(frame.data[19], 0xAA);
  EXPECT_EQ(frame.data[20], 0xBB);
  // The advertisement's RSSI, never the scan response's.
  EXPECT_EQ(frame.rssi, -40);
  EXPECT_EQ(this->listener_.parsed, 1);
  EXPECT_TRUE(this->merger_.empty());
}

TEST_F(ScanResponseMergerTest, ReAdvertisementDeliversHeldFrameAndReusesSlot) {
  this->stash_(MAC_A, -40, 20, 0xAA);
  this->stash_(MAC_A, -45, 22, 0xCC);  // same device again: first frame is delivered
  ASSERT_EQ(this->raw_.frames.size(), 1u);
  EXPECT_EQ(this->raw_.frames[0].data.size(), 20u);
  EXPECT_EQ(this->raw_.frames[0].rssi, -40);
  EXPECT_FALSE(this->merger_.empty());  // the second advertisement now holds the slot

  this->scan_rsp_(MAC_A, -70, 5, 0xBB);
  ASSERT_EQ(this->raw_.frames.size(), 2u);
  EXPECT_EQ(this->raw_.frames[1].data.size(), 27u);  // 22 + 5, merged from the reused slot
  EXPECT_EQ(this->raw_.frames[1].rssi, -45);
}

TEST_F(ScanResponseMergerTest, FullTableDegradesToUnmergedDelivery) {
  uint8_t mac[6] = {0x20, 0x00, 0x00, 0x00, 0x00, 0x00};
  for (uint8_t i = 0; i < 8; i++) {
    mac[5] = i;
    this->stash_(mac, -50, 10, i);
  }
  EXPECT_TRUE(this->raw_.frames.empty());  // 8 slots, all held

  mac[5] = 8;
  this->stash_(mac, -50, 10, 8);            // 9th device: no slot left
  ASSERT_EQ(this->raw_.frames.size(), 1u);  // delivered immediately, unmerged
  EXPECT_EQ(this->raw_.frames[0].data.size(), 10u);

  this->merger_.flush();  // the 8 held frames are all still intact
  EXPECT_EQ(this->raw_.frames.size(), 9u);
  EXPECT_TRUE(this->merger_.empty());
}

TEST_F(ScanResponseMergerTest, MergeTruncatesAtBufferCapacity) {
  this->stash_(MAC_A, -40, 31, 0xAA);
  this->scan_rsp_(MAC_A, -70, 40, 0xBB);  // only 31 bytes of room remain
  ASSERT_EQ(this->raw_.frames.size(), 1u);
  EXPECT_EQ(this->raw_.frames[0].data.size(), 62u);
  EXPECT_EQ(this->raw_.frames[0].data[31], 0xBB);
  EXPECT_EQ(this->raw_.frames[0].data[61], 0xBB);
}

TEST_F(ScanResponseMergerTest, UnmatchedScanResponseIsRawOnly) {
  this->scan_rsp_(MAC_B, -60, 12, 0xDD);
  ASSERT_EQ(this->raw_.frames.size(), 1u);  // still forwarded on the raw path
  EXPECT_EQ(this->raw_.frames[0].rssi, -60);
  EXPECT_EQ(this->listener_.parsed, 0);  // but never parsed for listeners
}

TEST_F(ScanResponseMergerTest, AddrTypeIsPartOfTheMatchKey) {
  std::vector<uint8_t> adv(20, 0xAA);
  this->merger_.stash_adv(MAC_A, -40, /*addr_type=*/0, adv.data(), adv.size(), 0);
  std::vector<uint8_t> rsp(10, 0xBB);
  this->merger_.submit_scan_rsp(MAC_A, -70, /*addr_type=*/1, rsp.data(), rsp.size());
  // Same MAC, different addr_type: no merge — the response goes out raw_only.
  ASSERT_EQ(this->raw_.frames.size(), 1u);
  EXPECT_EQ(this->raw_.frames[0].data.size(), 10u);
  EXPECT_EQ(this->listener_.parsed, 0);
  EXPECT_FALSE(this->merger_.empty());  // the advertisement is still held
}

TEST_F(ScanResponseMergerTest, SweepDeliversOnlyPastTheTimeout) {
  this->stash_(MAC_A, -40, 20, 0xAA, /*now=*/1000);
  this->merger_.sweep(1300);  // exactly 300 ms: not yet past the timeout
  EXPECT_TRUE(this->raw_.frames.empty());
  this->merger_.sweep(1301);
  ASSERT_EQ(this->raw_.frames.size(), 1u);
  EXPECT_EQ(this->raw_.frames[0].rssi, -40);
  EXPECT_EQ(this->listener_.parsed, 1);  // timeout delivery is a full parse, not raw_only
  EXPECT_TRUE(this->merger_.empty());
}

TEST_F(ScanResponseMergerTest, FlushDeliversEverythingImmediately) {
  this->stash_(MAC_A, -40, 20, 0xAA, /*now=*/1000);
  this->stash_(MAC_B, -50, 15, 0xBB, /*now=*/1000);
  this->merger_.flush();
  EXPECT_EQ(this->raw_.frames.size(), 2u);
  EXPECT_EQ(this->listener_.parsed, 2);
  EXPECT_TRUE(this->merger_.empty());
}

TEST_F(ScanResponseMergerTest, UnboundMergerDropsInsteadOfCrashing) {
  ScanResponseMerger unbound;
  std::vector<uint8_t> data(20, 0xAA);
  unbound.stash_adv(MAC_A, -40, 0, data.data(), data.size(), 0);
  unbound.submit_scan_rsp(MAC_A, -70, 0, data.data(), data.size());
  unbound.sweep(1000);
  unbound.flush();  // no null jump anywhere
  EXPECT_TRUE(unbound.empty());
}

TEST_F(ScanResponseMergerTest, PartialBindIsTreatedAsUnbound) {
  ScanResponseMerger partial;
  partial.bind(&this->dispatcher_, nullptr, "test");
  std::vector<uint8_t> data(20, 0xAA);
  partial.submit_scan_rsp(MAC_A, -70, 0, data.data(), data.size());
  partial.stash_adv(MAC_A, -40, 0, data.data(), data.size(), 0);
  partial.flush();  // dropped, not dispatched through half a binding
  EXPECT_TRUE(this->raw_.frames.empty());
}

}  // namespace
}  // namespace esphome::ble_device_base::testing
