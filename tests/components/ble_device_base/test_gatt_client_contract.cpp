// The GATT client contract compiles in no real build until a hub backend is
// configured; this TU pins it on the host so the header cannot rot unseen.
// The contract is a concept (BLEGattConnection is a per-platform alias), so
// the minimal backend here proves the concept stays satisfiable and routes
// events through the GattClientListener interface the way a real backend does.
#define USE_BLE_GATT_CLIENT

#include "esphome/components/ble_device_base/ble_gatt_client.h"

#include <gtest/gtest.h>

namespace esphome::ble_device_base::testing {

// Overrides only what it records; the interface's defaults cover the rest.
class RecordingListener : public GattClientListener {
 public:
  void on_connection_state(bool connected, uint16_t mtu, int error) override { this->connected_ = connected; }
  void on_service_discovery_done(int error) override { this->discovery_error_ = error; }
  void on_write_result(uint16_t handle, int error) override { this->write_handle_ = handle; }

  bool connected_{false};
  int discovery_error_{0};
  uint16_t write_handle_{0};
};

class MinimalConnection {
 public:
  void set_listener(GattClientListener *listener) { this->listener_ = listener; }

  int connect(uint64_t address, uint8_t addr_type) {
    this->listener_->on_connection_state(true, 517, 0);
    return 0;
  }
  bool cancel_gatt_disconnect() { return false; }
  int gatt_disconnect() { return 0; }
  int discover_services() {
    this->listener_->on_service_discovery_done(0);
    return 0;
  }
  int read_characteristic(uint16_t handle) { return GATT_ERR_NOT_CONNECTED; }
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
    this->listener_->on_write_result(handle, 0);
    return 0;
  }
  int read_descriptor(uint16_t handle) { return 0; }
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) { return 0; }
  int notify_characteristic(uint16_t handle, bool enable) { return 0; }
  int pair() { return GATT_ERR_NOT_CONNECTED; }
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout) {
    return 0;
  }
  GattServiceTable get_service_table() { return {}; }
  void release_services() {}
  void set_connection_type(ConnectionType ct) {}

 protected:
  GattClientListener *listener_{nullptr};
};

static_assert(BLEGattConnectionContract<MinimalConnection>,
              "a minimal backend must satisfy the contract the alias asserts");

TEST(BleGattClientContract, MinimalImplementerCompilesAndRoutesEvents) {
  MinimalConnection connection;
  RecordingListener listener;
  connection.set_listener(&listener);
  EXPECT_EQ(connection.connect(0xAABBCCDDEEFFULL, 0), 0);
  EXPECT_TRUE(listener.connected_);
  EXPECT_EQ(connection.discover_services(), 0);
  EXPECT_EQ(listener.discovery_error_, 0);
  EXPECT_EQ(connection.read_characteristic(1), GATT_ERR_NOT_CONNECTED);
  EXPECT_EQ(connection.write_characteristic(7, nullptr, 0, true), 0);
  EXPECT_EQ(listener.write_handle_, 7);

  // A default table is empty and safe to walk.
  GattServiceTable table = connection.get_service_table();
  EXPECT_EQ(table.service_count, 0);
  EXPECT_EQ(table.characteristic_count, 0);
  EXPECT_EQ(table.descriptor_count, 0);
}

}  // namespace esphome::ble_device_base::testing
