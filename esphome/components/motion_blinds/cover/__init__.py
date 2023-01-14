import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, ble_client
from esphome.const import CONF_ID

CODEOWNERS = ["@NicolaVerbeeck"]
DEPENDENCIES = ["ble_client"]
AUTO_LOAD = ["motion_blinds", "sensor"]

CONF_INVERT_POSITION = "invert_position"

motionblinds_ns = cg.esphome_ns.namespace("motion_blinds")
MotionBlindsComponent = motionblinds_ns.class_(
    "MotionBlindsComponent", cover.Cover, ble_client.BLEClientNode, cg.Component
)

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MotionBlindsComponent),
            cv.Optional(CONF_INVERT_POSITION, default=False): cv.boolean,
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_invert_position(config[CONF_INVERT_POSITION]))
    yield cg.register_component(var, config)
    yield cover.register_cover(var, config)
    yield ble_client.register_ble_node(var, config)
