#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API
#include "api_frame_helper.h"
#include "api_pb2.h"
#include "api_pb2_service.h"
#include "api_server.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"

#include <vector>
#include <functional>

namespace esphome {
namespace api {

// Keepalive timeout in milliseconds
static constexpr uint32_t KEEPALIVE_TIMEOUT_MS = 60000;

class APIConnection : public APIServerConnection {
 public:
  friend class APIServer;
  APIConnection(std::unique_ptr<socket::Socket> socket, APIServer *parent);
  virtual ~APIConnection();

  void start();
  void loop();

  bool send_list_info_done() {
    return this->schedule_message_(nullptr, &APIConnection::try_send_list_info_done,
                                   ListEntitiesDoneResponse::MESSAGE_TYPE);
  }
#ifdef USE_BINARY_SENSOR
  bool send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor);
  void send_binary_sensor_info(binary_sensor::BinarySensor *binary_sensor);
#endif
#ifdef USE_COVER
  bool send_cover_state(cover::Cover *cover);
  void send_cover_info(cover::Cover *cover);
  void cover_command(const CoverCommandRequest &msg) override;
#endif
#ifdef USE_FAN
  bool send_fan_state(fan::Fan *fan);
  void send_fan_info(fan::Fan *fan);
  void fan_command(const FanCommandRequest &msg) override;
#endif
#ifdef USE_LIGHT
  bool send_light_state(light::LightState *light);
  void send_light_info(light::LightState *light);
  void light_command(const LightCommandRequest &msg) override;
#endif
#ifdef USE_SENSOR
  bool send_sensor_state(sensor::Sensor *sensor);
  void send_sensor_info(sensor::Sensor *sensor);
#endif
#ifdef USE_SWITCH
  bool send_switch_state(switch_::Switch *a_switch);
  void send_switch_info(switch_::Switch *a_switch);
  void switch_command(const SwitchCommandRequest &msg) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool send_text_sensor_state(text_sensor::TextSensor *text_sensor);
  void send_text_sensor_info(text_sensor::TextSensor *text_sensor);
#endif
#ifdef USE_ESP32_CAMERA
  void set_camera_state(std::shared_ptr<esp32_camera::CameraImage> image);
  void send_camera_info(esp32_camera::ESP32Camera *camera);
  void camera_image(const CameraImageRequest &msg) override;
#endif
#ifdef USE_CLIMATE
  bool send_climate_state(climate::Climate *climate);
  void send_climate_info(climate::Climate *climate);
  void climate_command(const ClimateCommandRequest &msg) override;
#endif
#ifdef USE_NUMBER
  bool send_number_state(number::Number *number);
  void send_number_info(number::Number *number);
  void number_command(const NumberCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATE
  bool send_date_state(datetime::DateEntity *date);
  void send_date_info(datetime::DateEntity *date);
  void date_command(const DateCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_TIME
  bool send_time_state(datetime::TimeEntity *time);
  void send_time_info(datetime::TimeEntity *time);
  void time_command(const TimeCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATETIME
  bool send_datetime_state(datetime::DateTimeEntity *datetime);
  void send_datetime_info(datetime::DateTimeEntity *datetime);
  void datetime_command(const DateTimeCommandRequest &msg) override;
#endif
#ifdef USE_TEXT
  bool send_text_state(text::Text *text);
  void send_text_info(text::Text *text);
  void text_command(const TextCommandRequest &msg) override;
#endif
#ifdef USE_SELECT
  bool send_select_state(select::Select *select);
  void send_select_info(select::Select *select);
  void select_command(const SelectCommandRequest &msg) override;
#endif
#ifdef USE_BUTTON
  void send_button_info(button::Button *button);
  void button_command(const ButtonCommandRequest &msg) override;
#endif
#ifdef USE_LOCK
  bool send_lock_state(lock::Lock *a_lock);
  void send_lock_info(lock::Lock *a_lock);
  void lock_command(const LockCommandRequest &msg) override;
#endif
#ifdef USE_VALVE
  bool send_valve_state(valve::Valve *valve);
  void send_valve_info(valve::Valve *valve);
  void valve_command(const ValveCommandRequest &msg) override;
#endif
#ifdef USE_MEDIA_PLAYER
  bool send_media_player_state(media_player::MediaPlayer *media_player);
  void send_media_player_info(media_player::MediaPlayer *media_player);
  void media_player_command(const MediaPlayerCommandRequest &msg) override;
#endif
  bool try_send_log_message(int level, const char *tag, const char *line);
  void send_homeassistant_service_call(const HomeassistantServiceResponse &call) {
    if (!this->service_call_subscription_)
      return;
    this->send_message(call);
  }
#ifdef USE_BLUETOOTH_PROXY
  void subscribe_bluetooth_le_advertisements(const SubscribeBluetoothLEAdvertisementsRequest &msg) override;
  void unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) override;
  bool send_bluetooth_le_advertisement(const BluetoothLEAdvertisementResponse &msg);

  void bluetooth_device_request(const BluetoothDeviceRequest &msg) override;
  void bluetooth_gatt_read(const BluetoothGATTReadRequest &msg) override;
  void bluetooth_gatt_write(const BluetoothGATTWriteRequest &msg) override;
  void bluetooth_gatt_read_descriptor(const BluetoothGATTReadDescriptorRequest &msg) override;
  void bluetooth_gatt_write_descriptor(const BluetoothGATTWriteDescriptorRequest &msg) override;
  void bluetooth_gatt_get_services(const BluetoothGATTGetServicesRequest &msg) override;
  void bluetooth_gatt_notify(const BluetoothGATTNotifyRequest &msg) override;
  BluetoothConnectionsFreeResponse subscribe_bluetooth_connections_free(
      const SubscribeBluetoothConnectionsFreeRequest &msg) override;
  void bluetooth_scanner_set_mode(const BluetoothScannerSetModeRequest &msg) override;

#endif
#ifdef USE_HOMEASSISTANT_TIME
  void send_time_request() {
    GetTimeRequest req;
    this->send_message(req);
  }
#endif

#ifdef USE_VOICE_ASSISTANT
  void subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) override;
  void on_voice_assistant_response(const VoiceAssistantResponse &msg) override;
  void on_voice_assistant_event_response(const VoiceAssistantEventResponse &msg) override;
  void on_voice_assistant_audio(const VoiceAssistantAudio &msg) override;
  void on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &msg) override;
  void on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &msg) override;
  VoiceAssistantConfigurationResponse voice_assistant_get_configuration(
      const VoiceAssistantConfigurationRequest &msg) override;
  void voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) override;
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  bool send_alarm_control_panel_state(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel);
  void send_alarm_control_panel_info(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel);
  void alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) override;
