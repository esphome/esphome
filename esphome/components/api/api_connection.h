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
#include "esphome/core/string_ref.h"

#include <functional>
#include <limits>
#include <vector>

namespace esphome::api {

// Keepalive timeout in milliseconds
static constexpr uint32_t KEEPALIVE_TIMEOUT_MS = 60000;
// Maximum number of entities to process in a single batch during initial state/info sending
// API 1.14+ clients compute object_id client-side, so messages are smaller and we can fit more per batch
// TODO: Remove MAX_INITIAL_PER_BATCH_LEGACY before 2026.7.0 - all clients should support API 1.14 by then
static constexpr size_t MAX_INITIAL_PER_BATCH_LEGACY = 24;  // For clients < API 1.14 (includes object_id)
static constexpr size_t MAX_INITIAL_PER_BATCH = 34;         // For clients >= API 1.14 (no object_id)
// Verify MAX_MESSAGES_PER_BATCH (defined in api_frame_helper.h) can hold the initial batch
static_assert(MAX_MESSAGES_PER_BATCH >= MAX_INITIAL_PER_BATCH,
              "MAX_MESSAGES_PER_BATCH must be >= MAX_INITIAL_PER_BATCH");

class APIConnection final : public APIServerConnection {
 public:
  friend class APIServer;
  friend class ListEntitiesIterator;
  APIConnection(std::unique_ptr<socket::Socket> socket, APIServer *parent);
  virtual ~APIConnection();

  void start();
  void loop();

