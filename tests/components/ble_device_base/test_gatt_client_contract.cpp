// The GATT client contract compiles in no real build until a hub backend is
// configured; this TU pins it on the host so the header cannot rot unseen.
// The contract is a concept (BLEGattConnection is a per-platform alias), so
// the minimal backend here proves the concept stays satisfiable and routes
// events through the duck-typed sink the way a real backend does.
#define USE_BLE_GATT_CLIENT

#include "esphome/components/ble_device_base/ble_gatt_client.h"

#include <gtest/gtest.h>

namespace esphome::ble_device_base::testing {

struct RecordingSink {
  void on_connection_state(bool connected, uint16_t mtu, int error) { this->connected_ = connected; }
  void on_service_discovery_done(int error) { this->discovery_error_ = error; }
  void on_read_result(uint16_t handle, const uint8_t *data, uint16_t len, int error) {}
  void on_write_result(uint16_t handle, int error) {}
  void on_notify_state(uint16_t handle, bool enabled, int error) {}
  void on_notify_data(uint16_t handle, const uint8_t *data, uint16_t len) {}
  void on_pairing_result(int status) {}
  bool connected_{false};
  int discovery_error_{0};
};

static_assert(GattClientEventSinkContract<RecordingSink>, "the recording sink must cover the full event-sink surface");

class MinimalConnection {
 public:
  void set_listener(RecordingSink *listener) { this->listener_ = listener; }

  int connect(uint64_t address, uint8_t addr_type) {
    if (this->listener_ != nullptr)
      this->listener_->on_connection_state(true, 517, 0);
    return 0;
  }
  int disconnect() { return 0; }
  int discover_services() {
    if (this->listener_ != nullptr)
      this->listener_->on_service_discovery_done(0);
    return 0;
  }
  int read_characteristic(uint16_t handle) { return GATT_ERR_NOT_CONNECTED; }
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) { return 0; }
  int read_descriptor(uint16_t handle) { return 0; }
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) { return 0; }
  int notify_characteristic(uint16_t handle, bool enable) { return 0; }
  int pair() { return GATT_ERR_NOT_CONNECTED; }
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout) {
    return 0;
  }
  GattServiceTable get_service_table() { return {}; }
  void release_services() {}

 protected:
  RecordingSink *listener_{nullptr};
};

static_assert(BLEGattConnectionContract<MinimalConnection, RecordingSink>,
              "a minimal backend must satisfy the contract the alias asserts");

TEST(BleGattClientContract, MinimalImplementerCompilesAndRoutesEvents) {
  MinimalConnection connection;
  RecordingSink listener;
  connection.set_listener(&listener);
  EXPECT_EQ(connection.connect(0xAABBCCDDEEFFULL, 0), 0);
  EXPECT_TRUE(listener.connected_);
  EXPECT_EQ(connection.discover_services(), 0);
  EXPECT_EQ(listener.discovery_error_, 0);
  EXPECT_EQ(connection.read_characteristic(1), GATT_ERR_NOT_CONNECTED);

  // A default table is empty and safe to walk.
  GattServiceTable table = connection.get_service_table();
  EXPECT_EQ(table.service_count, 0);
  EXPECT_EQ(table.characteristic_count, 0);
  EXPECT_EQ(table.descriptor_count, 0);
}

}  // namespace esphome::ble_device_base::testing