#endif

#ifdef USE_EVENT
  void send_event(event::Event *event, const std::string &event_type);
  void send_event_info(event::Event *event);
#endif

#ifdef USE_UPDATE
  bool send_update_state(update::UpdateEntity *update);
  void send_update_info(update::UpdateEntity *update);
  void update_command(const UpdateCommandRequest &msg) override;
#endif

  void on_disconnect_response(const DisconnectResponse &value) override;
  void on_ping_response(const PingResponse &value) override {
    // we initiated ping
    this->ping_retries_ = 0;
    this->sent_ping_ = false;
  }
  void on_home_assistant_state_response(const HomeAssistantStateResponse &msg) override;
#ifdef USE_HOMEASSISTANT_TIME
  void on_get_time_response(const GetTimeResponse &value) override;
#endif
  HelloResponse hello(const HelloRequest &msg) override;
  ConnectResponse connect(const ConnectRequest &msg) override;
  DisconnectResponse disconnect(const DisconnectRequest &msg) override;
  PingResponse ping(const PingRequest &msg) override { return {}; }
  DeviceInfoResponse device_info(const DeviceInfoRequest &msg) override;
  void list_entities(const ListEntitiesRequest &msg) override { this->list_entities_iterator_.begin(); }
  void subscribe_states(const SubscribeStatesRequest &msg) override {
    this->state_subscription_ = true;
    this->initial_state_iterator_.begin();
  }
  void subscribe_logs(const SubscribeLogsRequest &msg) override {
    this->log_subscription_ = msg.level;
    if (msg.dump_config)
      App.schedule_dump_config();
  }
  void subscribe_homeassistant_services(const SubscribeHomeassistantServicesRequest &msg) override {
    this->service_call_subscription_ = true;
  }
  void subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) override;
  GetTimeResponse get_time(const GetTimeRequest &msg) override {
    // TODO
    return {};
  }
  void execute_service(const ExecuteServiceRequest &msg) override;
