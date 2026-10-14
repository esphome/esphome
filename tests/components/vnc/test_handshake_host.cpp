#include "vnc_test_helpers.h"

#ifdef USE_HOST

namespace esphome::vnc::testing {

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
  s.client.dump_config();
  s.client.set_pixel_format(VncPixelFormat::VNC_PIXEL_FORMAT_RGB332);
  s.client.dump_config();
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

TEST(VncHandshake, NoneSecurityFailureIsConfig) {
  FakeDisplay display(64, 48);
  Session s(&display);
  s.attach();
  s.deliver(Rfb().version().security_list({1, 16}));
  EXPECT_EQ(hex(s.take()), hex(cat({version_reply('8'), {1}})));
  s.deliver(Rfb().security_result(1).reason("Server is busy"));
  EXPECT_EQ(s.client.state(), VncState::VNC_STATE_IDLE);
  EXPECT_EQ(s.client.hold_ms(), 300000u);
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
