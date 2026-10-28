#include "vnc_test_helpers.h"

#ifdef USE_HOST

namespace esphome::vnc::testing {

static const Bytes SECRET12_RESPONSE = {0xAD, 0xCD, 0x99, 0x7F, 0x8E, 0x16, 0xFE, 0xE5,
                                        0x75, 0xE9, 0x73, 0xF9, 0x3C, 0x2B, 0x62, 0xB4};

static Bytes running_tail(const Bytes &pixel_format, uint16_t w, uint16_t h) {
  return cat({pixel_format, set_encodings(), fbur(false, w, h)});
}

TEST(VncHandshake, Rfb38NoneExactBytes) {
  FakeDisplay display(64, 48);
  Session s(&display);
  int connects = 0;
  s.client.add_on_connect_callback([&connects]() { connects++; });
  s.attach();
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_HANDSHAKE);

  s.deliver(Rfb().version());
  EXPECT_EQ(hex(s.take()), hex(version_reply('8')));
  s.deliver(Rfb().security_list({1}));
  EXPECT_EQ(hex(s.take()), hex({1}));
  s.deliver(Rfb().security_result(0));
  EXPECT_EQ(hex(s.take()), hex({1}));  // ClientInit, shared
  EXPECT_FALSE(s.client.is_connected());
  s.deliver(Rfb().server_init(80, 40));
  // The first request covers only what the display can show.
  EXPECT_EQ(hex(s.take()), hex(running_tail(set_pixel_format_565(), 64, 40)));
  EXPECT_TRUE(s.client.is_connected());
  EXPECT_EQ(s.client.sec_type(), 1);
  EXPECT_EQ(connects, 1);
}

TEST(VncHandshake, Rfb38NonePipelined) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(handshake_38(32, 100));
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('8'), {1}, {1}, running_tail(set_pixel_format_565(), 32, 48)})));
  EXPECT_TRUE(s.client.is_connected());
}

struct PixelFormatCase {
  display::DisplayType type;
  VncPixelFormat format;
  bool rgb565;
};

class VncPixelFormatSelection : public ::testing::TestWithParam<PixelFormatCase> {};

TEST_P(VncPixelFormatSelection, SetPixelFormatMatchesTheDisplay) {
  const auto &param = GetParam();
  FakeDisplay display(64, 48, param.type);
  Session s(&display);
  s.client.set_pixel_format(param.format);
  s.attach();
  s.deliver(handshake_38(64, 48));
  Bytes tx = s.take();
  const Bytes expected = running_tail(param.rgb565 ? set_pixel_format_565() : set_pixel_format_332(), 64, 48);
  ASSERT_GE(tx.size(), expected.size());
  EXPECT_EQ(hex(Bytes(tx.end() - expected.size(), tx.end())), hex(expected));
  EXPECT_EQ(s.client.bpp(), param.rgb565 ? 2 : 1);
}

INSTANTIATE_TEST_SUITE_P(
    Formats, VncPixelFormatSelection,
    ::testing::Values(PixelFormatCase{display::DISPLAY_TYPE_COLOR, VncPixelFormat::VNC_PIXEL_FORMAT_AUTO, true},
                      PixelFormatCase{display::DISPLAY_TYPE_GRAYSCALE, VncPixelFormat::VNC_PIXEL_FORMAT_AUTO, true},
                      PixelFormatCase{display::DISPLAY_TYPE_BINARY, VncPixelFormat::VNC_PIXEL_FORMAT_AUTO, false},
                      PixelFormatCase{display::DISPLAY_TYPE_COLOR, VncPixelFormat::VNC_PIXEL_FORMAT_RGB332, false},
                      PixelFormatCase{display::DISPLAY_TYPE_BINARY, VncPixelFormat::VNC_PIXEL_FORMAT_RGB565, true}));

TEST(VncHandshake, Rfb37NoneHasNoSecurityResult) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version("RFB 003.007\n"));
  EXPECT_EQ(hex(s.take()), hex(version_reply('7')));
  s.deliver(Rfb().security_list({1}));
  // Type and ClientInit go out together: 3.7 sends no SecurityResult for None.
  EXPECT_EQ(hex(s.take()), hex({1, 1}));
  s.deliver(Rfb().server_init(64, 48));
  EXPECT_EQ(hex(s.take()), hex(running_tail(set_pixel_format_565(), 64, 48)));
  EXPECT_TRUE(s.client.is_connected());
}

