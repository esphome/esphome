#include <gtest/gtest.h>

#include <csignal>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "esphome/components/rtsp_server/rtsp_session.h"

#if defined(USE_HOST) && defined(USE_CAMERA)

namespace esphome::rtsp_server::testing {

static constexpr uint16_t PORT = 554;
static constexpr size_t REQUEST_BUF_SIZE = 512;

class StubImage : public camera::CameraImage {
 public:
  explicit StubImage(size_t len) : data_(len, 0x55) {}
  uint8_t *get_data_buffer() override { return this->data_.data(); }
  size_t get_data_length() override { return this->data_.size(); }
  bool was_requested_by(camera::CameraRequester requester) const override { return true; }

 protected:
  std::vector<uint8_t> data_;
};

class RTSPSessionUnderTest : public RTSPSession {
 public:
  bool has_frame() const { return this->frame_ != nullptr; }
  bool audio_in_flight() const { return this->output_ == Output::OUTPUT_AUDIO && this->iov_pos_ < this->iov_count_; }
  uint32_t audio_in_flight_index() const { return this->audio_in_flight_; }
  uint16_t filler_len() const { return this->filler_len_; }
};

class RTSPSessionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    signal(SIGPIPE, SIG_IGN);
    int fds[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
    this->peer_fd_ = fds[1];
    this->session_.open(std::make_unique<socket::Socket>(fds[0]), this->now_);
  }
  void TearDown() override { ::close(this->peer_fd_); }

