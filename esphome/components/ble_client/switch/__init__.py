import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, ble_client
from esphome.const import ICON_BLUETOOTH
from .. import ble_client_ns

BLEClientSwitch = ble_client_ns.class_(
    "BLEClientSwitch", switch.Switch, cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    switch.switch_schema(BLEClientSwitch, icon=ICON_BLUETOOTH, block_inverted=True)
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
