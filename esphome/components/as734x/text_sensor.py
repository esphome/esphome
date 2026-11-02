import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_AS734X_ID, DOMAIN, AS734XComponent

DEPENDENCIES = [DOMAIN]

CONF_RGB_HEX = "rgb_hex"

ICON_PALETTE = "mdi:palette"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AS734X_ID): cv.use_id(AS734XComponent),
        cv.Optional(CONF_RGB_HEX): text_sensor.text_sensor_schema(icon=ICON_PALETTE),
    }
)


async def to_code(config: ConfigType) -> None:
    if (conf := config.get(CONF_RGB_HEX)) is None:
        return
    hub = await cg.get_variable(config[CONF_AS734X_ID])
    cg.add_define("USE_AS734X_RGB")
    sens = await text_sensor.new_text_sensor(conf)
    cg.add(hub.set_rgb_hex_text_sensor(sens))