  // Reopens the session on a loopback TCP connection, which (unlike AF_UNIX) accepts partial writes.
  void reopen_over_tcp() {
    int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof(addr);
    ASSERT_EQ(::bind(listener, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);
    ASSERT_EQ(::listen(listener, 1), 0);
    ASSERT_EQ(::getsockname(listener, reinterpret_cast<sockaddr *>(&addr), &len), 0);
    int client = ::socket(AF_INET, SOCK_STREAM, 0);
    int size = 8192;
    ::setsockopt(client, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    ASSERT_EQ(::connect(client, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);
    int server = ::accept(listener, nullptr, nullptr);
    ::close(listener);
    ASSERT_GE(server, 0);
    ::setsockopt(server, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
#ifdef SO_SNDLOWAT
    int lowat = 1;  // macOS otherwise refuses writes smaller than its 2 KiB low-water mark
    ::setsockopt(server, SOL_SOCKET, SO_SNDLOWAT, &lowat, sizeof(lowat));
#endif
    ::close(this->peer_fd_);
    this->peer_fd_ = client;
    this->session_.open(std::make_unique<socket::Socket>(server), this->now_);
  }

  void send(const std::string &data) {
    ASSERT_EQ(::write(this->peer_fd_, data.data(), data.size()), static_cast<ssize_t>(data.size()));
  }
  void loops(int count) {
    for (int i = 0; i < count; i++) {
      usleep(1000);  // let loopback TCP deliver
      this->session_.loop(this->now_, PORT);
    }
  }
  // Everything the session has written so far.
  std::string receive() {
    usleep(1000);
    std::string out;
    char buf[4096];
    while (true) {
      ssize_t n = ::recv(this->peer_fd_, buf, sizeof(buf), MSG_DONTWAIT);
      if (n <= 0)
        break;
      out.append(buf, n);
    }
    return out;
  }
  static std::string request(const char *method, int cseq, const std::string &extra = "") {
    char line[48];
    snprintf(line, sizeof(line), "%s rtsp://host/ RTSP/1.0\r\nCSeq: %d\r\n", method, cseq);
    return line + extra + "\r\n";
  }
  static size_t count(const std::string &haystack, const std::string &needle) {
    size_t n = 0;
    for (size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1))
      n++;
    return n;
  }
  void start_playing() {
    this->send(request("SETUP", 1, "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"));
    this->loops(2);
    this->send(request("PLAY", 2));
    this->loops(2);
    std::string out = this->receive();
    ASSERT_NE(out.find("CSeq: 2\r\n"), std::string::npos);
    ASSERT_TRUE(this->session_.is_playing());
  }

  RTSPSessionUnderTest session_;
  int peer_fd_{-1};
  uint32_t now_{1000};
};

TEST_F(RTSPSessionTest, InterleavedRtcpBeforeRequestIsSkipped) {
  std::string rtcp("$\x01\x00\x08", 4);
  rtcp.append("\x81\xc9\x00\x01\x00\x00\x00\x00", 8);  // contains NUL bytes
  this->send(rtcp + request("GET_PARAMETER", 7));
  this->loops(1);
  std::string out = this->receive();
  EXPECT_EQ(out.rfind("RTSP/1.0 200 OK\r\nCSeq: 7\r\n", 0), 0u) << out;
  EXPECT_TRUE(this->session_.is_active());
}

TEST_F(RTSPSessionTest, InterleavedPacketSplitAcrossReads) {
  this->send(std::string("$\x01\x01", 3));
  this->loops(1);
  std::string rest("\x00", 1);
  rest.append(256, '\0');
  this->send(rest + request("GET_PARAMETER", 8));
  this->loops(1);
  EXPECT_NE(this->receive().find("CSeq: 8\r\n"), std::string::npos);
  EXPECT_TRUE(this->session_.is_active());
}

TEST_F(RTSPSessionTest, MalformedRequestLineEchoesCSeq) {
  this->send("OPTIONS\r\nCSeq: 9\r\n\r\n");
  this->loops(1);
  std::string out = this->receive();
  EXPECT_EQ(out.rfind("RTSP/1.0 400 Bad Request\r\nCSeq: 9\r\n", 0), 0u) << out;
  EXPECT_TRUE(this->session_.is_active());
}

TEST_F(RTSPSessionTest, PipelinedRequestsBothAnswered) {
  this->send(request("OPTIONS", 1) + request("GET_PARAMETER", 2));
  this->loops(3);
  std::string out = this->receive();
  EXPECT_EQ(count(out, "RTSP/1.0 200 OK\r\n"), 2u) << out;
  size_t first = out.find("CSeq: 1\r\n");
  size_t second = out.find("CSeq: 2\r\n");
  ASSERT_NE(first, std::string::npos);
  ASSERT_NE(second, std::string::npos);
  EXPECT_LT(first, second);
}

TEST_F(RTSPSessionTest, RequestBodyIsConsumed) {
  this->send(request("GET_PARAMETER", 3, "Content-Length: 6\r\n") + "ping\r\n" + request("OPTIONS", 4));
  this->loops(3);
  std::string out = this->receive();
  EXPECT_EQ(count(out, "RTSP/1.0 200 OK\r\n"), 2u) << out;
  EXPECT_EQ(out.find("400"), std::string::npos) << out;
  size_t first = out.find("CSeq: 3\r\n");
  size_t second = out.find("CSeq: 4\r\n");
  ASSERT_NE(first, std::string::npos);
  ASSERT_NE(second, std::string::npos);
  EXPECT_LT(first, second);
}

TEST_F(RTSPSessionTest, RequestBodySplitAcrossReads) {
  this->send(request("GET_PARAMETER", 3, "Content-Length: 6\r\n") + "pi");
  this->loops(2);
  this->send("ng\r\n" + request("OPTIONS", 4));
  this->loops(3);
  std::string out = this->receive();
  EXPECT_EQ(count(out, "RTSP/1.0 200 OK\r\n"), 2u) << out;
  EXPECT_EQ(out.find("400"), std::string::npos) << out;
  EXPECT_NE(out.find("CSeq: 4\r\n"), std::string::npos) << out;
  EXPECT_TRUE(this->session_.is_active());
}

TEST_F(RTSPSessionTest, OversizedContentLengthClosesSession) {
  this->send(request("GET_PARAMETER", 3, "Content-Length: 100000\r\n"));
  this->loops(2);
  EXPECT_EQ(this->receive().rfind("RTSP/1.0 400 Bad Request\r\nCSeq: 3\r\n", 0), 0u);
  EXPECT_FALSE(this->session_.is_active());
}

static std::string padded_request(size_t total_len) {
  std::string head = "GET_PARAMETER rtsp://host/ RTSP/1.0\r\nCSeq: 9\r\nX-Pad: ";
  std::string tail = "\r\n\r\n";
  return head + std::string(total_len - head.size() - tail.size(), 'a') + tail;
}

TEST_F(RTSPSessionTest, RequestOfBufferSizeMinusOneIsHandled) {
  this->send(padded_request(REQUEST_BUF_SIZE - 1));
  this->loops(1);
  EXPECT_NE(this->receive().find("CSeq: 9\r\n"), std::string::npos);
  EXPECT_TRUE(this->session_.is_active());
}

TEST_F(RTSPSessionTest, RequestFillingTheBufferIsHandled) {
  this->send(padded_request(REQUEST_BUF_SIZE));
  this->loops(1);
  EXPECT_NE(this->receive().find("CSeq: 9\r\n"), std::string::npos);
  EXPECT_TRUE(this->session_.is_active());
}

TEST_F(RTSPSessionTest, OversizedRequestClosesSession) {
  this->send(std::string(REQUEST_BUF_SIZE + 1, 'a'));
  this->loops(1);
  EXPECT_FALSE(this->session_.is_active());
}

// Parses '$' channel length packets after the RTSP responses; false if the stream is not well framed.
static bool parse_interleaved(const std::string &stream, size_t start, std::vector<uint16_t> &sequences) {
  size_t pos = start;
  while (pos < stream.size()) {
    if (pos + 4 > stream.size() || stream[pos] != '$')
      return false;
    size_t len = (static_cast<uint8_t>(stream[pos + 2]) << 8) | static_cast<uint8_t>(stream[pos + 3]);
    if (pos + 4 + len > stream.size() || len < 12)
      return false;
    sequences.push_back((static_cast<uint8_t>(stream[pos + 6]) << 8) | static_cast<uint8_t>(stream[pos + 7]));
    pos += 4 + len;
  }
  return true;
}

static JpegInfo stub_jpeg(StubImage &image) {
  JpegInfo jpeg;
  jpeg.qtable0 = image.get_data_buffer();
  jpeg.qtable1 = image.get_data_buffer() + QTABLE_LEN;
  jpeg.scan_data = image.get_data_buffer() + 2 * QTABLE_LEN;
  jpeg.scan_len = image.get_data_length() - 2 * QTABLE_LEN;
  jpeg.width = 640;
  jpeg.height = 480;
  return jpeg;
}

TEST_F(RTSPSessionTest, StalledWriterDropsFrame) {
  this->reopen_over_tcp();
  this->start_playing();
  auto image = std::make_shared<StubImage>(8 * 1024 * 1024);
  this->session_.on_new_frame(image, stub_jpeg(*image), this->now_);
  this->loops(100);  // the peer does not read, so the socket buffers fill up
  ASSERT_TRUE(this->session_.has_frame());
  EXPECT_GT(image.use_count(), 1);

  this->now_ += FRAME_STALL_MS;
  this->loops(1);
  EXPECT_TRUE(this->session_.has_frame()) << "dropped before the stall limit";

  this->now_ += 1;
  this->loops(1);
  EXPECT_FALSE(this->session_.has_frame());
  EXPECT_EQ(image.use_count(), 1);
  EXPECT_TRUE(this->session_.is_active());

  // Until the socket drains, new frames are refused so the camera can keep capturing for other consumers.
  for (int i = 0; i < 5; i++) {
    this->now_ += 100;
    auto next = std::make_shared<StubImage>(2 * QTABLE_LEN + 100);
    this->session_.on_new_frame(next, stub_jpeg(*next), this->now_);
    this->loops(1);
    EXPECT_FALSE(this->session_.has_frame());
    EXPECT_EQ(next.use_count(), 1);
  }

  // The packet in flight is completed with zeros, so the interleaved stream stays well framed.
  std::string stream;
  for (int i = 0; i < 100; i++) {
    stream += this->receive();
    this->loops(1);
  }
  stream += this->receive();
  std::vector<uint16_t> sequences;
  ASSERT_TRUE(parse_interleaved(stream, 0, sequences));
  ASSERT_FALSE(sequences.empty());

  // Once drained, the next frame is taken and follows on in sequence.
  auto next = std::make_shared<StubImage>(2 * QTABLE_LEN + 100);
  this->session_.on_new_frame(next, stub_jpeg(*next), this->now_);
  EXPECT_TRUE(this->session_.has_frame());
  this->loops(2);
  std::vector<uint16_t> after;
  ASSERT_TRUE(parse_interleaved(this->receive(), 0, after));
  ASSERT_EQ(after.size(), 1u);
  EXPECT_EQ(after[0], static_cast<uint16_t>(sequences.back() + 1));
  EXPECT_EQ(next.use_count(), 1);
}

TEST_F(RTSPSessionTest, RepeatPlayContinuesSequence) {
  this->start_playing();
  auto image = std::make_shared<StubImage>(2 * QTABLE_LEN + 100);
  this->session_.on_new_frame(image, stub_jpeg(*image), this->now_);
  this->loops(1);
  std::vector<uint16_t> sequences;
  ASSERT_TRUE(parse_interleaved(this->receive(), 0, sequences));
  ASSERT_EQ(sequences.size(), 1u);

  this->send(request("PLAY", 3));
  this->loops(1);
  std::string reply = this->receive();
  ASSERT_NE(reply.find("CSeq: 3\r\n"), std::string::npos);
  this->now_ += 100;
  this->session_.on_new_frame(image, stub_jpeg(*image), this->now_);
  this->loops(1);
  std::vector<uint16_t> after;
  ASSERT_TRUE(parse_interleaved(this->receive(), 0, after));
  ASSERT_EQ(after.size(), 1u);
  EXPECT_EQ(after[0], static_cast<uint16_t>(sequences[0] + 1));
}

struct InterleavedPacket {
  uint8_t channel;
  std::string data;
};

// Splits '$' channel length packets; false if the stream is not well framed.
static bool split_interleaved(const std::string &stream, std::vector<InterleavedPacket> &packets) {
  size_t pos = 0;
  while (pos < stream.size()) {
    if (pos + 4 > stream.size() || stream[pos] != '$')
      return false;
    size_t len = (static_cast<uint8_t>(stream[pos + 2]) << 8) | static_cast<uint8_t>(stream[pos + 3]);
    if (pos + 4 + len > stream.size())
      return false;
    packets.push_back({static_cast<uint8_t>(stream[pos + 1]), stream.substr(pos + 4, len)});
    pos += 4 + len;
  }
  return true;
}

// Only the packets on one interleaved channel.
static std::vector<InterleavedPacket> on_channel(const std::vector<InterleavedPacket> &packets, uint8_t channel) {
  std::vector<InterleavedPacket> out;
  for (const auto &packet : packets) {
    if (packet.channel == channel)
      out.push_back(packet);
  }
  return out;
}

static uint32_t be32(const std::string &data, size_t pos) {
  return (static_cast<uint32_t>(static_cast<uint8_t>(data[pos])) << 24) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[pos + 1])) << 16) |
         (static_cast<uint32_t>(static_cast<uint8_t>(data[pos + 2])) << 8) | static_cast<uint8_t>(data[pos + 3]);
}
static uint16_t be16(const std::string &data, size_t pos) {
  return (static_cast<uint8_t>(data[pos]) << 8) | static_cast<uint8_t>(data[pos + 1]);
}