TEST(VncHandshake, Rfb33None) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version("RFB 003.003\n"));
  EXPECT_EQ(hex(s.take()), hex(version_reply('3')));
  s.deliver(Rfb().security_33(1));
  EXPECT_EQ(hex(s.take()), hex({1}));
  s.deliver(Rfb().server_init(64, 48));
  EXPECT_EQ(hex(s.take()), hex(running_tail(set_pixel_format_565(), 64, 48)));
  EXPECT_TRUE(s.client.is_connected());
  EXPECT_EQ(s.client.sec_type(), 1);
}

TEST(VncHandshake, Rfb33RefusalIsTransient) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version("RFB 003.003\n").security_33(0).reason("Too many security failures"));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 5000u);
  EXPECT_FALSE(s.client.status_has_error());
}

TEST(VncHandshake, Rfb33VncAuth) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version("RFB 003.003\n").security_33(2).challenge());
  // 3.3: the server picks the type, so the client sends no type byte.
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('3'), SECRET12_RESPONSE})));
  s.deliver(Rfb().security_result(0).server_init(64, 48));
  EXPECT_EQ(hex(s.take()), hex(cat({{1}, running_tail(set_pixel_format_565(), 64, 48)})));
  EXPECT_TRUE(s.client.is_connected());
}

TEST(VncHandshake, Rfb33PasswordWithoutKeyIsConfig) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version("RFB 003.003\n"));
  EXPECT_EQ(hex(s.take()), hex(version_reply('3')));
  s.deliver(Rfb().security_33(2).challenge());
  EXPECT_EQ(s.take().size(), 0u);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 300000u);
  EXPECT_TRUE(s.client.status_has_error());
}

struct VersionCase {
  const char *server;
  char reply_minor;
};

class VncVersionMapping : public ::testing::TestWithParam<VersionCase> {};

TEST_P(VncVersionMapping, RepliesWithTheMappedVersion) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(text(GetParam().server));
  EXPECT_EQ(hex(s.take()), hex(version_reply(GetParam().reply_minor)));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_HANDSHAKE);
  EXPECT_EQ(s.client.step(),
            GetParam().reply_minor == '3' ? VncStep::VNC_STEP_SECURITY_TYPE_33 : VncStep::VNC_STEP_SECURITY_COUNT);
}

INSTANTIATE_TEST_SUITE_P(Versions, VncVersionMapping,
                         ::testing::Values(VersionCase{"RFB 003.008\n", '8'}, VersionCase{"RFB 003.889\n", '8'},
                                           VersionCase{"RFB 004.001\n", '8'}, VersionCase{"RFB 005.000\n", '8'},
                                           VersionCase{"RFB 003.007\n", '7'}, VersionCase{"RFB 003.005\n", '3'},
                                           VersionCase{"RFB 003.003\n", '3'}));

class VncBadVersion : public ::testing::TestWithParam<const char *> {};

TEST_P(VncBadVersion, IsAConfigFailureHeldFor300s) {
  FakeDisplay display(64, 48);
  Listener listener;
  Session s(&display);
  s.client.set_port(listener.port());
  s.attach();
  s.deliver(text(GetParam()));
  EXPECT_EQ(s.take().size(), 0u);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 300000u);
  EXPECT_TRUE(s.client.status_has_error());
  const uint32_t failed_at = now_ms();
  // No attempt while the hold runs.
  while (now_ms() - failed_at < 299000) {
    s.tick(1000);
  }
  EXPECT_EQ(listener.accepted(), 0);
  EXPECT_FALSE(s.client.link().connecting());
  set_now(failed_at + 300000);
  EXPECT_TRUE(s.wait_until([&]() { return listener.accepted() == 1; }));
}

INSTANTIATE_TEST_SUITE_P(Versions, VncBadVersion,
                         ::testing::Values("HTTP/1.1 400", "RFB 002.000\n", "RFB 003,008\n", "RFB 0x3.008\n",
                                           "SSH-2.0-Open"));

