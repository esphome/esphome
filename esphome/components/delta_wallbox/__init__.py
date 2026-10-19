import esphome.codegen as cg
from esphome.components import ble_client, time
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PASSWORD, CONF_PIN, CONF_TIME_ID
from esphome.types import ConfigType

CODEOWNERS = ["@epicRE"]
DEPENDENCIES = ["ble_client"]
DOMAIN = "delta_wallbox"
MULTI_CONF = True

CONF_DELTA_WALLBOX_ID = "delta_wallbox_id"
CONF_USER = "user"

delta_wallbox_ns = cg.esphome_ns.namespace("delta_wallbox")
DeltaWallbox = delta_wallbox_ns.class_(
    "DeltaWallbox", ble_client.BLEClientNode, cg.PollingComponent
)

CONFIG_SCHEMA = (
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(DeltaWallbox),
            cv.Required(CONF_PASSWORD): cv.string_strict,
            cv.Optional(CONF_PIN): cv.int_range(min=0, max=999999),
            cv.Optional(CONF_USER, default="admin"): cv.string_strict,
            cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.polling_component_schema("15s"))
)

async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    cg.add(var.set_user(config[CONF_USER]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    if (pin := config.get(CONF_PIN)) is not None:
        cg.add(var.set_pin(pin))

    if time_id := config.get(CONF_TIME_ID):
        time_ = await cg.get_variable(time_id)
        cg.add(var.set_time_id(time_))
