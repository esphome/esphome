import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .audio_dac import CONF_TAS5805M_ID, TAS5805M

CONF_HAVE_FAULT = "have_fault"
CONF_LEFT_CHANNEL_DC_FAULT = "left_channel_dc_fault"
CONF_RIGHT_CHANNEL_DC_FAULT = "right_channel_dc_fault"
CONF_LEFT_CHANNEL_OVER_CURRENT = "left_channel_over_current"
CONF_RIGHT_CHANNEL_OVER_CURRENT = "right_channel_over_current"
CONF_OTP_CRC_CHECK = "otp_crc_check"
CONF_BQ_WRITE_FAILED = "bq_write_failed"
CONF_CLOCK_FAULT = "clock_fault"
CONF_PVDD_OVER_VOLTAGE = "pvdd_over_voltage"
CONF_PVDD_UNDER_VOLTAGE = "pvdd_under_voltage"
CONF_OVER_TEMP_SHUTDOWN = "over_temp_shutdown"
CONF_OVER_TEMP_WARNING = "over_temp_warning"

FAULT_SENSORS = (
    CONF_HAVE_FAULT,
    CONF_LEFT_CHANNEL_DC_FAULT,
    CONF_RIGHT_CHANNEL_DC_FAULT,
    CONF_LEFT_CHANNEL_OVER_CURRENT,
    CONF_RIGHT_CHANNEL_OVER_CURRENT,
    CONF_OTP_CRC_CHECK,
    CONF_BQ_WRITE_FAILED,
    CONF_CLOCK_FAULT,
    CONF_PVDD_OVER_VOLTAGE,
    CONF_PVDD_UNDER_VOLTAGE,
    CONF_OVER_TEMP_SHUTDOWN,
    CONF_OVER_TEMP_WARNING,
)

_FAULT_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TAS5805M_ID): cv.use_id(TAS5805M),
        **{cv.Optional(key): _FAULT_SCHEMA for key in FAULT_SENSORS},
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_TAS5805M_ID])
    for key in FAULT_SENSORS:
        if sensor_config := config.get(key):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(parent, f"set_{key}_binary_sensor")(sens))
