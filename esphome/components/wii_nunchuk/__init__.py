import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

CODEOWNERS = ["@RoganDawes"]
DEPENDENCIES = ["i2c"]
DOMAIN = "wii_nunchuk"
MULTI_CONF = True

CONF_WII_NUNCHUK_ID = "wii_nunchuk_id"

wii_nunchuk_ns = cg.esphome_ns.namespace("wii_nunchuk")
WiiNunchukComponent = wii_nunchuk_ns.class_(
    "WiiNunchukComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WiiNunchukComponent),
            cv.Optional(
                CONF_UPDATE_INTERVAL, default="50ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x52))
)

WII_NUNCHUK_CHILD_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_WII_NUNCHUK_ID): cv.use_id(WiiNunchukComponent)}
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL].total_milliseconds))