class RTSPAudioSessionTest : public RTSPSessionTest {
 protected:
  static constexpr uint16_t PAYLOAD_LEN = 320;  // 20 ms of 8 kHz L16
  static constexpr uint16_t SAMPLES = 160;

  void SetUp() override {
    RTSPSessionTest::SetUp();
    this->slots_.assign(AUDIO_SLOTS * PAYLOAD_LEN, 0);
    this->track_.slots = this->slots_.data();
    this->track_.sdp_len =
        static_cast<uint16_t>(format_audio_sdp(this->sdp_, sizeof(this->sdp_), AudioCodec::AUDIO_CODEC_L16, 96, 8000));
    this->track_.sdp = this->sdp_;
    this->track_.payload_len = PAYLOAD_LEN;
    this->track_.samples_per_packet = SAMPLES;
    this->track_.payload_type = 96;
    this->session_.set_audio_track(&this->track_);
  }
  // Writes one packet the way the server does: each byte of its payload is the packet index.
  void produce() {
    for (auto &session : {&this->session_})
      session->on_audio_slot_reuse(this->track_.produced - AUDIO_SLOTS);
    uint8_t *slot = this->slots_.data() + (this->track_.produced % AUDIO_SLOTS) * PAYLOAD_LEN;
    std::memset(slot, static_cast<uint8_t>(this->track_.produced), PAYLOAD_LEN);
    this->track_.capture_ms[this->track_.produced % AUDIO_SLOTS] = this->now_;
    this->track_.produced++;
  }
  static std::string setup(const char *track, int cseq, const char *channels) {
    char line[160];
    snprintf(line, sizeof(line),
             "SETUP rtsp://host/%s RTSP/1.0\r\nCSeq: %d\r\nTransport: RTP/AVP/TCP;unicast;interleaved=%s\r\n\r\n",
             track, cseq, channels);
    return line;
  }
  void play(bool video, bool audio) {
    int cseq = 1;
    if (video)
      this->send(setup("trackID=0", cseq++, "0-1"));
    this->loops(2);
    if (audio)
      this->send(setup("trackID=1", cseq++, "2-3"));
    this->loops(2);
    this->send(request("PLAY", cseq));
    this->loops(2);
    std::string out = this->receive();
    ASSERT_EQ(count(out, "RTSP/1.0 200 OK\r\n"), static_cast<size_t>(video + audio + 1)) << out;
    ASSERT_TRUE(this->session_.is_playing());
  }