#ifdef USE_API_NOISE
  NoiseEncryptionSetKeyResponse noise_encryption_set_key(const NoiseEncryptionSetKeyRequest &msg) override;
#endif

  bool is_authenticated() override { return this->connection_state_ == ConnectionState::AUTHENTICATED; }
  bool is_connection_setup() override {
    return this->connection_state_ == ConnectionState ::CONNECTED || this->is_authenticated();
  }
  void on_fatal_error() override;
  void on_unauthenticated_access() override;
  void on_no_setup_connection() override;
  ProtoWriteBuffer create_buffer(uint32_t reserve_size) override {
    // FIXME: ensure no recursive writes can happen

    // Get header padding size - used for both reserve and insert
    uint8_t header_padding = this->helper_->frame_header_padding();

    // Get shared buffer from parent server
    std::vector<uint8_t> &shared_buf = this->parent_->get_shared_buffer_ref();
    shared_buf.clear();
    // Reserve space for header padding + message + footer
    // - Header padding: space for protocol headers (7 bytes for Noise, 6 for Plaintext)
    // - Footer: space for MAC (16 bytes for Noise, 0 for Plaintext)
    shared_buf.reserve(reserve_size + header_padding + this->helper_->frame_footer_size());
    // Resize to add header padding so message encoding starts at the correct position
    shared_buf.resize(header_padding);
    return {&shared_buf};
  }

  // Prepare buffer for next message in batch
  ProtoWriteBuffer prepare_message_buffer(uint16_t message_size, bool is_first_message) {
    // Get reference to shared buffer (it maintains state between batch messages)
    std::vector<uint8_t> &shared_buf = this->parent_->get_shared_buffer_ref();

    if (is_first_message) {
      shared_buf.clear();
    }

    size_t current_size = shared_buf.size();

    // Calculate padding to add:
    // - First message: just header padding
    // - Subsequent messages: footer for previous message + header padding for this message
    size_t padding_to_add = is_first_message
                                ? this->helper_->frame_header_padding()
                                : this->helper_->frame_header_padding() + this->helper_->frame_footer_size();

    // Reserve space for padding + message
    shared_buf.reserve(current_size + padding_to_add + message_size);

    // Resize to add the padding bytes
    shared_buf.resize(current_size + padding_to_add);

    return {&shared_buf};
  }

  bool try_to_clear_buffer(bool log_out_of_space);
  bool send_buffer(ProtoWriteBuffer buffer, uint16_t message_type) override;

  std::string get_client_combined_info() const { return this->client_combined_info_; }

  // Buffer allocator methods for batch processing
  ProtoWriteBuffer allocate_single_message_buffer(uint16_t size);
  ProtoWriteBuffer allocate_batch_message_buffer(uint16_t size);

 protected:
  // Helper function to fill common entity info fields
  static void fill_entity_info_base(esphome::EntityBase *entity, InfoResponseProtoMessage &response) {
    // Set common fields that are shared by all entity types
    response.key = entity->get_object_id_hash();
    response.object_id = entity->get_object_id();

    if (entity->has_own_name())
      response.name = entity->get_name();

    // Set common EntityBase properties
    response.icon = entity->get_icon();
    response.disabled_by_default = entity->is_disabled_by_default();
    response.entity_category = static_cast<enums::EntityCategory>(entity->get_entity_category());
  }

  // Helper function to fill common entity state fields
  static void fill_entity_state_base(esphome::EntityBase *entity, StateResponseProtoMessage &response) {
    response.key = entity->get_object_id_hash();
  }

  // Non-template helper to encode any ProtoMessage
  static uint16_t encode_message_to_buffer(ProtoMessage &msg, uint16_t message_type, APIConnection *conn,
                                           uint32_t remaining_size, bool is_single);

#ifdef USE_BINARY_SENSOR
  static uint16_t try_send_binary_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                               bool is_single);
  static uint16_t try_send_binary_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single);
#endif
#ifdef USE_COVER
  static uint16_t try_send_cover_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
  static uint16_t try_send_cover_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_FAN
  static uint16_t try_send_fan_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
  static uint16_t try_send_fan_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_LIGHT
  static uint16_t try_send_light_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
  static uint16_t try_send_light_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_SENSOR
  static uint16_t try_send_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);
  static uint16_t try_send_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif
#ifdef USE_SWITCH
  static uint16_t try_send_switch_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);
  static uint16_t try_send_switch_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif
