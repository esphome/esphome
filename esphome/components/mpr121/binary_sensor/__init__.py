import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
from esphome.types import ConfigType

from .. import (
    CONF_MPR121_ID,
    CONF_RELEASE_THRESHOLD,
    CONF_TOUCH_THRESHOLD,
    MPR121Component,
    mpr121_ns,
)

DEPENDENCIES = ["mpr121"]
MPR121BinarySensor = mpr121_ns.class_("MPR121BinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(MPR121BinarySensor).extend(
    {
        cv.GenerateID(CONF_MPR121_ID): cv.use_id(MPR121Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=11),
        cv.Optional(CONF_TOUCH_THRESHOLD): cv.int_range(min=0x05, max=0x30),
        cv.Optional(CONF_RELEASE_THRESHOLD): cv.int_range(min=0x05, max=0x30),
    }
)


_register_channel = cg.slot_list("MPR121_CHANNEL_COUNT", "register_channel")


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_MPR121_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    await cg.register_parented(var, hub)

    cg.add_optional(var.set_touch_threshold, config.get(CONF_TOUCH_THRESHOLD))
    cg.add_optional(var.set_release_threshold, config.get(CONF_RELEASE_THRESHOLD))

    cg.add(_register_channel(hub, var))
