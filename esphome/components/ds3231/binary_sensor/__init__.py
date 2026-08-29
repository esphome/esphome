import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .. import CONF_DS3231_ID, DS3231Component, ds3231_ns

DEPENDENCIES = ["ds3231"]

CONF_ALARM_1 = "alarm_1"
CONF_ALARM_2 = "alarm_2"
CONF_OSCILLATOR_STOPPED = "oscillator_stopped"

DS3231BinarySensor = ds3231_ns.class_(
    "DS3231BinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(DS3231Component),
)

_ALARM_SCHEMA = binary_sensor.binary_sensor_schema(
    DS3231BinarySensor,
    device_class=DEVICE_CLASS_PROBLEM,
)

_OSC_SCHEMA = binary_sensor.binary_sensor_schema(
    DS3231BinarySensor,
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
        cv.Optional(CONF_ALARM_1): _ALARM_SCHEMA,
        cv.Optional(CONF_ALARM_2): _ALARM_SCHEMA,
        cv.Optional(CONF_OSCILLATOR_STOPPED): _OSC_SCHEMA,
    }
).add_extra(
    cv.has_at_least_one_key(CONF_ALARM_1, CONF_ALARM_2, CONF_OSCILLATOR_STOPPED)
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_DS3231_ID])

    for key, setter in (
        (CONF_ALARM_1, "set_alarm_1_binary_sensor"),
        (CONF_ALARM_2, "set_alarm_2_binary_sensor"),
        (CONF_OSCILLATOR_STOPPED, "set_oscillator_stopped_binary_sensor"),
    ):
        if (conf := config.get(key)) is not None:
            var = await binary_sensor.new_binary_sensor(conf)
            await cg.register_parented(var, config[CONF_DS3231_ID])
            cg.add(getattr(parent, setter)(var))
