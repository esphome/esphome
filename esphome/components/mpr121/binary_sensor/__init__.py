from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import (
    CHANNELS_PUBLISHER,
    CONF_MPR121_ID,
    CONF_RELEASE_THRESHOLD,
    CONF_TOUCH_THRESHOLD,
    THRESHOLDS_PUBLISHER,
    TOUCHED,
    MPR121Component,
    mpr121_ns,
)

DEPENDENCIES = ["mpr121"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.GenerateID(CONF_MPR121_ID): cv.use_id(MPR121Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=11),
        cv.Optional(CONF_TOUCH_THRESHOLD): cv.int_range(min=0x05, max=0x30),
        cv.Optional(CONF_RELEASE_THRESHOLD): cv.int_range(min=0x05, max=0x30),
    }
)


def _final_validate(config: ConfigType) -> None:
    """Take the thresholds the channel does not set from its hub."""
    hub = fv.get_declaring_config(config[CONF_MPR121_ID])
    for key in (CONF_TOUCH_THRESHOLD, CONF_RELEASE_THRESHOLD):
        config.setdefault(key, hub[key])


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_MPR121_ID])
    channel = config[CONF_CHANNEL]
    THRESHOLDS_PUBLISHER.add(
        hub,
        hub.write_thresholds(
            channel, config[CONF_TOUCH_THRESHOLD], config[CONF_RELEASE_THRESHOLD]
        ),
    )
    CHANNELS_PUBLISHER.add(
        hub, mpr121_ns.publish_channel(parent_ref(var), TOUCHED, channel)
    )
