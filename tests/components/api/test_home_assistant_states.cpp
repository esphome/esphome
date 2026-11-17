#ifdef USE_HOST
#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/api_wizard.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/socket/socket.h"

namespace esphome::api::testing {

// What the device sent to subscribe to one state
struct SentSubscription {
  std::string entity_id;
  std::string attribute;
  bool once{false};
  bool operator==(const SentSubscription &other) const = default;
};

std::ostream &operator<<(std::ostream &os, const SentSubscription &sub) {
  return os << "{" << sub.entity_id << ", '" << sub.attribute << "', once=" << sub.once << "}";
}

static bool read_varint(const uint8_t *&pos, const uint8_t *end, uint32_t &value) {
  value = 0;
  for (uint8_t shift = 0; pos < end && shift < 35; shift += 7) {
    uint8_t byte = *pos++;
    value |= static_cast<uint32_t>(byte & 0x7F) << shift;
    if ((byte & 0x80) == 0)
      return true;
  }
  return false;
}

static SentSubscription decode_subscription(const uint8_t *pos, const uint8_t *end) {
  SentSubscription sub;
  while (pos < end) {
    uint32_t tag, value;
    if (!read_varint(pos, end, tag) || !read_varint(pos, end, value)) {
      ADD_FAILURE() << "Truncated field";
      break;
    }
    if ((tag & 0x7) == 2) {
      if (value > static_cast<size_t>(end - pos)) {
        ADD_FAILURE() << "Field length " << value << " runs past the payload";
        break;
      }
      std::string text(reinterpret_cast<const char *>(pos), value);
      pos += value;
      if (tag >> 3 == 1) {
        sub.entity_id = text;
      } else if (tag >> 3 == 2) {
        sub.attribute = text;
      }
    } else if (tag >> 3 == 3) {
      sub.once = value != 0;
    }
  }
  return sub;
}

class HomeAssistantStatesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // APIServer() makes itself the global_api_server that CustomAPIDevice uses
    this->server_ = std::make_unique<APIServer>();
    this->connect_();
  }

  void TearDown() override {
    this->connection_.reset();
    if (this->peer_fd_ >= 0)
      ::close(this->peer_fd_);
    this->server_.reset();
    global_api_server = nullptr;
  }

  // A plaintext connection over loopback TCP; the test reads what it sends from peer_fd_
  void connect_() {
    int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listen_fd, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t len = sizeof(addr);
    ASSERT_EQ(::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), len), 0);
    ASSERT_EQ(::listen(listen_fd, 1), 0);
    ASSERT_EQ(::getsockname(listen_fd, reinterpret_cast<sockaddr *>(&addr), &len), 0);
    this->peer_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(this->peer_fd_, 0);
    ASSERT_EQ(::connect(this->peer_fd_, reinterpret_cast<sockaddr *>(&addr), len), 0);
    int device_fd = ::accept(listen_fd, nullptr, nullptr);
    ::close(listen_fd);
    ASSERT_GE(device_fd, 0);
    this->connection_ =
        std::make_unique<APIConnection>(std::make_unique<socket::Socket>(device_fd), this->server_.get());
    this->connection_->start();
  }

  // Ask for the subscriptions as Home Assistant does, then let the connection send them, one per loop
  std::vector<SentSubscription> request_subscriptions_() {
    this->connection_->on_subscribe_home_assistant_states_request();
    for (int i = 0; i < 64; i++)
      this->connection_->loop();
    return this->read_sent_();
  }

  std::vector<SentSubscription> read_sent_() {
    std::vector<uint8_t> data;
    uint8_t chunk[512];
    // Loopback delivery is not instant on every OS; stop once nothing more arrives for a moment
    pollfd pfd{this->peer_fd_, POLLIN, 0};
    while (::poll(&pfd, 1, 50) > 0) {
      ssize_t got = ::recv(this->peer_fd_, chunk, sizeof(chunk), MSG_DONTWAIT);
      if (got <= 0)
        break;
      data.insert(data.end(), chunk, chunk + got);
    }
    std::vector<SentSubscription> sent;
    const uint8_t *pos = data.data();
    const uint8_t *end = pos + data.size();
    while (pos < end) {
      // Plaintext frame: a zero byte, the payload size and the message type as varints, then the payload
      uint32_t size, type;
      if (*pos++ != 0 || !read_varint(pos, end, size) || !read_varint(pos, end, type) ||
          size > static_cast<size_t>(end - pos)) {
        ADD_FAILURE() << "Malformed frame";
        break;
      }
      if (type == SubscribeHomeAssistantStateResponse::MESSAGE_TYPE)
        sent.push_back(decode_subscription(pos, pos + size));
      pos += size;
    }
    return sent;
  }

  // Deliver a state from Home Assistant; the connection writes a terminator after a non-empty state, as in rx_buf_
  void deliver_(const char *entity_id, const char *attribute, const char *state) {
    size_t len = strlen(state);
    std::string buffer = std::string(state) + "#";
    HomeAssistantStateResponse msg;
    msg.entity_id = StringRef(entity_id);
    msg.attribute = StringRef(attribute);
    msg.state = StringRef(buffer.data(), len);
    this->connection_->on_home_assistant_state_response(msg);
    if (len > 0)
      EXPECT_EQ(buffer[len], '\0');
  }

  std::unique_ptr<APIServer> server_;
  std::unique_ptr<APIConnection> connection_;
  int peer_fd_{-1};
};