TEST(VncHandshake, VncAuthWithKey) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({2, 16}));
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('8'), {2}})));
  s.deliver(Rfb().challenge());
  EXPECT_EQ(hex(s.take()), hex(SECRET12_RESPONSE));
  s.deliver(Rfb().security_result(0));
  EXPECT_EQ(hex(s.take()), hex({1}));
  s.deliver(Rfb().server_init(64, 48));
  EXPECT_EQ(hex(s.take()), hex(running_tail(set_pixel_format_565(), 64, 48)));
  EXPECT_TRUE(s.client.is_connected());
  EXPECT_EQ(s.client.sec_type(), 2);
}

TEST(VncHandshake, PicksNoneWhenListedFirstEvenWithKey) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({16, 1, 2}));
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('8'), {1}})));
}

TEST(VncHandshake, PasswordRequiredWithoutKeyIsConfig) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version());
  EXPECT_EQ(hex(s.take()), hex(version_reply('8')));
  s.deliver(Rfb().security_list({2, 16}));
  // Never pick 16, never answer type 2 without a key.
  EXPECT_EQ(s.take().size(), 0u);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 300000u);
  EXPECT_TRUE(s.client.status_has_error());
  EXPECT_FALSE(s.client.link().connected());
}

TEST(VncHandshake, OnlyUnsupportedTypesIsConfig) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version());
  EXPECT_EQ(hex(s.take()), hex(version_reply('8')));
  s.deliver(Rfb().security_list({16}));
  EXPECT_EQ(s.take().size(), 0u);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 300000u);
  EXPECT_TRUE(s.client.status_has_error());
}

TEST(VncHandshake, EmptyListWithReasonIsTransient) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version());
  EXPECT_EQ(hex(s.take()), hex(version_reply('8')));
  s.deliver(Rfb().security_list({}).reason("Too many authentication failures"));
  EXPECT_EQ(s.take().size(), 0u);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 5000u);
  EXPECT_EQ(s.client.backoff_ms(), 5000u);
  EXPECT_TRUE(s.client.status_has_warning());
  EXPECT_FALSE(s.client.status_has_error());
}

TEST(VncHandshake, EmptyReasonEndsAtOnce) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version());
  s.deliver(Rfb().security_list({}).u32(0));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 5000u);
}

TEST(VncHandshake, LongReasonIsCutWithoutWaitingForAllOfIt) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version());
  // Announces 100000 bytes but sends only the first 63 the client logs.
  s.deliver(Rfb().security_list({}).u32(100000).fill(63, 'r'));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 5000u);
}

TEST(VncHandshake, FullSecurityListWithoutAUsableType) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version());
  Rfb list;
  list.u8(255);
  for (int i = 0; i < 255; i++) {
    list.u8(static_cast<uint8_t>(i == 1 || i == 2 ? 16 : i));
  }
  s.deliver(list);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 300000u);
}

TEST(VncHandshake, DumpConfig) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.client.dump_config();
  s.client.set_pixel_format(VncPixelFormat::VNC_PIXEL_FORMAT_RGB332);
  s.client.dump_config();
}

TEST(VncHandshake, AuthFailureLatches) {
  FakeDisplay display(64, 48);
  Listener listener;
  Session s(&display);
  s.client.set_port(listener.port());
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({2, 16}).challenge().security_result(1).reason("Authentication failed"));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
  EXPECT_TRUE(s.client.status_has_error());
  EXPECT_FALSE(s.client.link().connected());
  s.loop();
  EXPECT_TRUE(s.client.is_idle());
  // Even if something keeps calling loop(), the device never tries the wrong password again.
  for (int i = 0; i < 600; i++) {
    set_now(now_ms() + 1000);
    s.client.loop();
  }
  EXPECT_EQ(listener.accepted(), 0);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
  EXPECT_TRUE(s.client.status_has_error());
  EXPECT_FALSE(s.client.link().connecting());

  // The same holds once stopped by vnc.disconnect.
  s.client.connect();
  s.client.disconnect();
  for (int i = 0; i < 600; i++) {
    set_now(now_ms() + 1000);
    s.client.loop();
  }
  EXPECT_EQ(listener.accepted(), 0);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_STOPPED);
  EXPECT_TRUE(s.client.is_idle());
  EXPECT_FALSE(s.client.link().connecting());
}

