import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .sensor import SEN6XComponent

CONF_SEN6X_ID = "sen6x_id"
CONF_FAN_ERROR = "fan_error"
CONF_FAN_SPEED_WARNING = "fan_speed_warning"
CONF_RHT_ERROR = "rht_error"
CONF_GAS_ERROR = "gas_error"
CONF_CO2_ERROR = "co2_error"
CONF_HCHO_ERROR = "hcho_error"
CONF_PM_ERROR = "pm_error"

STATUS_SENSOR_MAP = {
    CONF_FAN_ERROR: "set_fan_error_binary_sensor",
    CONF_FAN_SPEED_WARNING: "set_fan_speed_warning_binary_sensor",
    CONF_RHT_ERROR: "set_rht_error_binary_sensor",
    CONF_GAS_ERROR: "set_gas_error_binary_sensor",
    CONF_CO2_ERROR: "set_co2_error_binary_sensor",
    CONF_HCHO_ERROR: "set_hcho_error_binary_sensor",
    CONF_PM_ERROR: "set_pm_error_binary_sensor",
}


_STATUS_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SEN6X_ID): cv.use_id(SEN6XComponent),
        **{cv.Optional(key): _STATUS_SCHEMA for key in STATUS_SENSOR_MAP},
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_SEN6X_ID])
    for key, func_name in STATUS_SENSOR_MAP.items():
        if cfg := config.get(key):
            sens = await binary_sensor.new_binary_sensor(cfg)
            cg.add(getattr(parent, func_name)(sens))