  std::vector<uint8_t> slots_;
  AudioTrack track_;
  char sdp_[256];
};

TEST_F(RTSPAudioSessionTest, DescribeOffersAudioTrack) {
  this->send(request("DESCRIBE", 1));
  this->loops(1);
  std::string out = this->receive();
  EXPECT_NE(out.find("m=video 0 RTP/AVP 26\r\na=control:trackID=0\r\n"), std::string::npos) << out;
  EXPECT_NE(out.find("m=audio 0 RTP/AVP 96\r\na=rtpmap:96 L16/8000/1\r\na=control:trackID=1\r\n"), std::string::npos)
      << out;
  char length[32];
  snprintf(length, sizeof(length), "Content-Length: %u\r\n", this->track_.sdp_len);
  EXPECT_NE(out.find(length), std::string::npos) << out;
}

TEST_F(RTSPSessionTest, AudioSetupWithoutTrackIsRefused) {
  this->send("SETUP rtsp://host/trackID=1 RTSP/1.0\r\nCSeq: 4\r\nTransport: RTP/AVP/TCP;interleaved=2-3\r\n\r\n");
  this->loops(1);
  EXPECT_EQ(this->receive().rfind("RTSP/1.0 404 Not Found\r\nCSeq: 4\r\n", 0), 0u);
}

