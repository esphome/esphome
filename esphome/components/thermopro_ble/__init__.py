import esphome.codegen as cg
from esphome.components import ble_client

DOMAIN = "thermopro_ble"
CONF_THERMOPRO_BLE_ID = "thermopro_ble_id"
thermopro_ble_ns = cg.esphome_ns.namespace(DOMAIN)
ThermoProBLEClient = thermopro_ble_ns.class_(
    "ThermoProBLEClient", cg.Component, ble_client.BLEClientNode
)
