from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_COL, CONF_ROW
from esphome.core import HexInt
from esphome.types import ConfigType

from .. import CONF_SX1509_ID, KEY_DATA, KEYPAD_PUBLISHER, SX1509Component, sx1509_ns

DEPENDENCIES = ["sx1509"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_SX1509_ID): cv.use_id(SX1509Component),
        cv.Required(CONF_ROW): cv.int_range(min=0, max=7),
        cv.Required(CONF_COL): cv.int_range(min=0, max=7),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_SX1509_ID])
    # The key data has the column bit in the high byte and the row bit in the low byte
    key = HexInt((1 << (config[CONF_COL] + 8)) | (1 << config[CONF_ROW]))
    KEYPAD_PUBLISHER.add(hub, sx1509_ns.publish_key(parent_ref(var), KEY_DATA, key))
