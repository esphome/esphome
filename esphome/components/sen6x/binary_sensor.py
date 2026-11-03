import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .sensor import SEN6XComponent, sen6x_ns

CONF_SEN6X_ID = "sen6x_id"
CONF_FAN_ERROR = "fan_error"
CONF_FAN_SPEED_WARNING = "fan_speed_warning"
CONF_RHT_ERROR = "rht_error"
CONF_GAS_ERROR = "gas_error"
CONF_CO2_ERROR = "co2_error"
CONF_HCHO_ERROR = "hcho_error"
CONF_PM_ERROR = "pm_error"

Sen6xStatus = sen6x_ns.enum("Sen6xStatus", is_class=True)

# Each key watches one bit of the device status register
_STATUS_KINDS = (
    (CONF_FAN_ERROR, Sen6xStatus.SEN6X_STATUS_FAN_ERROR),
    (CONF_FAN_SPEED_WARNING, Sen6xStatus.SEN6X_STATUS_FAN_SPEED_WARNING),
    (CONF_RHT_ERROR, Sen6xStatus.SEN6X_STATUS_RHT_ERROR),
    (CONF_GAS_ERROR, Sen6xStatus.SEN6X_STATUS_GAS_ERROR),
    (CONF_CO2_ERROR, Sen6xStatus.SEN6X_STATUS_CO2_ERROR),
    (CONF_HCHO_ERROR, Sen6xStatus.SEN6X_STATUS_HCHO_ERROR),
    (CONF_PM_ERROR, Sen6xStatus.SEN6X_STATUS_PM_ERROR),
)

_request_status_slot = cg.slot_counter("SEN6X_STATUS_SENSOR_COUNT")

_STATUS_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SEN6X_ID): cv.use_id(SEN6XComponent),
        **{cv.Optional(key): _STATUS_SCHEMA for key, _ in _STATUS_KINDS},
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SEN6X_ID])
    for key, status in _STATUS_KINDS:
        if (conf := config.get(key)) is None:
            continue
        sens = await binary_sensor.new_binary_sensor(conf)
        _request_status_slot(str(hub))
        cg.add(hub.add_status_binary_sensor(sens, status))