TEST_F(RTSPAudioSessionTest, SetupAnswersEachTrackOnItsChannels) {
  this->send(setup("trackID=0", 1, "0-1"));
  this->loops(1);
  EXPECT_NE(this->receive().find("interleaved=0-1\r\n"), std::string::npos);
  this->send(setup("trackID=1", 2, "0-1"));  // a client that asks for taken channels gets 2-3
  this->loops(1);
  EXPECT_NE(this->receive().find("interleaved=2-3\r\n"), std::string::npos);
}

TEST_F(RTSPAudioSessionTest, AudioPacketsCarryTheTrack) {
  this->play(false, true);
  ASSERT_TRUE(this->session_.is_playing_audio());
  EXPECT_FALSE(this->session_.is_playing_video());
  for (int i = 0; i < 3; i++)
    this->produce();
  this->loops(2);
  std::vector<InterleavedPacket> all;
  ASSERT_TRUE(split_interleaved(this->receive(), all));
  const auto packets = on_channel(all, 2);  // an audio only session also sends its report
  ASSERT_EQ(packets.size(), 3u);
  for (size_t i = 0; i < packets.size(); i++) {
    const std::string &rtp = packets[i].data;
    EXPECT_EQ(packets[i].channel, 2);
    ASSERT_EQ(rtp.size(), RTP_HEADER_LEN + PAYLOAD_LEN);
    EXPECT_EQ(static_cast<uint8_t>(rtp[0]), 0x80);
    EXPECT_EQ(static_cast<uint8_t>(rtp[1]), 96);  // no marker
    if (i > 0) {
      EXPECT_EQ(be16(rtp, 2), static_cast<uint16_t>(be16(packets[i - 1].data, 2) + 1));
      EXPECT_EQ(be32(rtp, 4), be32(packets[i - 1].data, 4) + SAMPLES);
      EXPECT_EQ(be32(rtp, 8), be32(packets[0].data, 8));
    }
    EXPECT_EQ(rtp.find_first_not_of(static_cast<char>(i), RTP_HEADER_LEN), std::string::npos);
  }

  // Audio only: frames are not taken.
  auto image = std::make_shared<StubImage>(2 * QTABLE_LEN + 100);
  this->session_.on_new_frame(image, stub_jpeg(*image), this->now_);
  EXPECT_FALSE(this->session_.has_frame());
}

