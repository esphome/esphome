from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import CONF_TAG
from esphome.types import ConfigType

from .. import (
    CONF_MK2PVROUTER_ID,
    DUMP_PUBLISHER,
    DUMP_TAG,
    MK2PVROUTER_CHILD_SCHEMA,
    TAG,
    VAL,
    VALUE_PUBLISHER,
    mk2pvrouter_ns,
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(MK2PVROUTER_CHILD_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = await text_sensor.new_text_sensor(config)
    mk2pvrouter = await cg.get_variable(config[CONF_MK2PVROUTER_ID])
    tag = config[CONF_TAG]
    VALUE_PUBLISHER.add(
        mk2pvrouter,
        mk2pvrouter_ns.publish_text_sensor_value(parent_ref(var), tag, TAG, VAL),
    )
    DUMP_PUBLISHER.add(
        mk2pvrouter, mk2pvrouter_ns.dump_text_sensor(DUMP_TAG, parent_ref(var), tag)
    )