TEST_F(HomeAssistantStatesTest, NothingSubscribedSendsNothing) { EXPECT_TRUE(this->request_subscriptions_().empty()); }

TEST_F(HomeAssistantStatesTest, SubscriptionsAreSentInRegistrationOrder) {
  auto noop = [](StringRef) {};
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr, noop);
  this->server_->get_home_assistant_state("number.b", "min", noop);
  this->server_->subscribe_home_assistant_state("light.c", "brightness", noop);
  this->server_->get_home_assistant_state("number.b", "max", noop);
  this->server_->subscribe_home_assistant_state("switch.d", nullptr, noop);

  EXPECT_EQ(this->request_subscriptions_(), (std::vector<SentSubscription>{
                                                {"sensor.a", "", false},
                                                {"number.b", "min", true},
                                                {"light.c", "brightness", false},
                                                {"number.b", "max", true},
                                                {"switch.d", "", false},
                                            }));
}

TEST_F(HomeAssistantStatesTest, ANewRequestSendsEverySubscriptionAgain) {
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr, [](StringRef) {});
  this->server_->subscribe_home_assistant_state("sensor.b", "unit", [](StringRef) {});
  ASSERT_EQ(this->request_subscriptions_().size(), 2u);
  EXPECT_TRUE(this->read_sent_().empty());
  EXPECT_EQ(this->request_subscriptions_().size(), 2u);
}

TEST_F(HomeAssistantStatesTest, StateWithoutAttributeReachesOnlyThePlainSubscription) {
  std::vector<std::string> plain, attribute;
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr,
                                                [&](StringRef state) { plain.push_back(state.str()); });
  this->server_->subscribe_home_assistant_state("sensor.a", "unit",
                                                [&](StringRef state) { attribute.push_back(state.str()); });

  this->deliver_("sensor.a", "", "21.5");
  EXPECT_EQ(plain, (std::vector<std::string>{"21.5"}));
  EXPECT_TRUE(attribute.empty());
}

TEST_F(HomeAssistantStatesTest, AttributeReachesOnlyTheMatchingSubscription) {
  std::vector<std::string> plain, unit, other;
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr,
                                                [&](StringRef state) { plain.push_back(state.str()); });
  this->server_->subscribe_home_assistant_state("sensor.a", "unit",
                                                [&](StringRef state) { unit.push_back(state.str()); });
  this->server_->subscribe_home_assistant_state("sensor.a", "other",
                                                [&](StringRef state) { other.push_back(state.str()); });

  this->deliver_("sensor.a", "unit", "C");
  EXPECT_TRUE(plain.empty());
  EXPECT_EQ(unit, (std::vector<std::string>{"C"}));
  EXPECT_TRUE(other.empty());
}

TEST_F(HomeAssistantStatesTest, OtherEntitiesAreIgnored) {
  int calls = 0;
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr, [&](StringRef) { calls++; });
  this->deliver_("sensor.ab", "", "1");
  this->deliver_("sensor.", "", "1");
  this->deliver_("binary_sensor.a", "", "1");
  EXPECT_EQ(calls, 0);
}

TEST_F(HomeAssistantStatesTest, EverySubscriberOfAStateIsCalledInOrder) {
  std::vector<int> order;
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr, [&](StringRef) { order.push_back(1); });
  this->server_->subscribe_home_assistant_state("sensor.b", nullptr, [&](StringRef) { order.push_back(99); });
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr, [&](StringRef) { order.push_back(2); });
  this->deliver_("sensor.a", "", "on");
  EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

TEST_F(HomeAssistantStatesTest, StateIsTerminatedForTheCallback) {
  std::string seen;
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr, [&](StringRef state) { seen = state.c_str(); });
  this->deliver_("sensor.a", "", "12.5");
  EXPECT_EQ(seen, "12.5");
}