TEST_F(RTSPAudioSessionTest, PlayStartsWithFreshAudio) {
  this->produce();
  this->produce();
  this->play(false, true);
  this->loops(2);
  EXPECT_TRUE(this->receive().empty());
  this->produce();
  this->loops(1);
  std::vector<InterleavedPacket> all;
  ASSERT_TRUE(split_interleaved(this->receive(), all));
  const auto packets = on_channel(all, 2);  // an audio only session also sends its report
  ASSERT_EQ(packets.size(), 1u);
  EXPECT_EQ(static_cast<uint8_t>(packets[0].data[RTP_HEADER_LEN]), 2);
}

TEST_F(RTSPAudioSessionTest, AudioAndVideoShareTheConnection) {
  this->play(true, true);
  auto image = std::make_shared<StubImage>(2 * QTABLE_LEN + 5000);  // several video packets
  this->session_.on_new_frame(image, stub_jpeg(*image), this->now_);
  this->produce();
  this->loops(3);
  // A frame arriving while an audio packet is pending is taken.
  this->produce();
  auto next = std::make_shared<StubImage>(2 * QTABLE_LEN + 100);
  this->now_ += 100;
  this->session_.on_new_frame(next, stub_jpeg(*next), this->now_);
  this->loops(3);

  std::vector<InterleavedPacket> packets;
  ASSERT_TRUE(split_interleaved(this->receive(), packets));
  size_t video = 0;
  size_t audio = 0;
  for (const auto &packet : packets) {
    if (packet.channel == 0)
      video++;
    if (packet.channel == 2)
      audio++;
  }
  EXPECT_GE(video, 6u);
  EXPECT_EQ(audio, 2u);
  EXPECT_EQ(image.use_count(), 1);
  EXPECT_EQ(next.use_count(), 1);
}

TEST_F(RTSPAudioSessionTest, SenderReportsPairTheTracks) {
  this->play(true, true);
  this->produce();  // captured at 1000 ms
  this->loops(1);
  std::vector<InterleavedPacket> packets;
  ASSERT_TRUE(split_interleaved(this->receive(), packets));
  ASSERT_EQ(packets.size(), 1u) << "no report before the first video packet";
  const uint32_t audio_timestamp = be32(packets[0].data, 4);

  this->now_ += 40;
  auto image = std::make_shared<StubImage>(2 * QTABLE_LEN + 100);
  this->session_.on_new_frame(image, stub_jpeg(*image), this->now_);
  this->loops(1);
  packets.clear();
  ASSERT_TRUE(split_interleaved(this->receive(), packets));
  ASSERT_EQ(packets.size(), 3u);
  EXPECT_EQ(packets[0].channel, 0);
  const uint32_t video_timestamp = be32(packets[0].data, 4);
  const InterleavedPacket &video = packets[1];
  const InterleavedPacket &audio = packets[2];
  EXPECT_EQ(video.channel, 1);
  EXPECT_EQ(audio.channel, 3);
  for (const auto *report : {&video, &audio}) {
    ASSERT_EQ(report->data.size(), RTCP_SR_LEN);
    EXPECT_EQ(static_cast<uint8_t>(report->data[0]), 0x80);
    EXPECT_EQ(static_cast<uint8_t>(report->data[1]), 200);
    EXPECT_EQ(be16(report->data, 2), 6);
  }
  // Each report maps the track's last timestamp to the time it was captured, on one clock.
  EXPECT_EQ(be32(video.data, 8), 1u);
  EXPECT_EQ(be32(video.data, 12), static_cast<uint32_t>((40ull << 32) / 1000));
  EXPECT_EQ(be32(video.data, 16), video_timestamp);
  EXPECT_EQ(be32(video.data, 20), 1u);
  EXPECT_EQ(be32(video.data, 24), 100u + JPEG_HEADER_LEN + QUANT_HEADER_LEN + 2 * QTABLE_LEN);
  EXPECT_EQ(be32(audio.data, 8), 1u);
  EXPECT_EQ(be32(audio.data, 12), 0u);
  EXPECT_EQ(be32(audio.data, 16), audio_timestamp);
  EXPECT_EQ(be32(audio.data, 20), 1u);
  EXPECT_EQ(be32(audio.data, 24), PAYLOAD_LEN);

  // The next reports wait for the interval, then go ahead of the audio.
  this->now_ += RTCP_INTERVAL_MS - 1;
  this->produce();
  this->loops(1);
  packets.clear();
  ASSERT_TRUE(split_interleaved(this->receive(), packets));
  ASSERT_EQ(packets.size(), 1u);
  this->now_ += 1;
  this->produce();
  this->loops(1);
  packets.clear();
  ASSERT_TRUE(split_interleaved(this->receive(), packets));
  ASSERT_EQ(packets.size(), 3u);
  EXPECT_EQ(packets[0].channel, 1);
  EXPECT_EQ(packets[1].channel, 3);
  EXPECT_EQ(packets[2].channel, 2);
}

