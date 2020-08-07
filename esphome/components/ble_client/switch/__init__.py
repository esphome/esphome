import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, ble_client
from esphome.const import CONF_ICON, CONF_ID, CONF_INVERTED, ICON_BLUETOOTH
from .. import ble_client_ns

BleClientSwitch = ble_client_ns.class_('BleClientSwitch', switch.Switch, cg.Component,
                                       ble_client.BLEClientNode)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BleClientSwitch),
    cv.Optional(CONF_INVERTED): cv.invalid("BLE client switches do not support inverted mode!"),
    cv.Optional(CONF_ICON, default=ICON_BLUETOOTH): switch.icon,
}).extend(ble_client.BLE_CLIENT_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield ble_client.register_ble_node(var, config)
