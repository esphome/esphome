from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_KEY
from esphome.types import ConfigType

from ..display import CONF_TM1638_ID, KEYS_PUBLISHER, TM1638Component, tm1638_ns

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_TM1638_ID): cv.use_id(TM1638Component),
        cv.Required(CONF_KEY): cv.int_range(min=0, max=15),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_TM1638_ID])
    KEYS_PUBLISHER.add(
        hub,
        tm1638_ns.publish_key(
            parent_ref(var), KEYS_PUBLISHER.param("keys"), config[CONF_KEY]
        ),
    )
