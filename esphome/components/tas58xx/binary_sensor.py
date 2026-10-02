import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .audio_dac import CONF_TAS58XX_ID, TAS58xx, tas58xx_ns

CONF_HAVE_FAULT = "have_fault"

# Each name matches a FaultSensor value in tas58xx.h
FAULT_SENSORS = (
    "left_channel_dc_fault",
    "right_channel_dc_fault",
    "left_channel_over_current",
    "right_channel_over_current",
    "otp_crc_check",
    "bq_write_failed",
    "clock_fault",
    "pvdd_over_voltage",
    "pvdd_under_voltage",
    "over_temp_shutdown",
    "over_temp_warning",
)

FaultSensor = tas58xx_ns.enum("FaultSensor")

_FAULT_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_PROBLEM,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TAS58XX_ID): cv.use_id(TAS58xx),
        cv.Optional(CONF_HAVE_FAULT): _FAULT_SCHEMA,
        **{cv.Optional(key): _FAULT_SCHEMA for key in FAULT_SENSORS},
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_TAS58XX_ID])
    if sensor_config := config.get(CONF_HAVE_FAULT):
        sens = await binary_sensor.new_binary_sensor(sensor_config)
        cg.add(parent.set_have_fault_binary_sensor(sens))
    for key in FAULT_SENSORS:
        if sensor_config := config.get(key):
            sens = await binary_sensor.new_binary_sensor(sensor_config)
            fault = getattr(FaultSensor, f"FAULT_SENSOR_{key.upper()}")
            cg.add(parent.set_fault_binary_sensor(fault, sens))
