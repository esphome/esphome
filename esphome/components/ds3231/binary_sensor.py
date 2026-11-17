import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_DS3231_ID, DS3231Component

DEPENDENCIES = ["ds3231"]

CONF_ALARM_1 = "alarm_1"
CONF_ALARM_2 = "alarm_2"
CONF_OSCILLATOR_STOPPED = "oscillator_stopped"

ICON_ALARM = "mdi:alarm"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
            cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
            cv.Optional(CONF_ALARM_1): binary_sensor.binary_sensor_schema(
                icon=ICON_ALARM
            ),
            cv.Optional(CONF_ALARM_2): binary_sensor.binary_sensor_schema(
                icon=ICON_ALARM
            ),
            cv.Optional(CONF_OSCILLATOR_STOPPED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_ALARM_1, CONF_ALARM_2, CONF_OSCILLATOR_STOPPED),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DS3231_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_ALARM_1, hub.set_alarm_1_binary_sensor)
    await binary_sensors(CONF_ALARM_2, hub.set_alarm_2_binary_sensor)
    await binary_sensors(
        CONF_OSCILLATOR_STOPPED, hub.set_oscillator_stopped_binary_sensor
    )
    if CONF_ALARM_1 in config or CONF_ALARM_2 in config:
        cg.add(hub.set_monitor_alarms(True))