  bool send_list_info_done() {
    return this->schedule_message_(nullptr, ListEntitiesDoneResponse::MESSAGE_TYPE,
                                   ListEntitiesDoneResponse::ESTIMATED_SIZE);
  }
#ifdef USE_BINARY_SENSOR
  bool send_binary_sensor_state(binary_sensor::BinarySensor *binary_sensor);
#endif
#ifdef USE_COVER
  bool send_cover_state(cover::Cover *cover);
  void cover_command(const CoverCommandRequest &msg) override;
#endif
#ifdef USE_FAN
  bool send_fan_state(fan::Fan *fan);
  void fan_command(const FanCommandRequest &msg) override;
#endif
#ifdef USE_LIGHT
  bool send_light_state(light::LightState *light);
  void light_command(const LightCommandRequest &msg) override;
#endif
#ifdef USE_SENSOR
  bool send_sensor_state(sensor::Sensor *sensor);
#endif
#ifdef USE_SWITCH
  bool send_switch_state(switch_::Switch *a_switch);
  void switch_command(const SwitchCommandRequest &msg) override;
#endif
#ifdef USE_TEXT_SENSOR
  bool send_text_sensor_state(text_sensor::TextSensor *text_sensor);
#endif
#ifdef USE_CAMERA
  void set_camera_state(std::shared_ptr<camera::CameraImage> image);
  void camera_image(const CameraImageRequest &msg) override;
#endif
#ifdef USE_CLIMATE
  bool send_climate_state(climate::Climate *climate);
  void climate_command(const ClimateCommandRequest &msg) override;
#endif
#ifdef USE_NUMBER
  bool send_number_state(number::Number *number);
  void number_command(const NumberCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATE
  bool send_date_state(datetime::DateEntity *date);
  void date_command(const DateCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_TIME
  bool send_time_state(datetime::TimeEntity *time);
  void time_command(const TimeCommandRequest &msg) override;
#endif
#ifdef USE_DATETIME_DATETIME
  bool send_datetime_state(datetime::DateTimeEntity *datetime);
  void datetime_command(const DateTimeCommandRequest &msg) override;
#endif
#ifdef USE_TEXT
  bool send_text_state(text::Text *text);
  void text_command(const TextCommandRequest &msg) override;
#endif
#ifdef USE_SELECT
  bool send_select_state(select::Select *select);
  void select_command(const SelectCommandRequest &msg) override;
#endif
#ifdef USE_BUTTON
  void button_command(const ButtonCommandRequest &msg) override;
#endif
#ifdef USE_LOCK
  bool send_lock_state(lock::Lock *a_lock);
  void lock_command(const LockCommandRequest &msg) override;
#endif
#ifdef USE_VALVE
  bool send_valve_state(valve::Valve *valve);
  void valve_command(const ValveCommandRequest &msg) override;
#endif
#ifdef USE_MEDIA_PLAYER
  bool send_media_player_state(media_player::MediaPlayer *media_player);
  void media_player_command(const MediaPlayerCommandRequest &msg) override;
#endif
  bool try_send_log_message(int level, const char *tag, const char *line, size_t message_len);
#ifdef USE_API_HOMEASSISTANT_SERVICES
  void send_homeassistant_action(const HomeassistantActionRequest &call) {
    if (!this->flags_.service_call_subscription)
      return;
    this->send_message(call, HomeassistantActionRequest::MESSAGE_TYPE);
  }
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  void on_homeassistant_action_response(const HomeassistantActionResponse &msg) override;
#endif  // USE_API_HOMEASSISTANT_ACTION_RESPONSES
#endif  // USE_API_HOMEASSISTANT_SERVICES
#ifdef USE_BLUETOOTH_PROXY
  void subscribe_bluetooth_le_advertisements(const SubscribeBluetoothLEAdvertisementsRequest &msg) override;
  void unsubscribe_bluetooth_le_advertisements(const UnsubscribeBluetoothLEAdvertisementsRequest &msg) override;

  void bluetooth_device_request(const BluetoothDeviceRequest &msg) override;
  void bluetooth_gatt_read(const BluetoothGATTReadRequest &msg) override;
  void bluetooth_gatt_write(const BluetoothGATTWriteRequest &msg) override;
  void bluetooth_gatt_read_descriptor(const BluetoothGATTReadDescriptorRequest &msg) override;
  void bluetooth_gatt_write_descriptor(const BluetoothGATTWriteDescriptorRequest &msg) override;
  void bluetooth_gatt_get_services(const BluetoothGATTGetServicesRequest &msg) override;
  void bluetooth_gatt_notify(const BluetoothGATTNotifyRequest &msg) override;
  bool send_subscribe_bluetooth_connections_free_response(const SubscribeBluetoothConnectionsFreeRequest &msg) override;
  void bluetooth_scanner_set_mode(const BluetoothScannerSetModeRequest &msg) override;

#endif
#ifdef USE_HOMEASSISTANT_TIME
  void send_time_request() {
    GetTimeRequest req;
    this->send_message(req, GetTimeRequest::MESSAGE_TYPE);
  }
#endif

#ifdef USE_VOICE_ASSISTANT
  void subscribe_voice_assistant(const SubscribeVoiceAssistantRequest &msg) override;
  void on_voice_assistant_response(const VoiceAssistantResponse &msg) override;
  void on_voice_assistant_event_response(const VoiceAssistantEventResponse &msg) override;
  void on_voice_assistant_audio(const VoiceAssistantAudio &msg) override;
  void on_voice_assistant_timer_event_response(const VoiceAssistantTimerEventResponse &msg) override;
  void on_voice_assistant_announce_request(const VoiceAssistantAnnounceRequest &msg) override;
  bool send_voice_assistant_get_configuration_response(const VoiceAssistantConfigurationRequest &msg) override;
  void voice_assistant_set_configuration(const VoiceAssistantSetConfiguration &msg) override;
#endif

#ifdef USE_ZWAVE_PROXY
  void zwave_proxy_frame(const ZWaveProxyFrame &msg) override;
  void zwave_proxy_request(const ZWaveProxyRequest &msg) override;
#endif

#ifdef USE_ALARM_CONTROL_PANEL
  bool send_alarm_control_panel_state(alarm_control_panel::AlarmControlPanel *a_alarm_control_panel);
  void alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) override;
#endif

#ifdef USE_WATER_HEATER
  bool send_water_heater_state(water_heater::WaterHeater *water_heater);
  void on_water_heater_command_request(const WaterHeaterCommandRequest &msg) override;
#endif

#ifdef USE_IR_RF
  void infrared_rf_transmit_raw_timings(const InfraredRFTransmitRawTimingsRequest &msg) override;
  void send_infrared_rf_receive_event(const InfraredRFReceiveEvent &msg);
#endif

#ifdef USE_EVENT
  void send_event(event::Event *event);
#endif

#ifdef USE_UPDATE
  bool send_update_state(update::UpdateEntity *update);
  void update_command(const UpdateCommandRequest &msg) override;
#endif

  void on_disconnect_response(const DisconnectResponse &value) override;
  void on_ping_response(const PingResponse &value) override {
    // we initiated ping
    this->flags_.sent_ping = false;
  }
#ifdef USE_API_HOMEASSISTANT_STATES
  void on_home_assistant_state_response(const HomeAssistantStateResponse &msg) override;
#endif
#ifdef USE_HOMEASSISTANT_TIME
  void on_get_time_response(const GetTimeResponse &value) override;
#endif
  bool send_hello_response(const HelloRequest &msg) override;
  bool send_disconnect_response(const DisconnectRequest &msg) override;
  bool send_ping_response(const PingRequest &msg) override;
  bool send_device_info_response(const DeviceInfoRequest &msg) override;
  void list_entities(const ListEntitiesRequest &msg) override { this->begin_iterator_(ActiveIterator::LIST_ENTITIES); }
  void subscribe_states(const SubscribeStatesRequest &msg) override {
    this->flags_.state_subscription = true;
    // Start initial state iterator only if no iterator is active
    // If list_entities is running, we'll start initial_state when it completes
    if (this->active_iterator_ == ActiveIterator::NONE) {
      this->begin_iterator_(ActiveIterator::INITIAL_STATE);
    }
  }
  void subscribe_logs(const SubscribeLogsRequest &msg) override {
    this->flags_.log_subscription = msg.level;
    if (msg.dump_config)
      App.schedule_dump_config();
  }
#ifdef USE_API_HOMEASSISTANT_SERVICES
  void subscribe_homeassistant_services(const SubscribeHomeassistantServicesRequest &msg) override {
    this->flags_.service_call_subscription = true;
  }
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
  void subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) override;
#endif
#ifdef USE_API_USER_DEFINED_ACTIONS
  void execute_service(const ExecuteServiceRequest &msg) override;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  void send_execute_service_response(uint32_t call_id, bool success, StringRef error_message);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  void send_execute_service_response(uint32_t call_id, bool success, StringRef error_message,
                                     const uint8_t *response_data, size_t response_data_len);
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
#endif  // USE_API_USER_DEFINED_ACTION_RESPONSES
#endif
#ifdef USE_API_NOISE
  bool send_noise_encryption_set_key_response(const NoiseEncryptionSetKeyRequest &msg) override;
#endif