TEST_F(HomeAssistantStatesTest, EmptyStateIsDelivered) {
  std::vector<std::string> seen;
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr,
                                                [&](StringRef state) { seen.push_back(state.str()); });
  this->deliver_("sensor.a", "", "");
  EXPECT_EQ(seen, (std::vector<std::string>{""}));
}

TEST_F(HomeAssistantStatesTest, GetIsCalledForEveryMatchingState) {
  // once only asks Home Assistant to send the state a single time; the device keeps the subscription
  std::vector<std::string> seen;
  this->server_->get_home_assistant_state("number.a", "min", [&](StringRef state) { seen.push_back(state.str()); });
  this->deliver_("number.a", "min", "1");
  this->deliver_("number.a", "min", "2");
  EXPECT_EQ(seen, (std::vector<std::string>{"1", "2"}));
  EXPECT_EQ(this->request_subscriptions_(), (std::vector<SentSubscription>{{"number.a", "min", true}}));
}

TEST_F(HomeAssistantStatesTest, StdStringOverloadsCopyTheirStrings) {
  std::vector<std::string> seen;
  {
    std::string entity_id = "sensor.temporary";
    std::string attribute = "unit";
    this->server_->subscribe_home_assistant_state(
        entity_id, optional<std::string>(attribute),
        std::function<void(StringRef)>([&](StringRef state) { seen.push_back("sub:" + state.str()); }));
    this->server_->get_home_assistant_state(entity_id, nullopt, std::function<void(StringRef)>([&](StringRef state) {
                                              seen.push_back("get:" + state.str());
                                            }));
    entity_id.assign(40, 'x');
    attribute.assign(40, 'y');
  }
  this->deliver_("sensor.temporary", "unit", "C");
  this->deliver_("sensor.temporary", "", "5");
  EXPECT_EQ(seen, (std::vector<std::string>{"sub:C", "get:5"}));
  EXPECT_EQ(this->request_subscriptions_(), (std::vector<SentSubscription>{
                                                {"sensor.temporary", "unit", false},
                                                {"sensor.temporary", "", true},
                                            }));
}

TEST_F(HomeAssistantStatesTest, LegacyStdStringCallbackGetsACopy) {
  std::string seen;
  this->server_->subscribe_home_assistant_state(
      "sensor.a", nullopt, std::function<void(const std::string &)>([&](const std::string &state) { seen = state; }));
  this->deliver_("sensor.a", "", "on");
  EXPECT_EQ(seen, "on");
}

class TestDevice : public CustomAPIDevice {
 public:
  void on_state(StringRef state) { this->seen.push_back("state:" + state.str()); }
  void on_entity_state(const std::string &entity_id, StringRef state) {
    this->seen.push_back(entity_id + ":" + state.str());
  }
  void on_legacy_state(std::string state) { this->seen.push_back("legacy:" + state); }
  void on_legacy_entity_state(std::string entity_id, std::string state) {
    this->seen.push_back("legacy " + entity_id + ":" + state);
  }
  std::vector<std::string> seen;
};

TEST_F(HomeAssistantStatesTest, CustomAPIDeviceSubscriptions) {
  TestDevice device;
  device.subscribe_homeassistant_state(&TestDevice::on_state, "sensor.a");
  device.subscribe_homeassistant_state(&TestDevice::on_entity_state, std::string("sensor.b"), "unit");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  device.subscribe_homeassistant_state(&TestDevice::on_legacy_state, "sensor.c", "unit");
  device.subscribe_homeassistant_state(&TestDevice::on_legacy_entity_state, "sensor.d");
#pragma GCC diagnostic pop

  this->deliver_("sensor.a", "", "1");
  this->deliver_("sensor.b", "unit", "2");
  this->deliver_("sensor.c", "unit", "3");
  this->deliver_("sensor.d", "", "4");
  // An empty attribute is no attribute
  this->deliver_("sensor.b", "", "ignored");
  EXPECT_EQ(device.seen, (std::vector<std::string>{"state:1", "sensor.b:2", "legacy:3", "legacy sensor.d:4"}));

  EXPECT_EQ(this->request_subscriptions_(), (std::vector<SentSubscription>{
                                                {"sensor.a", "", false},
                                                {"sensor.b", "unit", false},
                                                {"sensor.c", "unit", false},
                                                {"sensor.d", "", false},
                                            }));
}

