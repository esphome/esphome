#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#include "api_noise_context.h"
#include "api_pb2.h"
#include "api_pb2_service.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/log.h"
#include "list_entities.h"
#include "subscribe_state.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
#ifdef USE_CAMERA
#include "esphome/components/camera/camera.h"
#endif

#include <vector>

namespace esphome::api {

#ifdef USE_API_USER_DEFINED_ACTIONS
// Forward declaration - full definition in user_services.h
class UserServiceDescriptor;
#endif

#ifdef USE_API_NOISE
struct SavedNoisePsk {
  psk_t psk;
} PACKED;  // NOLINT
#endif

class APIServer : public Component,
                  public Controller
#ifdef USE_LOGGER
    ,
                  public logger::LogListener
#endif
#ifdef USE_CAMERA
    ,
                  public camera::CameraListener
#endif
{
 public:
  APIServer();
  void setup() override;
  uint16_t get_port() const;
  float get_setup_priority() const override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  bool teardown() override;
#ifdef USE_LOGGER
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override;
#endif
#ifdef USE_CAMERA
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
#endif
#ifdef USE_API_PASSWORD
  bool check_password(const uint8_t *password_data, size_t password_len) const;
  void set_password(const std::string &password);
#endif
  void set_port(uint16_t port);
  void set_reboot_timeout(uint32_t reboot_timeout);
  void set_batch_delay(uint16_t batch_delay);
  uint16_t get_batch_delay() const { return batch_delay_; }
  void set_listen_backlog(uint8_t listen_backlog) { this->listen_backlog_ = listen_backlog; }
  void set_max_connections(uint8_t max_connections) { this->max_connections_ = max_connections; }

  // Get reference to shared buffer for API connections
  std::vector<uint8_t> &get_shared_buffer_ref() { return shared_write_buffer_; }

#ifdef USE_API_NOISE
  bool save_noise_psk(psk_t psk, bool make_active = true);
  bool clear_noise_psk(bool make_active = true);
  void set_noise_psk(psk_t psk) { this->noise_ctx_.set_psk(psk); }
  APINoiseContext &get_noise_ctx() { return this->noise_ctx_; }
#endif  // USE_API_NOISE

  void handle_disconnect(APIConnection *conn);
#ifdef USE_BINARY_SENSOR
  void on_binary_sensor_update(binary_sensor::BinarySensor *obj) override;
#endif
#ifdef USE_COVER
  void on_cover_update(cover::Cover *obj) override;
#endif
#ifdef USE_FAN
  void on_fan_update(fan::Fan *obj) override;
#endif
#ifdef USE_LIGHT
  void on_light_update(light::LightState *obj) override;
#endif
#ifdef USE_SENSOR
  void on_sensor_update(sensor::Sensor *obj) override;
#endif
#ifdef USE_SWITCH
  void on_switch_update(switch_::Switch *obj) override;
#endif
#ifdef USE_TEXT_SENSOR
  void on_text_sensor_update(text_sensor::TextSensor *obj) override;
#endif
#ifdef USE_CLIMATE
  void on_climate_update(climate::Climate *obj) override;
#endif
#ifdef USE_NUMBER
  void on_number_update(number::Number *obj) override;
#endif
#ifdef USE_DATETIME_DATE
  void on_date_update(datetime::DateEntity *obj) override;
#endif
#ifdef USE_DATETIME_TIME
  void on_time_update(datetime::TimeEntity *obj) override;
#endif
#ifdef USE_DATETIME_DATETIME
  void on_datetime_update(datetime::DateTimeEntity *obj) override;
#endif
#ifdef USE_TEXT
  void on_text_update(text::Text *obj) override;
#endif
#ifdef USE_SELECT
  void on_select_update(select::Select *obj) override;
#endif
#ifdef USE_LOCK
  void on_lock_update(lock::Lock *obj) override;
#endif
#ifdef USE_VALVE
  void on_valve_update(valve::Valve *obj) override;
#endif
#ifdef USE_MEDIA_PLAYER
  void on_media_player_update(media_player::MediaPlayer *obj) override;
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
  void send_homeassistant_action(const HomeassistantActionRequest &call);

#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  // Action response handling
  using ActionResponseCallback = std::function<void(const class ActionResponse &)>;
  void register_action_response_callback(uint32_t call_id, ActionResponseCallback callback);
  void handle_action_response(uint32_t call_id, bool success, const std::string &error_message);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  void handle_action_response(uint32_t call_id, bool success, const std::string &error_message,
                              const uint8_t *response_data, size_t response_data_len);
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES
#endif  // USE_API_HOMEASSISTANT_SERVICES
#ifdef USE_API_USER_DEFINED_ACTIONS
  void initialize_user_services(std::initializer_list<UserServiceDescriptor *> services) {
    this->user_services_.assign(services);
  }
#ifdef USE_API_CUSTOM_SERVICES
  // Only compile push_back method when custom_services: true (external components)
  void register_user_service(UserServiceDescriptor *descriptor) { this->user_services_.push_back(descriptor); }
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  // Action call context management - supports concurrent calls from multiple clients
  // Returns server-generated action_call_id to avoid collisions when clients use same call_id
  uint32_t register_active_action_call(uint32_t client_call_id, APIConnection *conn);
  void unregister_active_action_call(uint32_t action_call_id);
  void unregister_active_action_calls_for_connection(APIConnection *conn);
  // Send response for a specific action call (uses action_call_id, sends client_call_id in response)
  void send_action_response(uint32_t action_call_id, bool success, const std::string &error_message);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  void send_action_response(uint32_t action_call_id, bool success, const std::string &error_message,
                            const uint8_t *response_data, size_t response_data_len);
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES
#endif
#ifdef USE_HOMEASSISTANT_TIME
  void request_time();
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  void on_alarm_control_panel_update(alarm_control_panel::AlarmControlPanel *obj) override;
#endif
#ifdef USE_EVENT
  void on_event(event::Event *obj) override;
#endif
#ifdef USE_UPDATE
  void on_update(update::UpdateEntity *obj) override;
#endif
#ifdef USE_ZWAVE_PROXY
  void on_zwave_proxy_request(const esphome::api::ProtoMessage &msg);
#endif

  bool is_connected(bool state_subscription_only = false) const;

#ifdef USE_API_HOMEASSISTANT_STATES
  struct HomeAssistantStateSubscription {
    const char *entity_id;  // Pointer to flash (internal) or heap (external)
    const char *attribute;  // Pointer to flash or nullptr (nullptr means no attribute)
    std::function<void(std::string)> callback;
    bool once;

    // Dynamic storage for external components using std::string API (custom_api_device.h)
    // These are only allocated when using the std::string overload (nullptr for const char* overload)
    std::unique_ptr<std::string> entity_id_dynamic_storage;
    std::unique_ptr<std::string> attribute_dynamic_storage;
  };

  // New const char* overload (for internal components - zero allocation)
  void subscribe_home_assistant_state(const char *entity_id, const char *attribute, std::function<void(std::string)> f);
  void get_home_assistant_state(const char *entity_id, const char *attribute, std::function<void(std::string)> f);

  // Existing std::string overload (for custom_api_device.h - heap allocation)
  void subscribe_home_assistant_state(std::string entity_id, optional<std::string> attribute,
                                      std::function<void(std::string)> f);
  void get_home_assistant_state(std::string entity_id, optional<std::string> attribute,
                                std::function<void(std::string)> f);

  const std::vector<HomeAssistantStateSubscription> &get_state_subs() const;
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
  const std::vector<UserServiceDescriptor *> &get_user_services() const { return this->user_services_; }
#endif

#ifdef USE_API_CLIENT_CONNECTED_TRIGGER
  Trigger<std::string, std::string> *get_client_connected_trigger() const { return this->client_connected_trigger_; }
#endif
#ifdef USE_API_CLIENT_DISCONNECTED_TRIGGER
  Trigger<std::string, std::string> *get_client_disconnected_trigger() const {
    return this->client_disconnected_trigger_;
  }
#endif

 protected:
#ifdef USE_API_NOISE
  bool update_noise_psk_(const SavedNoisePsk &new_psk, const LogString *save_log_msg, const LogString *fail_log_msg,
                         const psk_t &active_psk, bool make_active);
#endif  // USE_API_NOISE
#ifdef USE_API_HOMEASSISTANT_STATES
  // Helper methods to reduce code duplication
  void add_state_subscription_(const char *entity_id, const char *attribute, std::function<void(std::string)> f,
                               bool once);
  void add_state_subscription_(std::string entity_id, optional<std::string> attribute,
                               std::function<void(std::string)> f, bool once);
#endif  // USE_API_HOMEASSISTANT_STATES
  // Pointers and pointer-like types first (4 bytes each)
  std::unique_ptr<socket::Socket> socket_ = nullptr;
#ifdef USE_API_CLIENT_CONNECTED_TRIGGER
  Trigger<std::string, std::string> *client_connected_trigger_ = new Trigger<std::string, std::string>();
#endif
#ifdef USE_API_CLIENT_DISCONNECTED_TRIGGER
  Trigger<std::string, std::string> *client_disconnected_trigger_ = new Trigger<std::string, std::string>();
#endif

  // 4-byte aligned types
  uint32_t reboot_timeout_{300000};
  uint32_t last_connected_{0};

  // Vectors and strings (12 bytes each on 32-bit)
  std::vector<std::unique_ptr<APIConnection>> clients_;
#ifdef USE_API_PASSWORD
  std::string password_;
#endif
  std::vector<uint8_t> shared_write_buffer_;  // Shared proto write buffer for all connections
#ifdef USE_API_HOMEASSISTANT_STATES
  std::vector<HomeAssistantStateSubscription> state_subs_;
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
  std::vector<UserServiceDescriptor *> user_services_;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  // Active action calls - supports concurrent calls from multiple clients
  // Uses server-generated action_call_id to avoid collisions when multiple clients use same call_id
  struct ActiveActionCall {
    uint32_t action_call_id;  // Server-generated unique ID (passed to actions)
    uint32_t client_call_id;  // Client's original call_id (used in response)
    APIConnection *connection;
  };
  std::vector<ActiveActionCall> active_action_calls_;
  uint32_t next_action_call_id_{1};  // Counter for generating unique action_call_ids
#endif                               // USE_API_USER_DEFINED_ACTION_RESPONSES
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  struct PendingActionResponse {
    uint32_t call_id;
    ActionResponseCallback callback;
  };
  std::vector<PendingActionResponse> action_response_callbacks_;
#endif

  // Group smaller types together
  uint16_t port_{6053};
  uint16_t batch_delay_{100};
  // Connection limits - these defaults will be overridden by config values
  // from cv.SplitDefault in __init__.py which sets platform-specific defaults
  uint8_t listen_backlog_{4};
  uint8_t max_connections_{8};
  bool shutting_down_ = false;
  // 7 bytes used, 1 byte padding

#ifdef USE_API_NOISE
  APINoiseContext noise_ctx_;
  ESPPreferenceObject noise_pref_;
#endif  // USE_API_NOISE
};

extern APIServer *global_api_server;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class APIConnectedCondition : public Condition<Ts...> {
  TEMPLATABLE_VALUE(bool, state_subscription_only)
 public:
  bool check(const Ts &...x) override {
    return global_api_server->is_connected(this->state_subscription_only_.value(x...));
  }
};

}  // namespace esphome::api
#endif