#ifdef USE_TEXT_SENSOR
  static uint16_t try_send_text_sensor_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single);
  static uint16_t try_send_text_sensor_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                            bool is_single);
#endif
#ifdef USE_CLIMATE
  static uint16_t try_send_climate_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                         bool is_single);
  static uint16_t try_send_climate_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);
#endif
#ifdef USE_NUMBER
  static uint16_t try_send_number_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);
  static uint16_t try_send_number_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif
#ifdef USE_DATETIME_DATE
  static uint16_t try_send_date_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
  static uint16_t try_send_date_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_DATETIME_TIME
  static uint16_t try_send_time_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
  static uint16_t try_send_time_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_DATETIME_DATETIME
  static uint16_t try_send_datetime_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                          bool is_single);
  static uint16_t try_send_datetime_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                         bool is_single);
#endif
#ifdef USE_TEXT
  static uint16_t try_send_text_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
  static uint16_t try_send_text_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_SELECT
  static uint16_t try_send_select_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);
  static uint16_t try_send_select_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif
#ifdef USE_BUTTON
  static uint16_t try_send_button_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif
#ifdef USE_LOCK
  static uint16_t try_send_lock_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
  static uint16_t try_send_lock_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_VALVE
  static uint16_t try_send_valve_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
  static uint16_t try_send_valve_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_MEDIA_PLAYER
  static uint16_t try_send_media_player_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single);
  static uint16_t try_send_media_player_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single);
#endif
#ifdef USE_ALARM_CONTROL_PANEL
  static uint16_t try_send_alarm_control_panel_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                     bool is_single);
  static uint16_t try_send_alarm_control_panel_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                                    bool is_single);
#endif
#ifdef USE_EVENT
  static uint16_t try_send_event_response(event::Event *event, const std::string &event_type, APIConnection *conn,
                                          uint32_t remaining_size, bool is_single);
  static uint16_t try_send_event_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_UPDATE
  static uint16_t try_send_update_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);
  static uint16_t try_send_update_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif
#ifdef USE_ESP32_CAMERA
  static uint16_t try_send_camera_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif

  // Method for ListEntitiesDone batching
  static uint16_t try_send_list_info_done(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                          bool is_single);

  // Method for DisconnectRequest batching
  static uint16_t try_send_disconnect_request(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single);

  // Helper function to get estimated message size for buffer pre-allocation
  static uint16_t get_estimated_message_size(uint16_t message_type);

  enum class ConnectionState {
    WAITING_FOR_HELLO,
    CONNECTED,
    AUTHENTICATED,
  } connection_state_{ConnectionState::WAITING_FOR_HELLO};

  bool remove_{false};

  std::unique_ptr<APIFrameHelper> helper_;

  std::string client_info_;
  std::string client_peername_;
  std::string client_combined_info_;
  uint32_t client_api_version_major_{0};
  uint32_t client_api_version_minor_{0};
#ifdef USE_ESP32_CAMERA
  esp32_camera::CameraImageReader image_reader_;
