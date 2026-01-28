#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

#include "esphome/core/application.h"
#include "esphome/components/mqtt/mqtt_client.h"

namespace esphome::mqtt::testing {

struct PublishRecord {
  std::string topic;
  std::string payload;
  uint8_t qos;
  bool retain;
};

struct SubscribeRecord {
  std::string topic;
  uint8_t qos;
};

class FakeMQTTBackend : public MQTTBackend {
 public:
  void set_keep_alive(uint16_t keep_alive) override { (void) keep_alive; }
  void set_client_id(const char *client_id) override { (void) client_id; }
  void set_clean_session(bool clean_session) override { (void) clean_session; }
  void set_credentials(const char *username, const char *password) override {
    (void) username;
    (void) password;
  }
  void set_will(const char *topic, uint8_t qos, bool retain, const char *payload) override {
    (void) topic;
    (void) qos;
    (void) retain;
    (void) payload;
  }
  void set_server(network::IPAddress ip, uint16_t port) override {
    (void) ip;
    (void) port;
  }
  void set_server(const char *host, uint16_t port) override {
    (void) host;
    (void) port;
  }

  void set_on_connect(std::function<on_connect_callback_t> &&callback) override {
    this->on_connect_ = std::move(callback);
  }
  void set_on_disconnect(std::function<on_disconnect_callback_t> &&callback) override {
    this->on_disconnect_ = std::move(callback);
  }
  void set_on_subscribe(std::function<on_subscribe_callback_t> &&callback) override {
    this->on_subscribe_ = std::move(callback);
  }
  void set_on_unsubscribe(std::function<on_unsubscribe_callback_t> &&callback) override {
    this->on_unsubscribe_ = std::move(callback);
  }
  void set_on_message(std::function<on_message_callback_t> &&callback) override {
    this->on_message_ = std::move(callback);
  }
  void set_on_publish(std::function<on_publish_user_callback_t> &&callback) override {
    this->on_publish_ = std::move(callback);
  }

  bool connected() const override { return this->connected_; }

  void connect() override {
    this->connected_ = true;
    if (this->on_connect_) {
      this->on_connect_(false);
    }
  }

  void disconnect() override {
    bool was_connected = this->connected_;
    this->connected_ = false;
    if (was_connected && this->on_disconnect_) {
      this->on_disconnect_(MQTTClientDisconnectReason::TCP_DISCONNECTED);
    }
  }

  bool subscribe(const char *topic, uint8_t qos) override {
    this->subscribes_.push_back({topic ? topic : "", qos});
    if (this->on_subscribe_) {
      this->on_subscribe_(0, qos);
    }
    return true;
  }

  bool unsubscribe(const char *topic) override {
    (void) topic;
    if (this->on_unsubscribe_) {
      this->on_unsubscribe_(0);
    }
    return true;
  }

  bool publish(const char *topic, const char *payload, size_t length, uint8_t qos, bool retain) override {
    this->publishes_.push_back({.topic = topic ? topic : "",
                                .payload = std::string(payload ? payload : "", payload ? length : 0),
                                .qos = qos,
                                .retain = retain});
    if (this->on_publish_) {
      this->on_publish_(0);
    }
    return true;
  }

  void inject_message(const std::string &topic, const std::string &payload) {
    ASSERT_TRUE(this->on_message_);
    this->on_message_(topic.c_str(), payload.data(), payload.size(), 0, payload.size());
  }

  std::vector<PublishRecord> publishes_;
  std::vector<SubscribeRecord> subscribes_;

 protected:
  bool connected_{false};
  std::function<on_connect_callback_t> on_connect_{};
  std::function<on_disconnect_callback_t> on_disconnect_{};
  std::function<on_subscribe_callback_t> on_subscribe_{};
  std::function<on_unsubscribe_callback_t> on_unsubscribe_{};
  std::function<on_message_callback_t> on_message_{};
  std::function<on_publish_user_callback_t> on_publish_{};
};

class TestMQTTClientComponent : public MQTTClientComponent {
 public:
  void set_backend(FakeMQTTBackend *backend) { this->set_mqtt_backend_for_testing_(backend); }

  void force_connected(FakeMQTTBackend *backend) {
    backend->connect();
    this->state_ = MQTT_CLIENT_CONNECTED;
  }

  void resubscribe_for_testing() { this->resubscribe_subscriptions_(); }
};

TEST(MQTTClientHostEmulationTest, DiscoverRequestPublishesDeviceInfo) {
  App.pre_setup("host-light", "", false);

  FakeMQTTBackend backend;
  TestMQTTClientComponent mqtt;
  mqtt.set_backend(&backend);
  mqtt.setup();
  mqtt.force_connected(&backend);

  backend.inject_message("esphome/discover", "");

  bool found = false;
  for (const auto &pub : backend.publishes_) {
    if (pub.topic == "esphome/discover/host-light") {
      found = true;
      EXPECT_NE(pub.payload.find("\"name\":\"host-light\""), std::string::npos);
      break;
    }
  }
  EXPECT_TRUE(found);
}

TEST(MQTTClientHostEmulationTest, DiscoveryIpSubscribesToDiscoverAndPingTopics) {
  App.pre_setup("host-light", "", false);

  FakeMQTTBackend backend;
  TestMQTTClientComponent mqtt;
  mqtt.set_backend(&backend);
  mqtt.setup();
  mqtt.force_connected(&backend);

  // Now that we're connected, resubscribe should issue the subscribes.
  mqtt.resubscribe_for_testing();

  bool has_discover = false;
  bool has_ping = false;
  for (const auto &sub : backend.subscribes_) {
    if (sub.topic == "esphome/discover" && sub.qos == 2)
      has_discover = true;
    if (sub.topic == "esphome/ping/host-light" && sub.qos == 2)
      has_ping = true;
  }
  EXPECT_TRUE(has_discover);
  EXPECT_TRUE(has_ping);
}

}  // namespace esphome::mqtt::testing
