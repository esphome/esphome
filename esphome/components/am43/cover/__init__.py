import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, ble_client
from esphome.const import CONF_ID, CONF_PIN

CODEOWNERS = ["@buxtronix"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["am43", "sensor"]

CONF_INVERT_POSITION = "invert_position"

am43_ns = cg.esphome_ns.namespace("am43")
Am43Component = am43_ns.class_(
    "Am43Component", cover.Cover, ble_client.BLEClientNode, cg.Component
)

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(Am43Component),
            cv.Optional(CONF_PIN, default=8888): cv.int_range(min=0, max=0xFFFF),
            cv.Optional(CONF_INVERT_POSITION, default=False): cv.boolean,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_invert_position(config[CONF_INVERT_POSITION]))
    yield cg.register_component(var, config)
    yield cover.register_cover(var, config)
    yield ble_client.register_ble_node(var, config)
