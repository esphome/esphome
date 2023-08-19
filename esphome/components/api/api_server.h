#pragma once

#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/defines.h"
#include "esphome/core/log.h"
#include "esphome/components/socket/socket.h"
#include "api_pb2.h"
#include "api_pb2_service.h"
#include "list_entities.h"
#include "subscribe_state.h"
#include "user_services.h"
#include "api_noise_context.h"

#include <vector>

namespace esphome {
namespace api {

class APIServer : public Component {
 public:
  APIServer();
  void setup() override;
  uint16_t get_port() const;
  float get_setup_priority() const override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool check_password(const std::string &password) const;
  bool uses_password() const;
  void set_port(uint16_t port);
  void set_password(const std::string &password);
  void set_reboot_timeout(uint32_t reboot_timeout);

#ifdef USE_API_NOISE
  void set_noise_psk(psk_t psk) { noise_ctx_->set_psk(psk); }
  std::shared_ptr<APINoiseContext> get_noise_ctx() { return noise_ctx_; }
#endif  // USE_API_NOISE

  void handle_disconnect(APIConnection *conn);
  void send_homeassistant_service_call(const HomeassistantServiceResponse &call);
  void register_user_service(UserServiceDescriptor *descriptor) { this->user_services_.push_back(descriptor); }
#ifdef USE_HOMEASSISTANT_TIME
  void request_time();
#endif

#ifdef USE_VOICE_ASSISTANT
  bool start_voice_assistant(const std::string &conversation_id, bool use_vad);
  void stop_voice_assistant();
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) override;
#endif

  bool is_connected() const;

  struct HomeAssistantStateSubscription {
    std::string entity_id;
    optional<std::string> attribute;
    std::function<void(std::string)> callback;
  };

  void subscribe_home_assistant_state(std::string entity_id, optional<std::string> attribute,
                                      std::function<void(std::string)> f);
  const std::vector<HomeAssistantStateSubscription> &get_state_subs() const;
  const std::vector<UserServiceDescriptor *> &get_user_services() const { return this->user_services_; }

 protected:
  std::unique_ptr<socket::Socket> socket_ = nullptr;
  uint16_t port_{6053};
  uint32_t reboot_timeout_{300000};
  uint32_t last_connected_{0};
  std::vector<std::unique_ptr<APIConnection>> clients_;
  std::string password_;
  std::vector<HomeAssistantStateSubscription> state_subs_;
  std::vector<UserServiceDescriptor *> user_services_;

#ifdef USE_API_NOISE
  std::shared_ptr<APINoiseContext> noise_ctx_ = std::make_shared<APINoiseContext>();
#endif  // USE_API_NOISE
};

extern APIServer *global_api_server;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class APIConnectedCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override { return global_api_server->is_connected(); }
};

}  // namespace api
}  // namespace esphome