  bool is_authenticated() override {
    return static_cast<ConnectionState>(this->flags_.connection_state) == ConnectionState::AUTHENTICATED;
  }
  bool is_connection_setup() override {
    return static_cast<ConnectionState>(this->flags_.connection_state) == ConnectionState::CONNECTED ||
           this->is_authenticated();
  }
  uint8_t get_log_subscription_level() const { return this->flags_.log_subscription; }

  // Get client API version for feature detection
  bool client_supports_api_version(uint16_t major, uint16_t minor) const {
    return this->client_api_version_major_ > major ||
           (this->client_api_version_major_ == major && this->client_api_version_minor_ >= minor);
  }

  void on_fatal_error() override;
  void on_no_setup_connection() override;
  ProtoWriteBuffer create_buffer(uint32_t reserve_size) override {
    // FIXME: ensure no recursive writes can happen

    // Get header padding size - used for both reserve and insert
    uint8_t header_padding = this->helper_->frame_header_padding();
    // Get shared buffer from parent server
    std::vector<uint8_t> &shared_buf = this->parent_->get_shared_buffer_ref();
    this->prepare_first_message_buffer(shared_buf, header_padding,
                                       reserve_size + header_padding + this->helper_->frame_footer_size());
    return {&shared_buf};
  }

  void prepare_first_message_buffer(std::vector<uint8_t> &shared_buf, size_t header_padding, size_t total_size) {
    shared_buf.clear();
    // Reserve space for header padding + message + footer
    // - Header padding: space for protocol headers (7 bytes for Noise, 6 for Plaintext)
    // - Footer: space for MAC (16 bytes for Noise, 0 for Plaintext)
    shared_buf.reserve(total_size);
    // Resize to add header padding so message encoding starts at the correct position
    shared_buf.resize(header_padding);
  }

  bool try_to_clear_buffer(bool log_out_of_space);
  bool send_buffer(ProtoWriteBuffer buffer, uint8_t message_type) override;

  const char *get_name() const { return this->helper_->get_client_name(); }
  /// Get peer name (IP address) - cached at connection init time
  const char *get_peername() const { return this->helper_->get_client_peername(); }

 protected:
  // Helper function to handle authentication completion
  void complete_authentication_();

#ifdef USE_CAMERA
  void try_send_camera_image_();
#endif

#ifdef USE_API_HOMEASSISTANT_STATES
  void process_state_subscriptions_();
#endif

