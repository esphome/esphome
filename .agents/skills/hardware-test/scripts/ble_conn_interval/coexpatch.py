"""coexpatch.py WORKTREE: hold the BLE coexistence status bit (ESP_COEX_BLE_ST_MESH_CONFIG) for every uncached connect
until search complete or the link closes; counted across slots."""

import pathlib
import sys

root = pathlib.Path(sys.argv[1]) / "esphome/components/bluetooth_connection"
h = root / "bluetooth_connection_bluedroid.h"
s = h.read_text()
s = s.replace(
    "  void set_disconnecting_();\n",
    "  void set_disconnecting_();\n  void coex_hold_(bool on);\n",
    1,
)
s = s.replace(
    "  bool cancel_open_sent_ : 1 {false};\n",
    "  bool cancel_open_sent_ : 1 {false};\n  // This slot holds the BLE coexistence priority (uncached discovery).\n  bool coex_held_ : 1 {false};\n",
    1,
)
h.write_text(s)
c = root / "bluetooth_connection_bluedroid.cpp"
s = c.read_text()
s = s.replace(
    "#include <cstring>\n", "#include <cstring>\n#include <esp_coexist.h>\n", 1
)
s = s.replace(
    "namespace esphome::bluetooth_connection {\n",
    """namespace esphome::bluetooth_connection {

extern "C" uint32_t coex_schm_status_get(uint32_t type);  // libcoexist, BLE status read back for the test log
static uint8_t coex_hold_count = 0;  // NOLINT

void BluedroidGattClient::coex_hold_(bool on) {
  if (on == this->coex_held_)
    return;
  this->coex_held_ = on;
  if (on ? coex_hold_count++ != 0 : --coex_hold_count != 0)
    return;
  esp_err_t err = on ? esp_coex_status_bit_set(ESP_COEX_ST_TYPE_BLE, ESP_COEX_BLE_ST_MESH_CONFIG)
                     : esp_coex_status_bit_clear(ESP_COEX_ST_TYPE_BLE, ESP_COEX_BLE_ST_MESH_CONFIG);
  ESP_LOGI(TAG, "[%d] COEXHOLD %s err=%d ble_status=0x%x", this->connection_index_, on ? "set" : "clear", err,
           (unsigned) coex_schm_status_get(1));
}
""",
    1,
)
old = "  this->set_state(ClientState::CONNECTING);\n  if (this->connection_type_ == ConnectionType::V3_WITHOUT_CACHE) {\n"
assert old in s
s = s.replace(old, old + "    this->coex_hold_(true);\n", 1)
old = "int BluedroidGattClient::handle_search_cmpl_(esp_gatt_status_t status) {\n"
assert old in s
s = s.replace(old, old + "  this->coex_hold_(false);\n", 1)
old = "void BluedroidGattClient::set_idle_() {\n"
s = s.replace(old, old + "  this->coex_hold_(false);\n", 1)
old = "void BluedroidGattClient::set_disconnecting_() {\n"
s = s.replace(old, old + "  this->coex_hold_(false);\n", 1)
c.write_text(s)
