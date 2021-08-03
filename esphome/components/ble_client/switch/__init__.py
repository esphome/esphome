import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, ble_client
from esphome.const import CONF_ICON, CONF_ID, CONF_INVERTED, ICON_BLUETOOTH
from .. import ble_client_ns

BLEClientSwitch = ble_client_ns.class_(
    "BLEClientSwitch", switch.Switch, cg.Component, ble_client.BLEClientNode
)

CONFIG_SCHEMA = (
    switch.SWITCH_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(BLEClientSwitch),
            cv.Optional(CONF_INVERTED): cv.invalid(
                "BLE client switches do not support inverted mode!"
            ),
            cv.Optional(CONF_ICON, default=ICON_BLUETOOTH): switch.icon,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    await ble_client.register_ble_node(var, config)