TEST_F(RTSPAudioSessionTest, LaggingSessionSkipsOldAudio) {
  this->play(false, true);
  for (size_t i = 0; i < AUDIO_SLOTS + 3; i++)
    this->produce();
  this->loops(2);
  std::vector<InterleavedPacket> all;
  ASSERT_TRUE(split_interleaved(this->receive(), all));
  const auto packets = on_channel(all, 2);  // an audio only session also sends its report
  ASSERT_EQ(packets.size(), AUDIO_SLOTS);
  EXPECT_EQ(static_cast<uint8_t>(packets[0].data[RTP_HEADER_LEN]), 3);
  // Sequence numbers count what was sent, timestamps count samples, so the gap shows in the timestamps.
  EXPECT_EQ(be16(packets[1].data, 2), static_cast<uint16_t>(be16(packets[0].data, 2) + 1));
  EXPECT_EQ(be32(packets[1].data, 4), be32(packets[0].data, 4) + SAMPLES);
}

TEST_F(RTSPAudioSessionTest, ReusedSlotInFlightIsPaddedWithZeros) {
  this->reopen_over_tcp();
  this->session_.set_audio_track(&this->track_);
  this->play(false, true);
  // The peer does not read, so the socket backs up part way through an audio packet.
  for (int i = 0; i < 1000 && !this->session_.audio_in_flight(); i++) {
    this->produce();
    this->loops(1);
  }
  ASSERT_TRUE(this->session_.audio_in_flight());
  const auto in_flight = static_cast<uint8_t>(this->session_.audio_in_flight_index());

  // Replacing every slot cuts the packet loose from its slot; the rest of it is owed as zeros.
  for (size_t i = 0; i < AUDIO_SLOTS; i++)
    this->produce();
  EXPECT_FALSE(this->session_.audio_in_flight());
  EXPECT_GT(this->session_.filler_len(), 0u);

  std::string stream;
  for (int i = 0; i < 300; i++) {
    stream += this->receive();
    this->loops(1);
  }
  stream += this->receive();
  std::vector<InterleavedPacket> all;
  ASSERT_TRUE(split_interleaved(stream, all));
  const auto packets = on_channel(all, 2);
  bool padded = false;
  for (const auto &packet : packets) {
    ASSERT_EQ(packet.data.size(), RTP_HEADER_LEN + PAYLOAD_LEN);
    const size_t data_end = packet.data.find_first_not_of(static_cast<char>(in_flight), RTP_HEADER_LEN);
    if (data_end != std::string::npos && packet.data.find_first_not_of('\0', data_end) == std::string::npos &&
        packet.data.back() == '\0')
      padded = true;
  }
  EXPECT_TRUE(padded);
  // The session goes on with the newest audio.
  EXPECT_EQ(static_cast<uint8_t>(packets.back().data[RTP_HEADER_LEN]), static_cast<uint8_t>(this->track_.produced - 1));
}

