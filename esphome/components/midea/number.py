import esphome.codegen as cg
from esphome.components import number
from esphome.components.climate import CONF_TARGET_HUMIDITY
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_HUMIDITY, ICON_FAN, UNIT_PERCENT
from esphome.types import ConfigType

from .climate import CONF_MIDEA_ID, AirConditioner

CODEOWNERS = ["@dudanov"]

CONF_FAN_SPEED = "fan_speed"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MIDEA_ID): cv.use_id(AirConditioner),
        # Manual fan speed in percent, for units with stepless fan control
        cv.Optional(CONF_FAN_SPEED): number.number_schema(
            number.ApplyNumber,
            icon=ICON_FAN,
            unit_of_measurement=UNIT_PERCENT,
        ),
        # Humidity setpoint of the dry modes
        cv.Optional(CONF_TARGET_HUMIDITY): number.number_schema(
            number.ApplyNumber,
            device_class=DEVICE_CLASS_HUMIDITY,
            unit_of_measurement=UNIT_PERCENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MIDEA_ID])
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_FAN_SPEED,
        hub.set_fan_speed_number,
        min_value=1,
        max_value=100,
        step=1,
        apply="control_fan_speed(value)",
    )
    await numbers(
        CONF_TARGET_HUMIDITY,
        hub.set_target_humidity_number,
        min_value=30,
        max_value=90,
        step=1,
        apply="control_target_humidity(value)",
    )