  // Non-template helper to encode any ProtoMessage
  static uint16_t encode_message_to_buffer(ProtoMessage &msg, uint8_t message_type, APIConnection *conn,
                                           uint32_t remaining_size, bool is_single);

  // Helper to fill entity state base and encode message
  static uint16_t fill_and_encode_entity_state(EntityBase *entity, StateResponseProtoMessage &msg, uint8_t message_type,
                                               APIConnection *conn, uint32_t remaining_size, bool is_single) {
    msg.key = entity->get_object_id_hash();
#ifdef USE_DEVICES
    msg.device_id = entity->get_device_id();
#endif
    return encode_message_to_buffer(msg, message_type, conn, remaining_size, is_single);
  }

  // Helper to fill entity info base and encode message
  static uint16_t fill_and_encode_entity_info(EntityBase *entity, InfoResponseProtoMessage &msg, uint8_t message_type,
                                              APIConnection *conn, uint32_t remaining_size, bool is_single) {
    // Set common fields that are shared by all entity types
    msg.key = entity->get_object_id_hash();

    // API 1.14+ clients compute object_id client-side from the entity name
    // For older clients, we must send object_id for backward compatibility
    // See: https://github.com/esphome/backlog/issues/76
    // TODO: Remove this backward compat code before 2026.7.0 - all clients should support API 1.14 by then
    // Buffer must remain in scope until encode_message_to_buffer is called
    char object_id_buf[OBJECT_ID_MAX_LEN];
    if (!conn->client_supports_api_version(1, 14)) {
      msg.object_id = entity->get_object_id_to(object_id_buf);
    }

    if (entity->has_own_name()) {
      msg.name = entity->get_name();
    }

    // Set common EntityBase properties
#ifdef USE_ENTITY_ICON
    msg.icon = entity->get_icon_ref();
#endif
    msg.disabled_by_default = entity->is_disabled_by_default();
    msg.entity_category = static_cast<enums::EntityCategory>(entity->get_entity_category());
#ifdef USE_DEVICES
    msg.device_id = entity->get_device_id();
#endif
    return encode_message_to_buffer(msg, message_type, conn, remaining_size, is_single);
  }

#ifdef USE_VOICE_ASSISTANT
  // Helper to check voice assistant validity and connection ownership
  inline bool check_voice_assistant_api_connection_() const;
#endif

  // Get the max batch size based on client API version
  // API 1.14+ clients don't receive object_id, so messages are smaller and more fit per batch
  // TODO: Remove this method before 2026.7.0 and use MAX_INITIAL_PER_BATCH directly
  size_t get_max_batch_size_() const {
    return this->client_supports_api_version(1, 14) ? MAX_INITIAL_PER_BATCH : MAX_INITIAL_PER_BATCH_LEGACY;
  }

  // Helper method to process multiple entities from an iterator in a batch
  template<typename Iterator> void process_iterator_batch_(Iterator &iterator) {
    size_t initial_size = this->deferred_batch_.size();
    size_t max_batch = this->get_max_batch_size_();
    while (!iterator.completed() && (this->deferred_batch_.size() - initial_size) < max_batch) {
      iterator.advance();
    }

    // If the batch is full, process it immediately
    // Note: iterator.advance() already calls schedule_batch_() via schedule_message_()
    if (this->deferred_batch_.size() >= max_batch) {
      this->process_batch_();
    }
  }

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
#ifdef USE_WATER_HEATER
  static uint16_t try_send_water_heater_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single);
  static uint16_t try_send_water_heater_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                             bool is_single);
#endif
#ifdef USE_INFRARED
  static uint16_t try_send_infrared_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                         bool is_single);
#endif
#ifdef USE_EVENT
  static uint16_t try_send_event_response(event::Event *event, StringRef event_type, APIConnection *conn,
                                          uint32_t remaining_size, bool is_single);
  static uint16_t try_send_event_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single);
#endif
#ifdef USE_UPDATE
  static uint16_t try_send_update_state(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);
  static uint16_t try_send_update_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif
#ifdef USE_CAMERA
  static uint16_t try_send_camera_info(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                       bool is_single);