// ITU-T G.711 decoders, to check the encoders against.
static int16_t decode_ulaw(uint8_t code) {
  code = ~code;
  int32_t value = ((code & 0x0F) << 3) + 0x84;
  value <<= (code & 0x70) >> 4;
  return static_cast<int16_t>((code & 0x80) ? 0x84 - value : value - 0x84);
}
static int16_t decode_alaw(uint8_t code) {
  code ^= 0x55;
  int32_t value = (code & 0x0F) << 4;
  const int segment = (code & 0x70) >> 4;
  if (segment == 0) {
    value += 8;
  } else {
    value = (value + 0x108) << (segment - 1);
  }
  return static_cast<int16_t>((code & 0x80) ? value : -value);
}

TEST(RTSPG711, KnownCodes) {
  EXPECT_EQ(encode_ulaw(0), 0xFF);
  EXPECT_EQ(encode_ulaw(32767), 0x80);
  EXPECT_EQ(encode_ulaw(-32768), 0x00);
  EXPECT_EQ(encode_alaw(0), 0xD5);
  EXPECT_EQ(encode_alaw(-1), 0x55);
  EXPECT_EQ(encode_alaw(32767), 0xAA);
  EXPECT_EQ(encode_alaw(-32768), 0x2A);
}

TEST(RTSPG711, RoundTripStaysWithinTheStep) {
  for (int32_t sample = -32768; sample <= 32767; sample++) {
    const auto pcm = static_cast<int16_t>(sample);
    // The step doubles with each segment; within a segment it is at most 1/16 of the value.
    const int32_t limit = std::abs(sample) / 16 + 16;
    const int32_t clip = std::abs(sample) > 32124 ? 1024 : 0;  // the top of the largest segment is clipped
    ASSERT_LE(std::abs(decode_ulaw(encode_ulaw(pcm)) - sample), limit + clip) << sample;
    ASSERT_LE(std::abs(decode_alaw(encode_alaw(pcm)) - sample), limit + clip) << sample;
  }
}

// Minimal JPEG: SOI, DQT (two tables), optional DRI, SOF0, SOS, scan byte, EOI.
static std::vector<uint8_t> make_jpeg(uint8_t components, uint8_t y_sampling, bool dri) {
  std::vector<uint8_t> j = {0xFF, 0xD8, 0xFF, 0xDB, 0x00, 2 + 2 * 65};
  for (uint8_t id = 0; id < 2; id++) {
    j.push_back(id);
    j.insert(j.end(), 64, 1);
  }
  if (dri)
    j.insert(j.end(), {0xFF, 0xDD, 0x00, 0x04, 0x00, 0x10});
  j.insert(j.end(),
           {0xFF, 0xC0, 0x00, static_cast<uint8_t>(8 + 3 * components), 8, 0x01, 0xE0, 0x02, 0x80, components});
  for (uint8_t c = 0; c < components; c++)
    j.insert(j.end(), {static_cast<uint8_t>(c + 1), c == 0 ? y_sampling : static_cast<uint8_t>(0x11),
                       static_cast<uint8_t>(c == 0 ? 0 : 1)});
  j.insert(j.end(), {0xFF, 0xDA, 0x00, 0x02, 0x12, 0xFF, 0xD9});
  return j;
}

TEST(RTSPParseJpeg, AcceptsBaselineColor) {
  JpegInfo info;
  auto j420 = make_jpeg(3, 0x22, false);
  ASSERT_TRUE(parse_jpeg_info(j420.data(), j420.size(), info));
  EXPECT_EQ(info.type, 1);
  EXPECT_EQ(info.width, 640);
  EXPECT_EQ(info.height, 480);
  EXPECT_EQ(info.scan_len, 1u);
  JpegInfo info422;
  auto j422 = make_jpeg(3, 0x21, false);
  ASSERT_TRUE(parse_jpeg_info(j422.data(), j422.size(), info422));
  EXPECT_EQ(info422.type, 0);
}

TEST(RTSPParseJpeg, RejectsUnsupportedFrames) {
  JpegInfo info;
  auto gray = make_jpeg(1, 0x11, false);
  EXPECT_FALSE(parse_jpeg_info(gray.data(), gray.size(), info));
  auto j444 = make_jpeg(3, 0x11, false);
  EXPECT_FALSE(parse_jpeg_info(j444.data(), j444.size(), info));
  auto restart = make_jpeg(3, 0x22, true);
  EXPECT_FALSE(parse_jpeg_info(restart.data(), restart.size(), info));
}

}  // namespace esphome::rtsp_server::testing

#endif
