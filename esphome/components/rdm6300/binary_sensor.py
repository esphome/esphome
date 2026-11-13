from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor, rdm6300
import esphome.config_validation as cv
from esphome.const import CONF_UID
from esphome.types import ConfigType

from . import CARDS_PUBLISHER, TAG, rdm6300_ns

DEPENDENCIES = ["rdm6300"]

CONF_RDM6300_ID = "rdm6300_id"

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    binary_sensor.BinarySensorInitiallyOff
).extend(
    {
        cv.GenerateID(CONF_RDM6300_ID): cv.use_id(rdm6300.RDM6300Component),
        cv.Required(CONF_UID): cv.uint32_t,
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_RDM6300_ID])
    CARDS_PUBLISHER.add(
        hub, rdm6300_ns.publish_card(parent_ref(var), TAG, config[CONF_UID])
    )