#endif

  // Method for ListEntitiesDone batching
  static uint16_t try_send_list_info_done(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                          bool is_single);

  // Method for DisconnectRequest batching
  static uint16_t try_send_disconnect_request(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                              bool is_single);

  // Batch message method for ping requests
  static uint16_t try_send_ping_request(EntityBase *entity, APIConnection *conn, uint32_t remaining_size,
                                        bool is_single);

  // === Optimal member ordering for 32-bit systems ===

  // Group 1: Pointers (4 bytes each on 32-bit)
  std::unique_ptr<APIFrameHelper> helper_;
  APIServer *parent_;

  // Group 2: Iterator union (saves ~16 bytes vs separate iterators)
  // These iterators are never active simultaneously - list_entities runs to completion
  // before initial_state begins, so we use a union with explicit construction/destruction.
  enum class ActiveIterator : uint8_t { NONE, LIST_ENTITIES, INITIAL_STATE };

  union IteratorUnion {
    ListEntitiesIterator list_entities;
    InitialStateIterator initial_state;
    // Constructor/destructor do nothing - use placement new/explicit destructor
    IteratorUnion() {}
    ~IteratorUnion() {}
  } iterator_storage_;

  // Helper methods for iterator lifecycle management
  void destroy_active_iterator_();
  void begin_iterator_(ActiveIterator type);
#ifdef USE_CAMERA
  std::unique_ptr<camera::CameraImageReader> image_reader_;
#endif

  // Group 3: 4-byte types
  uint32_t last_traffic_;
#ifdef USE_API_HOMEASSISTANT_STATES
  int state_subs_at_ = -1;
#endif

  // Function pointer type for message encoding
  using MessageCreatorPtr = uint16_t (*)(EntityBase *, APIConnection *, uint32_t remaining_size, bool is_single);

  // Generic batching mechanism for both state updates and entity info
  struct DeferredBatch {
    // Sentinel value for unused aux_data_index
    static constexpr uint8_t AUX_DATA_UNUSED = std::numeric_limits<uint8_t>::max();

    struct BatchItem {
      EntityBase *entity;                       // 4 bytes - Entity pointer
      uint8_t message_type;                     // 1 byte - Message type for protocol and dispatch
      uint8_t estimated_size;                   // 1 byte - Estimated message size (max 255 bytes)
      uint8_t aux_data_index{AUX_DATA_UNUSED};  // 1 byte - For events: index into entity's event_types
      // 1 byte padding
    };

    std::vector<BatchItem> items;
    uint32_t batch_start_time{0};

    // No pre-allocation - log connections never use batching, and for
    // connections that do, buffers are released after initial sync anyway

    // Add item to the batch (with deduplication)
    void add_item(EntityBase *entity, uint8_t message_type, uint8_t estimated_size,
                  uint8_t aux_data_index = AUX_DATA_UNUSED);
    // Add item to the front of the batch (for high priority messages like ping)
    void add_item_front(EntityBase *entity, uint8_t message_type, uint8_t estimated_size);

    // Clear all items
    void clear() {
      items.clear();
      batch_start_time = 0;
    }

    // Remove processed items from the front
    void remove_front(size_t count) { items.erase(items.begin(), items.begin() + count); }

    bool empty() const { return items.empty(); }
    size_t size() const { return items.size(); }
    const BatchItem &operator[](size_t index) const { return items[index]; }

    // Release excess capacity - only releases if items already empty
    void release_buffer() {
      // Safe to call: batch is processed before release_buffer is called,
      // and if any items remain (partial processing), we must not clear them.
      // Use swap trick since shrink_to_fit() is non-binding and may be ignored.
      if (items.empty()) {
        std::vector<BatchItem>().swap(items);
      }
    }
  };

  // DeferredBatch here (16 bytes, 4-byte aligned)
  DeferredBatch deferred_batch_;

  // ConnectionState enum for type safety
  enum class ConnectionState : uint8_t {
    WAITING_FOR_HELLO = 0,
    CONNECTED = 1,
    AUTHENTICATED = 2,
  };

  // Group 5: Pack all small members together to minimize padding
  // This group starts at a 4-byte boundary after DeferredBatch
  struct APIFlags {
    // Connection state only needs 2 bits (3 states)
    uint8_t connection_state : 2;
    // Log subscription needs 3 bits (log levels 0-7)
    uint8_t log_subscription : 3;
    // Boolean flags (1 bit each)
    uint8_t remove : 1;
    uint8_t state_subscription : 1;
    uint8_t sent_ping : 1;

    uint8_t service_call_subscription : 1;
    uint8_t next_close : 1;
    uint8_t batch_scheduled : 1;
    uint8_t batch_first_message : 1;          // For batch buffer allocation
    uint8_t should_try_send_immediately : 1;  // True after initial states are sent
#ifdef HAS_PROTO_MESSAGE_DUMP
    uint8_t log_only_mode : 1;
#endif
  } flags_{};  // 2 bytes total

  // 2-byte types immediately after flags_ (no padding between them)
  uint16_t client_api_version_major_{0};
  uint16_t client_api_version_minor_{0};
  // 1-byte type to fill padding
  ActiveIterator active_iterator_{ActiveIterator::NONE};
  // Total: 2 (flags) + 2 + 2 + 1 = 7 bytes, then 1 byte padding to next 4-byte boundary

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
  static constexpr size_t MAX_BATCH_PACKET_SIZE = 1390;  // MTU

  bool schedule_batch_();
  void process_batch_();
  void clear_batch_() {
    this->deferred_batch_.clear();
    this->flags_.batch_scheduled = false;
  }

  // Dispatch message encoding based on message_type - replaces function pointer storage
  // Switch assigns pointer, single call site for smaller code size
  uint16_t dispatch_message_(const DeferredBatch::BatchItem &item, uint32_t remaining_size, bool is_single);