TEST(VncHandshake, Rfb37AuthFailureLatchesWithoutReason) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version("RFB 003.007\n").security_list({2}).challenge().security_result(1));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
}

TEST(VncHandshake, EofInsideReasonAfterFailureStaysAuth) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge().security_result(1).u32(40).str("Authen"));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_HANDSHAKE);
  s.close_peer();
  s.loop(2);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
}

TEST(VncHandshake, EofBeforeReasonLengthAfterFailureStaysAuth) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge().security_result(1).u8(0));
  s.close_peer();
  s.loop(2);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
}

TEST(VncHandshake, TimeoutInsideReasonAfterFailureStaysAuth) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge().security_result(1).u32(40).str("Au"));
  s.tick(15000);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
}

TEST(VncHandshake, EofAfterResponseHoldsThenLatches) {
  FakeDisplay display(64, 48);
  Listener listener;
  Session s(&display);
  s.client.set_port(listener.port());
  s.client.set_auth_key(SECRET12_KEY);

  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge());
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('8'), {2}, SECRET12_RESPONSE})));
  s.close_peer();
  s.loop(2);
  // The server may or may not have counted the attempt: wait a minute, then try once more.
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 60000u);
  const uint32_t failed_at = now_ms();
  while (now_ms() - failed_at < 59000) {
    s.tick(1000);
  }
  EXPECT_EQ(listener.accepted(), 0);
  set_now(failed_at + 60000);
  EXPECT_TRUE(s.wait_until([&]() { return listener.accepted() == 1; }));

  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge());
  s.close_peer();
  s.loop(2);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
  EXPECT_TRUE(s.client.status_has_error());
}

/// An RFB 3.8 password handshake that the server drops right after the client's response.
static void unclear_drop(Session &s) {
  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge());
  s.close_peer();
  s.loop(2);
}

TEST(VncHandshake, SuccessClearsTheAmbiguousCount) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  unclear_drop(s);
  ASSERT_TRUE(s.client.auth_retried());

  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge().security_result(0).server_init(64, 48));
  ASSERT_TRUE(s.client.is_connected());
  s.close_peer();
  s.loop(2);
  EXPECT_FALSE(s.client.auth_retried());

  // The next unclear drop starts a fresh retry instead of latching.
  unclear_drop(s);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 60000u);
}

TEST(VncHandshake, AcceptedPasswordClearsTheAmbiguousCount) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  unclear_drop(s);
  ASSERT_TRUE(s.client.auth_retried());

  // The password is accepted, then the server drops before ServerInit.
  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge().security_result(0));
  s.close_peer();
  s.loop(2);
  EXPECT_FALSE(s.client.auth_retried());

  unclear_drop(s);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 60000u);
}

TEST(VncHandshake, NoneSecurityResultDoesNotClearTheAmbiguousCount) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  unclear_drop(s);
  ASSERT_TRUE(s.client.auth_retried());

  // No password is checked, then the server drops before ServerInit.
  s.attach();
  s.deliver(Rfb().version().security_list({1}).security_result(0));
  s.close_peer();
  s.loop(2);
  EXPECT_TRUE(s.client.auth_retried());
}

TEST(VncHandshake, TransientFailureBetweenUnclearDropsStillLatches) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  unclear_drop(s);
  ASSERT_TRUE(s.client.auth_retried());

  // The retry is refused before the password is sent.
  s.attach();
  s.deliver(Rfb().version());
  s.close_peer();
  s.loop(2);
  EXPECT_TRUE(s.client.auth_retried());

  unclear_drop(s);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
  EXPECT_TRUE(s.client.status_has_error());
}

TEST(VncHandshake, TwoUnclearDropsInARowLatch) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  unclear_drop(s);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  unclear_drop(s);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_AUTH_FAILED);
  EXPECT_TRUE(s.client.status_has_error());
  EXPECT_FALSE(s.client.auth_retried());
}

TEST(VncHandshake, StallAfterResponseIsAmbiguous) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  const uint32_t start = now_ms();
  s.deliver(Rfb().version().security_list({2}).challenge());
  s.at(start + 14999);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_HANDSHAKE);
  s.at(start + 15000);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 60000u);
  EXPECT_FALSE(s.client.link().connected());
}

