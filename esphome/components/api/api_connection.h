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
// Maximum number of entities to process in a single batch during initial state/info sending
static constexpr size_t MAX_INITIAL_PER_BATCH = 20;

class APIConnection : public APIServerConnection {
 public:
  friend class APIServer;
  friend class ListEntitiesIterator;
  APIConnection(std::unique_ptr<socket::Socket> socket, APIServer *parent);
  virtual ~APIConnection();

  void start();
  void loop();

  bool send_list_info_done() {
    return this->schedule_message_(nullptr, &APIConnection::try_send_list_info_done,
                                   ListEntitiesDoneResponse::MESSAGE_TYPE, ListEntitiesDoneResponse::ESTIMATED_SIZE);
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
  void send_homeassistant_service_call(const HomeassistantServiceResponse &call) {
    if (!this->flags_.service_call_subscription)
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
  void alarm_control_panel_command(const AlarmControlPanelCommandRequest &msg) override;
#endif

#ifdef USE_EVENT
  void send_event(event::Event *event, const std::string &event_type);
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
    this->flags_.state_subscription = true;
    this->initial_state_iterator_.begin();
  }
  void subscribe_logs(const SubscribeLogsRequest &msg) override {
    this->flags_.log_subscription = msg.level;
    if (msg.dump_config)
      App.schedule_dump_config();
  }
  void subscribe_homeassistant_services(const SubscribeHomeassistantServicesRequest &msg) override {
    this->flags_.service_call_subscription = true;
  }
  void subscribe_home_assistant_states(const SubscribeHomeAssistantStatesRequest &msg) override;
  GetTimeResponse get_time(const GetTimeRequest &msg) override {
    // TODO
    return {};
  }
#ifdef USE_API_SERVICES
  void execute_service(const ExecuteServiceRequest &msg) override;
#endif
#ifdef USE_API_NOISE
  NoiseEncryptionSetKeyResponse noise_encryption_set_key(const NoiseEncryptionSetKeyRequest &msg) override;
#endif

  bool is_authenticated() override {
    return static_cast<ConnectionState>(this->flags_.connection_state) == ConnectionState::AUTHENTICATED;
  }
  bool is_connection_setup() override {
    return static_cast<ConnectionState>(this->flags_.connection_state) == ConnectionState::CONNECTED ||
           this->is_authenticated();
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
  bool send_buffer(ProtoWriteBuffer buffer, uint8_t message_type) override;

  std::string get_client_combined_info() const {
    if (this->client_info_ == this->client_peername_) {
      // Before Hello message, both are the same (just IP:port)
      return this->client_info_;
    }
    return this->client_info_ + " (" + this->client_peername_ + ")";
  }

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
#ifdef USE_DEVICES
    response.device_id = entity->get_device_id();
#endif
  }

  // Helper function to fill common entity state fields
  static void fill_entity_state_base(esphome::EntityBase *entity, StateResponseProtoMessage &response) {
    response.key = entity->get_object_id_hash();
#ifdef USE_DEVICES
    response.device_id = entity->get_device_id();
#endif
  }

  // Non-template helper to encode any ProtoMessage
  static uint16_t encode_message_to_buffer(ProtoMessage &msg, uint8_t message_type, APIConnection *conn,
                                           uint32_t remaining_size, bool is_single);

#ifdef USE_VOICE_ASSISTANT
  // Helper to check voice assistant validity and connection ownership
  inline bool check_voice_assistant_api_connection_() const;
#endif

  // Helper method to process multiple entities from an iterator in a batch
  template<typename Iterator> void process_iterator_batch_(Iterator &iterator) {
    size_t initial_size = this->deferred_batch_.size();
    while (!iterator.completed() && (this->deferred_batch_.size() - initial_size) < MAX_INITIAL_PER_BATCH) {
      iterator.advance();
    }

    // If the batch is full, process it immediately
    // Note: iterator.advance() already calls schedule_batch_() via schedule_message_()
    if (this->deferred_batch_.size() >= MAX_INITIAL_PER_BATCH) {
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

  // Group 2: Larger objects (must be 4-byte aligned)
  // These contain vectors/pointers internally, so putting them early ensures good alignment
  InitialStateIterator initial_state_iterator_;
  ListEntitiesIterator list_entities_iterator_;
#ifdef USE_CAMERA
  std::unique_ptr<camera::CameraImageReader> image_reader_;
#endif

  // Group 3: Strings (12 bytes each on 32-bit, 4-byte aligned)
  std::string client_info_;
  std::string client_peername_;

  // Group 4: 4-byte types
  uint32_t last_traffic_;
  int state_subs_at_ = -1;

  // Function pointer type for message encoding
  using MessageCreatorPtr = uint16_t (*)(EntityBase *, APIConnection *, uint32_t remaining_size, bool is_single);

  class MessageCreator {
   public:
    // Constructor for function pointer
    MessageCreator(MessageCreatorPtr ptr) { data_.function_ptr = ptr; }

    // Constructor for string state capture
    explicit MessageCreator(const std::string &str_value) { data_.string_ptr = new std::string(str_value); }

    // No destructor - cleanup must be called explicitly with message_type

    // Delete copy operations - MessageCreator should only be moved
    MessageCreator(const MessageCreator &other) = delete;
    MessageCreator &operator=(const MessageCreator &other) = delete;

    // Move constructor
    MessageCreator(MessageCreator &&other) noexcept : data_(other.data_) { other.data_.function_ptr = nullptr; }

    // Move assignment
    MessageCreator &operator=(MessageCreator &&other) noexcept {
      if (this != &other) {
        // IMPORTANT: Caller must ensure cleanup() was called if this contains a string!
        // In our usage, this happens in add_item() deduplication and vector::erase()
        data_ = other.data_;
        other.data_.function_ptr = nullptr;
      }
      return *this;
    }

    // Call operator - uses message_type to determine union type
    uint16_t operator()(EntityBase *entity, APIConnection *conn, uint32_t remaining_size, bool is_single,
                        uint8_t message_type) const;

    // Manual cleanup method - must be called before destruction for string types
    void cleanup(uint8_t message_type) {
#ifdef USE_EVENT
      if (message_type == EventResponse::MESSAGE_TYPE && data_.string_ptr != nullptr) {
        delete data_.string_ptr;
        data_.string_ptr = nullptr;
      }
#endif
    }

   private:
    union Data {
      MessageCreatorPtr function_ptr;
      std::string *string_ptr;
    } data_;  // 4 bytes on 32-bit, 8 bytes on 64-bit - same as before
  };

  // Generic batching mechanism for both state updates and entity info
  struct DeferredBatch {
    struct BatchItem {
      EntityBase *entity;      // Entity pointer
      MessageCreator creator;  // Function that creates the message when needed
      uint8_t message_type;    // Message type for overhead calculation (max 255)
      uint8_t estimated_size;  // Estimated message size (max 255 bytes)

      // Constructor for creating BatchItem
      BatchItem(EntityBase *entity, MessageCreator creator, uint8_t message_type, uint8_t estimated_size)
          : entity(entity), creator(std::move(creator)), message_type(message_type), estimated_size(estimated_size) {}
    };

    std::vector<BatchItem> items;
    uint32_t batch_start_time{0};

   private:
    // Helper to cleanup items from the beginning
    void cleanup_items_(size_t count) {
      for (size_t i = 0; i < count; i++) {
        items[i].creator.cleanup(items[i].message_type);
      }
    }

   public:
    DeferredBatch() {
      // Pre-allocate capacity for typical batch sizes to avoid reallocation
      items.reserve(8);
    }

    ~DeferredBatch() {
      // Ensure cleanup of any remaining items
      clear();
    }

    // Add item to the batch
    void add_item(EntityBase *entity, MessageCreator creator, uint8_t message_type, uint8_t estimated_size);
    // Add item to the front of the batch (for high priority messages like ping)
    void add_item_front(EntityBase *entity, MessageCreator creator, uint8_t message_type, uint8_t estimated_size);

    // Clear all items with proper cleanup
    void clear() {
      cleanup_items_(items.size());
      items.clear();
      batch_start_time = 0;
    }

    // Remove processed items from the front with proper cleanup
    void remove_front(size_t count) {
      cleanup_items_(count);
      items.erase(items.begin(), items.begin() + count);
    }

    bool empty() const { return items.empty(); }
    size_t size() const { return items.size(); }
    const BatchItem &operator[](size_t index) const { return items[index]; }
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
  // Total: 2 (flags) + 2 + 2 = 6 bytes, then 2 bytes padding to next 4-byte boundary

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

#ifdef HAS_PROTO_MESSAGE_DUMP
  // Helper to log a proto message from a MessageCreator object
  void log_proto_message_(EntityBase *entity, const MessageCreator &creator, uint8_t message_type) {
    this->flags_.log_only_mode = true;
    creator(entity, this, MAX_BATCH_PACKET_SIZE, true, message_type);
    this->flags_.log_only_mode = false;
  }

  void log_batch_item_(const DeferredBatch::BatchItem &item) {
    // Use the helper to log the message
    this->log_proto_message_(item.entity, item.creator, item.message_type);
  }
#endif

  // Helper method to send a message either immediately or via batching
  bool send_message_smart_(EntityBase *entity, MessageCreatorPtr creator, uint8_t message_type,
                           uint8_t estimated_size) {
    // Try to send immediately if:
    // 1. We should try to send immediately (should_try_send_immediately = true)
    // 2. Batch delay is 0 (user has opted in to immediate sending)
    // 3. Buffer has space available
    if (this->flags_.should_try_send_immediately && this->get_batch_delay_ms_() == 0 &&
        this->helper_->can_write_without_blocking()) {
      // Now actually encode and send
      if (creator(entity, this, MAX_BATCH_PACKET_SIZE, true) &&
          this->send_buffer(ProtoWriteBuffer{&this->parent_->get_shared_buffer_ref()}, message_type)) {
#ifdef HAS_PROTO_MESSAGE_DUMP
        // Log the message in verbose mode
        this->log_proto_message_(entity, MessageCreator(creator), message_type);
#endif
        return true;
      }

      // If immediate send failed, fall through to batching
    }

    // Fall back to scheduled batching
    return this->schedule_message_(entity, creator, message_type, estimated_size);
  }

  // Helper function to schedule a deferred message with known message type
  bool schedule_message_(EntityBase *entity, MessageCreator creator, uint8_t message_type, uint8_t estimated_size) {
    this->deferred_batch_.add_item(entity, std::move(creator), message_type, estimated_size);
    return this->schedule_batch_();
  }

  // Overload for function pointers (for info messages and current state reads)
  bool schedule_message_(EntityBase *entity, MessageCreatorPtr function_ptr, uint8_t message_type,
                         uint8_t estimated_size) {
    return schedule_message_(entity, MessageCreator(function_ptr), message_type, estimated_size);
  }

  // Helper function to schedule a high priority message at the front of the batch
  bool schedule_message_front_(EntityBase *entity, MessageCreatorPtr function_ptr, uint8_t message_type,
                               uint8_t estimated_size) {
    this->deferred_batch_.add_item_front(entity, MessageCreator(function_ptr), message_type, estimated_size);
    return this->schedule_batch_();
  }
};

}  // namespace api
}  // namespace esphome
#endif