#ifdef HAS_PROTO_MESSAGE_DUMP
  void log_batch_item_(const DeferredBatch::BatchItem &item) {
    this->flags_.log_only_mode = true;
    this->dispatch_message_(item, MAX_BATCH_PACKET_SIZE, true);
    this->flags_.log_only_mode = false;
  }
#endif

  // Helper to check if a message type should bypass batching
  // Returns true if:
  // 1. It's an UpdateStateResponse (always send immediately to handle cases where
  //    the main loop is blocked, e.g., during OTA updates)
  // 2. It's an EventResponse (events are edge-triggered - every occurrence matters)
  // 3. OR: User has opted into immediate sending (should_try_send_immediately = true
  //    AND batch_delay = 0)
  inline bool should_send_immediately_(uint8_t message_type) const {
    return (
#ifdef USE_UPDATE
        message_type == UpdateStateResponse::MESSAGE_TYPE ||
#endif
#ifdef USE_EVENT
        message_type == EventResponse::MESSAGE_TYPE ||
#endif
        (this->flags_.should_try_send_immediately && this->get_batch_delay_ms_() == 0));
  }

  // Helper method to send a message either immediately or via batching
  // Tries immediate send if should_send_immediately_() returns true and buffer has space
  // Falls back to batching if immediate send fails or isn't applicable
  bool send_message_smart_(EntityBase *entity, uint8_t message_type, uint8_t estimated_size,
                           uint8_t aux_data_index = DeferredBatch::AUX_DATA_UNUSED) {
    if (this->should_send_immediately_(message_type) && this->helper_->can_write_without_blocking()) {
      DeferredBatch::BatchItem item{entity, message_type, estimated_size, aux_data_index};
      if (this->dispatch_message_(item, MAX_BATCH_PACKET_SIZE, true) &&
          this->send_buffer(ProtoWriteBuffer{&this->parent_->get_shared_buffer_ref()}, message_type)) {
#ifdef HAS_PROTO_MESSAGE_DUMP
        this->log_batch_item_(item);
#endif
        return true;
      }
    }
    return this->schedule_message_(entity, message_type, estimated_size, aux_data_index);
  }

  // Helper function to schedule a deferred message with known message type
  bool schedule_message_(EntityBase *entity, uint8_t message_type, uint8_t estimated_size,
                         uint8_t aux_data_index = DeferredBatch::AUX_DATA_UNUSED) {
    this->deferred_batch_.add_item(entity, message_type, estimated_size, aux_data_index);
    return this->schedule_batch_();
  }

  // Helper function to schedule a high priority message at the front of the batch
  bool schedule_message_front_(EntityBase *entity, uint8_t message_type, uint8_t estimated_size) {
    this->deferred_batch_.add_item_front(entity, message_type, estimated_size);
    return this->schedule_batch_();
  }

  // Helper function to log client messages with name and peername
  void log_client_(int level, const LogString *message);
  // Helper function to log API errors with errno
  void log_warning_(const LogString *message, APIError err);
  // Helper to handle fatal errors with logging
  inline void fatal_error_with_log_(const LogString *message, APIError err) {
    this->on_fatal_error();
    this->log_warning_(message, err);
  }
};

}  // namespace esphome::api
#endif