#endif

  bool state_subscription_{false};
  int log_subscription_{ESPHOME_LOG_LEVEL_NONE};
  uint32_t last_traffic_;
  uint32_t next_ping_retry_{0};
  uint8_t ping_retries_{0};
  bool sent_ping_{false};
  bool service_call_subscription_{false};
  bool next_close_ = false;
  APIServer *parent_;
  InitialStateIterator initial_state_iterator_;
  ListEntitiesIterator list_entities_iterator_;
  int state_subs_at_ = -1;

  // Function pointer type for message encoding
  using MessageCreatorPtr = uint16_t (*)(EntityBase *, APIConnection *, uint32_t remaining_size, bool is_single);

  // Optimized MessageCreator class using union dispatch
  class MessageCreator {
   public:
    // Constructor for function pointer (message_type = 0)
    MessageCreator(MessageCreatorPtr ptr) : message_type_(0) { data_.ptr = ptr; }

    // Constructor for string state capture
    MessageCreator(const std::string &value, uint16_t msg_type) : message_type_(msg_type) {
      data_.string_ptr = new std::string(value);
    }

    // Destructor
    ~MessageCreator() {
      // Clean up string data for string-based message types
      if (uses_string_data_()) {
        delete data_.string_ptr;
      }
    }

    // Copy constructor
    MessageCreator(const MessageCreator &other) : message_type_(other.message_type_) {
      if (message_type_ == 0) {
        data_.ptr = other.data_.ptr;
      } else if (uses_string_data_()) {
        data_.string_ptr = new std::string(*other.data_.string_ptr);
      } else {
        data_ = other.data_;  // For POD types
      }
    }

    // Move constructor
    MessageCreator(MessageCreator &&other) noexcept : data_(other.data_), message_type_(other.message_type_) {
      other.message_type_ = 0;  // Reset other to function pointer type
      other.data_.ptr = nullptr;
    }

    // Assignment operators (needed for batch deduplication)
    MessageCreator &operator=(const MessageCreator &other) {
      if (this != &other) {
        // Clean up current string data if needed
        if (uses_string_data_()) {
          delete data_.string_ptr;
        }
        // Copy new data
        message_type_ = other.message_type_;
        if (other.message_type_ == 0) {
          data_.ptr = other.data_.ptr;
        } else if (other.uses_string_data_()) {
          data_.string_ptr = new std::string(*other.data_.string_ptr);
        } else {
          data_ = other.data_;
        }
      }
      return *this;
    }

    MessageCreator &operator=(MessageCreator &&other) noexcept {
      if (this != &other) {
        // Clean up current string data if needed
        if (uses_string_data_()) {
          delete data_.string_ptr;
        }
        // Move data
        message_type_ = other.message_type_;
        data_ = other.data_;
        // Reset other to safe state
        other.message_type_ = 0;
        other.data_.ptr = nullptr;
      }
      return *this;
    }

    // Call operator
    uint16_t operator()(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single) const;

   private:
    // Helper to check if this message type uses heap-allocated strings
    bool uses_string_data_() const { return message_type_ == EventResponse::MESSAGE_TYPE; }
    union CreatorData {
      MessageCreatorPtr ptr;    // 8 bytes
      std::string *string_ptr;  // 8 bytes
    } data_;                    // 8 bytes
    uint16_t message_type_;     // 2 bytes (0 = function ptr, >0 = state capture)
  };

  // Generic batching mechanism for both state updates and entity info
  struct DeferredBatch {
    struct BatchItem {
      EntityBase *entity;      // Entity pointer
      MessageCreator creator;  // Function that creates the message when needed
      uint16_t message_type;   // Message type for overhead calculation

      // Constructor for creating BatchItem
      BatchItem(EntityBase *entity, MessageCreator creator, uint16_t message_type)
          : entity(entity), creator(std::move(creator)), message_type(message_type) {}
    };

    std::vector<BatchItem> items;
    uint32_t batch_start_time{0};
    bool batch_scheduled{false};

    DeferredBatch() {
      // Pre-allocate capacity for typical batch sizes to avoid reallocation
      items.reserve(8);
    }

    // Add item to the batch
    void add_item(EntityBase *entity, MessageCreator creator, uint16_t message_type);
    void clear() {
      items.clear();
      batch_scheduled = false;
      batch_start_time = 0;
    }
    bool empty() const { return items.empty(); }
  };

  DeferredBatch deferred_batch_;
  uint32_t get_batch_delay_ms_() const;
  // Message will use 8 more bytes than the minimum size, and typical
  // MTU is 1500. Sometimes users will see as low as 1460 MTU.
  // If its IPv6 the header is 40 bytes, and if its IPv4
  // the header is 20 bytes. So we have 1460 - 40 = 1420 bytes
  // available for the payload. But we also need to add the size of
  // the protobuf overhead, which is 8 bytes.
  //
  // To be safe we pick 1390 bytes as the maximum size
  // to send in one go. This is the maximum size of a single packet
  // that can be sent over the network.
  // This is to avoid fragmentation of the packet.
  static constexpr size_t MAX_PACKET_SIZE = 1390;  // MTU

  bool schedule_batch_();
  void process_batch_();

  // State for batch buffer allocation
  bool batch_first_message_{false};

  // Helper function to schedule a deferred message with known message type
  bool schedule_message_(EntityBase *entity, MessageCreator creator, uint16_t message_type) {
    this->deferred_batch_.add_item(entity, std::move(creator), message_type);
    return this->schedule_batch_();
  }

  // Overload for function pointers (for info messages and current state reads)
  bool schedule_message_(EntityBase *entity, MessageCreatorPtr function_ptr, uint16_t message_type) {
    return schedule_message_(entity, MessageCreator(function_ptr), message_type);
  }
};

}  // namespace api
}  // namespace esphome
#endif
