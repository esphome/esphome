#pragma once

#include <array>
#include <span>

#include "esphome/components/packet_interface/packet_interface.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

namespace esphome::entity_transport {

/**
 * Sends the states of local sensors and binary sensors over a packet interface, and publishes the states received
 * from other devices to remote entities.
 *
 * Each device numbers its sensors and its binary sensors from 0, and sends a dictionary that gives the id for each
 * number. Data packets then refer to an entity by its number. Every packet carries the sender's dictionary id, a hash
 * of its device name and dictionary, so that a receiver knows which device sent it and which dictionary applies.
 * All values are little endian.
 *
 * Dictionary packet:
 * - type: 1 byte, PACKET_DICTIONARY;
 * - dictionary id: 4 bytes;
 * - part: 1 byte, the packet's index in the dictionary in bits 0-6, counting from 0 and wrapping, and bit 7 set on
 *   the last packet;
 * - sender's device name: length (1 byte), then the name;
 * - one or more sections, each: entity type (1 byte), first number (1 byte), count (1 byte), then for each entity
 *   its id: length (1 byte), then the id.
 *
 * Data packet:
 * - type: 1 byte, PACKET_DATA;
 * - dictionary id: 4 bytes;
 * - records, to the end, each a 16-bit tag, then its payload. The tag holds the entity's number in bits 0-7, its type
 *   in bits 8-12, and the payload form in bits 13-14: none, with the state in bit 15 (binary sensors); 4 bytes (a
 *   sensor's float); a length byte, then that many bytes; or no state, with no payload. The form gives the record's
 *   size, so a receiver skips records of types it does not know.
 *
 * Every packet is checked in full before anything in it is used, so a damaged or forged packet changes no state.
 *
 * Dictionary request packet, sent by a device that receives data with a dictionary id it has not received in full,
 * with every part in order:
 * - type: 1 byte, PACKET_REQUEST;
 * - the dictionary id wanted: 4 bytes.
 *
 * Changed states are sent from loop(). The dictionary and all states are sent at boot, when a session layer reports
 * the peer connected, when the dictionary is requested, and every update interval. Unless the interface has both a
 * session and a reliability layer, the update interval defaults to DEFAULT_UPDATE_INTERVAL_MS. When the interface
 * refuses a packet, sending is tried again after a delay. With a remote timeout, a remote entity loses its state when
 * none arrives for it within the timeout.
 */
class EntityTransport : public Component {
 public:
  explicit EntityTransport(packet_interface::PacketInterface *interface) : interface_(interface) {}

  void setup() override;
  void loop() override;
  void dump_config() override;

  /// 0 chooses the default for the interface's layers; SCHEDULER_DONT_RUN never resends.
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }
  /// Remote entities lose their state when none arrives for this long; 0 keeps the last state.
  void set_remote_timeout(uint32_t timeout_ms) { this->remote_timeout_ms_ = timeout_ms; }
  void set_entity_count(size_t count) { this->entities_.init(count); }
  // Sensors must be added before binary sensors, so that data packets hold sensor records first
#ifdef USE_SENSOR
  void add_sensor(const char *id, sensor::Sensor *sensor) {
    this->entities_.push_back({sensor, id, this, this->sensor_count_++, true, true});
  }
#endif
#ifdef USE_BINARY_SENSOR
  void add_binary_sensor(const char *id, binary_sensor::BinarySensor *sensor) {
    this->entities_.push_back({sensor, id, this, this->binary_sensor_count_++, false, true});
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  void add_remote_sensor(const char *provider, const char *remote_id, sensor::Sensor *sensor) {
    this->remote_sensors_.push_back({provider, remote_id, sensor});
  }
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  void add_remote_binary_sensor(const char *provider, const char *remote_id, binary_sensor::BinarySensor *sensor) {
    this->remote_binary_sensors_.push_back({provider, remote_id, sensor});
  }
#endif

 protected:
  struct LocalEntity {
    EntityBase *entity;  // a sensor::Sensor or a binary_sensor::BinarySensor, as is_sensor says
    const char *id;
    EntityTransport *parent;
    uint8_t number;
    bool is_sensor;
    bool updated;
  };
  template<typename T> struct RemoteEntity {
    const char *provider;
    const char *remote_id;
    T *entity;
    uint32_t dictionary_id{0};  // learnt from the provider's dictionary
    uint32_t last_state_ms{0};
    uint8_t number{0};
    bool known{false};
    bool has_state{false};  // a state arrived, and has not yet timed out
  };

  void send_all_();
  void mark_updated_(LocalEntity &entity);
  bool send_dictionary_();
  bool send_updated_();
  size_t write_record_(const LocalEntity &entity, uint8_t *record) const;
  bool flush_(size_t length, size_t first, size_t last);
  size_t write_header_(uint8_t type) const;
  void receive_(const packet_interface::PacketBuffer &data);
  void receive_dictionary_(std::span<const uint8_t> packet, uint32_t dictionary_id);
  void receive_data_(std::span<const uint8_t> packet, uint32_t dictionary_id);
  void learn_(const char *provider, size_t provider_length, uint8_t type, uint8_t number, const char *id,
              size_t id_length, uint32_t dictionary_id);
  void request_dictionary_(uint32_t dictionary_id);
  bool dictionary_seen_(uint32_t dictionary_id) const;
  /// Warns about, and clears, each remote entity of \p provider that its complete dictionary does not name.
  void forget_unnamed_(const char *provider, size_t provider_length, uint32_t dictionary_id);
  bool warning_due_();
  void on_session_(bool connected);
  void clear_remote_states_(bool timed_out_only);

  packet_interface::PacketInterface *interface_;
  FixedVector<LocalEntity> entities_;
#ifdef ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT
  StaticVector<RemoteEntity<sensor::Sensor>, ENTITY_TRANSPORT_REMOTE_SENSOR_COUNT> remote_sensors_;
#endif
#ifdef ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT
  StaticVector<RemoteEntity<binary_sensor::BinarySensor>, ENTITY_TRANSPORT_REMOTE_BINARY_SENSOR_COUNT>
      remote_binary_sensors_;
#endif
  // Packets are built here, allocated in setup()
  uint8_t *buffer_{nullptr};
  size_t max_packet_size_{0};
  uint32_t update_interval_ms_{0};
  uint32_t remote_timeout_ms_{0};
  uint32_t dictionary_id_{0};
  uint32_t last_request_ms_{0};
  uint32_t last_warning_ms_{0};
  uint32_t rx_dictionary_id_{0};  // the dictionary whose parts are arriving
  // The last dictionary ids received in full, so a provider whose dictionary names none of our entities is not asked
  // again
  std::array<uint32_t, 8> seen_dictionaries_{};
  uint8_t seen_next_{0};
  uint8_t rx_next_part_{0};     // the index of its next part, or PART_NONE if one was missed
  uint8_t dictionary_part_{0};  // the index of the next dictionary packet to send
  uint8_t sensor_count_{0};
  uint8_t binary_sensor_count_{0};
  size_t dictionary_next_{0};  // the first entity of the next dictionary packet to send
  bool dictionary_pending_{true};
  bool request_sent_{false};
  bool warned_{false};
};

}  // namespace esphome::entity_transport