TEST_F(HomeAssistantStatesTest, InternalAndCustomAPIDeviceSubscriptionsAreAllSent) {
  TestDevice device;
  for (const char *entity_id : {"sensor.1", "sensor.2", "sensor.3", "sensor.4", "sensor.5", "sensor.6"})
    this->server_->subscribe_home_assistant_state(entity_id, nullptr, [](StringRef) {});
  device.subscribe_homeassistant_state(&TestDevice::on_state, "sensor.custom");

  auto sent = this->request_subscriptions_();
  ASSERT_EQ(sent.size(), 7u);
  EXPECT_EQ(sent.front().entity_id, "sensor.1");
  EXPECT_EQ(sent[5].entity_id, "sensor.6");
  EXPECT_EQ(sent.back().entity_id, "sensor.custom");

  this->deliver_("sensor.custom", "", "x");
  EXPECT_EQ(device.seen, (std::vector<std::string>{"state:x"}));
}

TEST_F(HomeAssistantStatesTest, CustomAPIDeviceSubscribedFirst) {
  TestDevice device;
  device.subscribe_homeassistant_state(&TestDevice::on_state, "sensor.custom");
  this->server_->subscribe_home_assistant_state("sensor.1", nullptr, [](StringRef) {});
  this->server_->subscribe_home_assistant_state("sensor.2", nullptr, [](StringRef) {});

  // Sent in registration order
  EXPECT_EQ(this->request_subscriptions_(), (std::vector<SentSubscription>{
                                                {"sensor.custom", "", false},
                                                {"sensor.1", "", false},
                                                {"sensor.2", "", false},
                                            }));
  this->deliver_("sensor.custom", "", "x");
  EXPECT_EQ(device.seen, (std::vector<std::string>{"state:x"}));
}

TEST_F(HomeAssistantStatesTest, MemberCallbacksSeeTheirObject) {
  struct Holder {
    std::string last;
    void subscribe() {
      global_api_server->subscribe_home_assistant_state("sensor.a", nullptr,
                                                        [this](StringRef state) { this->last = state.str(); });
    }
  } holder;
  holder.subscribe();
  this->deliver_("sensor.a", "", "on");
  EXPECT_EQ(holder.last, "on");
}

TEST_F(HomeAssistantStatesTest, AnEntityOfTheSameLengthIsIgnored) {
  int calls = 0;
  this->server_->subscribe_home_assistant_state("sensor.a", nullptr, [&](StringRef) { calls++; });
  this->deliver_("sensor.b", "", "1");
  this->deliver_("number.a", "", "1");
  EXPECT_EQ(calls, 0);
  this->deliver_("sensor.a", "", "1");
  EXPECT_EQ(calls, 1);
}

#ifdef USE_API_WIZARD_LINKED_INPUTS
// Set a wizard input as the client does, through the connection
static void set_wizard_input(APIConnection &connection, uint32_t key, const char *entity_id) {
  WizardInputSetRequest request;
  request.key = key;
  request.entity_id = StringRef(entity_id);
  connection.on_wizard_input_set_request(request);
}

TEST_F(HomeAssistantStatesTest, WizardEntityIdIsSentOnceItIsSet) {
  // A wizard input's entity id lives in a RAM buffer that is empty until the wizard sets it: the single input with
  // an empty buffer that test_wizard.cpp defines.
  const WizardInputEntry *end = API_WIZARD_INPUTS + API_WIZARD_INPUT_COUNT;
  const WizardInputEntry *found = std::find_if(
      API_WIZARD_INPUTS, end, [](const WizardInputEntry &e) { return e.size == 0 && e.entity_id[0] == '\0'; });
  ASSERT_NE(found, end);
  const WizardInputEntry &input = *found;
  char *buffer = input.entity_id;
  // Leave the shared buffer empty for later tests, also when an assertion returns early
  struct ClearBuffer {
    char *buffer;
    ~ClearBuffer() { this->buffer[0] = '\0'; }
  } clear{buffer};
  std::string seen;
  this->server_->subscribe_home_assistant_state(buffer, nullptr, [&](StringRef state) { seen = state.str(); });
  this->server_->subscribe_home_assistant_state("sensor.fixed", nullptr, [](StringRef) {});

  EXPECT_EQ(this->request_subscriptions_(), (std::vector<SentSubscription>{{"sensor.fixed", "", false}}));

  set_wizard_input(*this->connection_, input.key, "sensor.chosen");
  ASSERT_STREQ(buffer, "sensor.chosen");
  this->connection_->resend_state_subscriptions(buffer);
  EXPECT_EQ(this->read_sent_(), (std::vector<SentSubscription>{{"sensor.chosen", "", false}}));

  this->deliver_("sensor.chosen", "", "on");
  EXPECT_EQ(seen, "on");

  // A new id of another length replaces the old one
  set_wizard_input(*this->connection_, input.key, "light.x");
  this->deliver_("sensor.chosen", "", "stale");
  EXPECT_EQ(seen, "on");
  this->deliver_("light.x", "", "off");
  EXPECT_EQ(seen, "off");
}
#endif

}  // namespace esphome::api::testing
#endif  // USE_HOST
