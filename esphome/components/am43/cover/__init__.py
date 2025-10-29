import esphome.codegen as cg
from esphome.components import ble_client, cover
import esphome.config_validation as cv
from esphome.const import CONF_PIN

CODEOWNERS = ["@buxtronix"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["am43"]

CONF_INVERT_POSITION = "invert_position"

am43_ns = cg.esphome_ns.namespace("am43")
Am43Component = am43_ns.class_(
    "Am43Component", cover.Cover, ble_client.BLEClientNode, cg.Component
)

CONFIG_SCHEMA = (
    cover.cover_schema(Am43Component)
    .extend(
        {
            cv.Optional(CONF_PIN, default=8888): cv.int_range(min=0, max=0xFFFF),
            cv.Optional(CONF_INVERT_POSITION, default=False): cv.boolean,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_invert_position(config[CONF_INVERT_POSITION]))
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)