TEST(VncHandshake, AmbiguousHoldSurvivesANetworkDrop) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  const uint32_t start = now_ms();
  s.deliver(Rfb().version().security_list({2}).challenge());
  s.at(start + 15000);
  ASSERT_EQ(s.client.hold_ms(), 60000u);
  s.client.network_dropped();
  s.tick(10);
  EXPECT_EQ(s.client.hold_ms(), 60000u);
}

TEST(VncHandshake, HandshakeTimeout) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  const uint32_t start = now_ms();
  s.deliver(Rfb().version());
  s.at(start + 14999);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_HANDSHAKE);
  EXPECT_TRUE(s.client.link().connected());
  s.at(start + 15000);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 5000u);
  EXPECT_FALSE(s.client.link().connected());
  EXPECT_FALSE(s.client.status_has_error());
}

TEST(VncHandshake, StaleSecurityTypeGivesConfigNotAuth) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({2, 16}).challenge().security_result(0).server_init(64, 48));
  ASSERT_TRUE(s.client.is_connected());
  ASSERT_EQ(s.client.sec_type(), 2);
  s.close_peer();
  s.loop(2);
  ASSERT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);

  // The server dropped its password; a failed None handshake is not a wrong password.
  s.attach();
  s.deliver(Rfb().version().security_list({1, 16}));
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('8'), {1}})));
  s.deliver(Rfb().security_result(1).reason("Server is busy"));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 300000u);
}

TEST(VncHandshake, StaleSecurityTypeDoesNotMakeADropAmbiguous) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.client.set_auth_key(SECRET12_KEY);
  s.attach();
  s.deliver(Rfb().version().security_list({2}).challenge().security_result(0).server_init(64, 48));
  ASSERT_TRUE(s.client.is_connected());
  s.close_peer();
  s.loop(2);

  s.attach();
  s.deliver(Rfb().version().security_list({1}));
  s.close_peer();
  s.loop(2);
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_FALSE(s.client.auth_retried());
  EXPECT_NE(s.client.hold_ms(), 60000u);
}

TEST(VncHandshake, EmptyServerName) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version().security_list({1}).security_result(0).server_init(64, 48, ""));
  EXPECT_TRUE(s.client.is_connected());
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('8'), {1}, {1}, running_tail(set_pixel_format_565(), 64, 48)})));
}

TEST(VncHandshake, LongServerNameIsSkipped) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version().security_list({1}).security_result(0).server_init(64, 48, std::string(700, 'n')));
  ASSERT_TRUE(s.client.is_connected());
  s.take();
  s.deliver(Rfb().fbu(1).raw_rect(0, 0, 1, 1, {0xF800}));
  EXPECT_EQ(display.pixel(0, 0), expect_px(0xF800, 2));
  EXPECT_TRUE(s.client.is_connected());
}

TEST(VncHandshake, RunningOnlyAfterTheWholeServerName) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  Bytes init = Rfb().version().security_list({1}).security_result(0).server_init(64, 48, std::string(700, 'n')).bytes;
  s.deliver(Bytes(init.begin(), init.end() - 1));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_HANDSHAKE);
  s.deliver(Bytes(init.end() - 1, init.end()));
  EXPECT_TRUE(s.client.is_connected());
}

TEST(VncHandshake, EmptyServerScreenIsTransient) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version().security_list({1}).security_result(0).server_init(0, 48));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 5000u);
}

TEST(VncHandshake, DisconnectCallbackOnlyAfterRunning) {
  FakeDisplay display(64, 48);
  Session s(&display);
  int disconnects = 0;
  s.client.add_on_disconnect_callback([&disconnects]() { disconnects++; });
  s.attach();
  s.deliver(Rfb().version());
  s.close_peer();
  s.loop(2);
  EXPECT_EQ(disconnects, 0);

  s.start(64, 48);
  s.close_peer();
  s.loop(2);
  EXPECT_EQ(disconnects, 1);
  EXPECT_FALSE(s.client.is_connected());
}

}  // namespace esphome::vnc::testing

#endif  // USE_HOST
