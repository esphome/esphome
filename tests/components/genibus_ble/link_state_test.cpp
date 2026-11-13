#include <gtest/gtest.h>

#include "esphome/components/genibus_ble/link_state.h"

namespace esphome::genibus_ble::testing {

namespace {

constexpr uint16_t GENI_HANDLE = 0x0010;
constexpr uint16_t OTHER_HANDLE = 0x0020;
constexpr uint16_t DESCRIPTOR_HANDLE = 0x0011;

// Notifications on, settled and encrypted
void make_ready(LinkState &link) {
  link.settle();
  link.settled();
  link.encrypted();
}

LinkState connected() {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  make_ready(link);
  link.up();
  return link;
}

}  // namespace

TEST(GenibusBleLinkState, NoNotifyBeforeTheLinkIsUp) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  EXPECT_TRUE(link.is_ours(GENI_HANDLE));
  EXPECT_FALSE(link.accepts(GENI_HANDLE));
  make_ready(link);
  link.up();
  EXPECT_TRUE(link.accepts(GENI_HANDLE));
  EXPECT_FALSE(link.accepts(OTHER_HANDLE));
}

TEST(GenibusBleLinkState, NoHandleIsOursBeforeTheSearch) {
  LinkState link;
  EXPECT_FALSE(link.is_ours(0));
  EXPECT_FALSE(link.accepts(0));
}

TEST(GenibusBleLinkState, DisconnectThenCloseEndsTheLinkOnce) {
  LinkState link = connected();
  EXPECT_TRUE(link.down());
  EXPECT_FALSE(link.is_up());
  EXPECT_FALSE(link.accepts(GENI_HANDLE));
  EXPECT_FALSE(link.is_ours(GENI_HANDLE));
  EXPECT_FALSE(link.down());
}

TEST(GenibusBleLinkState, CloseAloneEndsTheLink) {
  LinkState link = connected();
  EXPECT_TRUE(link.down());
  EXPECT_FALSE(link.is_up());
}

TEST(GenibusBleLinkState, CloseBeforeTheLinkIsUpForgetsTheHandle) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  EXPECT_FALSE(link.down());
  EXPECT_FALSE(link.is_ours(GENI_HANDLE));
}

TEST(GenibusBleLinkState, FaultIsReportedOncePerSeries) {
  LinkState link;
  EXPECT_FALSE(link.has_fault());
  EXPECT_TRUE(link.fault());
  EXPECT_FALSE(link.fault());
  EXPECT_FALSE(link.down());
  EXPECT_TRUE(link.has_fault());
  EXPECT_FALSE(link.fault());
}

TEST(GenibusBleLinkState, LinkUpEndsTheFaultSeries) {
  LinkState link;
  link.fault();
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  make_ready(link);
  EXPECT_TRUE(link.up());
  EXPECT_FALSE(link.has_fault());
  EXPECT_TRUE(link.down());
  EXPECT_TRUE(link.fault());
}

TEST(GenibusBleLinkState, LinkUpWithoutFaultClearsNothing) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  make_ready(link);
  EXPECT_FALSE(link.up());
}

TEST(GenibusBleLinkState, SettlingLinkIsNotUp) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  link.settle();
  EXPECT_TRUE(link.is_settling());
  EXPECT_FALSE(link.is_up());
  EXPECT_FALSE(link.accepts(GENI_HANDLE));
  link.settled();
  link.encrypted();
  link.up();
  EXPECT_FALSE(link.is_settling());
  EXPECT_TRUE(link.is_up());
  EXPECT_TRUE(link.accepts(GENI_HANDLE));
}

TEST(GenibusBleLinkState, LinkComesUpOnlyAfterSettling) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  link.fault();
  EXPECT_FALSE(link.up());
  EXPECT_FALSE(link.is_up());
  EXPECT_TRUE(link.has_fault());
}

TEST(GenibusBleLinkState, DisconnectWhileSettlingEndsNothing) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  link.settle();
  EXPECT_FALSE(link.down());
  EXPECT_FALSE(link.is_settling());
  EXPECT_FALSE(link.up());
  EXPECT_FALSE(link.is_up());
}

TEST(GenibusBleLinkState, UpLinkDoesNotSettleAgain) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  EXPECT_TRUE(link.settle());
  link.settled();
  link.encrypted();
  link.up();
  EXPECT_FALSE(link.settle());
  EXPECT_FALSE(link.is_settling());
  EXPECT_TRUE(link.is_up());
}

TEST(GenibusBleLinkState, LinkWaitsForEncryptionAfterSettling) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  link.settle();
  EXPECT_FALSE(link.settled());
  EXPECT_FALSE(link.up());
  EXPECT_FALSE(link.is_up());
  EXPECT_TRUE(link.encrypted());
  link.up();
  EXPECT_TRUE(link.is_up());
}

TEST(GenibusBleLinkState, LinkWaitsForSettlingAfterEncryption) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  EXPECT_FALSE(link.encrypted());
  link.settle();
  EXPECT_FALSE(link.is_ready());
  EXPECT_TRUE(link.settled());
  link.up();
  EXPECT_TRUE(link.is_up());
}

TEST(GenibusBleLinkState, SettlingEndsOnlyAfterNotificationsAreOn) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  link.encrypted();
  EXPECT_FALSE(link.settled());
  EXPECT_FALSE(link.is_ready());
}

TEST(GenibusBleLinkState, DisconnectForgetsTheEncryption) {
  LinkState link = connected();
  link.down();
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  link.settle();
  EXPECT_FALSE(link.settled());
  EXPECT_FALSE(link.up());
  EXPECT_FALSE(link.is_up());
}

TEST(GenibusBleLinkState, RepeatedSubscriptionSettlesAgain) {
  LinkState link;
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  link.encrypted();
  link.settle();
  link.settled();
  EXPECT_TRUE(link.settle());
  EXPECT_FALSE(link.is_ready());
  EXPECT_TRUE(link.settled());
}

TEST(GenibusBleLinkState, ActiveFromTheDiscoveryUntilDown) {
  LinkState link;
  EXPECT_FALSE(link.is_active());
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  EXPECT_TRUE(link.is_active());
  make_ready(link);
  link.up();
  EXPECT_TRUE(link.is_active());
  link.down();
  EXPECT_FALSE(link.is_active());
}

TEST(GenibusBleLinkState, DescriptorBelongsToThisConnection) {
  LinkState link;
  EXPECT_FALSE(link.is_our_descriptor(0));
  link.set_handles(GENI_HANDLE, DESCRIPTOR_HANDLE);
  EXPECT_TRUE(link.is_our_descriptor(DESCRIPTOR_HANDLE));
  EXPECT_FALSE(link.is_our_descriptor(GENI_HANDLE));
  link.down();
  EXPECT_FALSE(link.is_our_descriptor(DESCRIPTOR_HANDLE));
}

}  // namespace esphome::genibus_ble::testing
