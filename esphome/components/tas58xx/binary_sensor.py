import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import FILTER_SOURCE_FILES  # noqa: F401  pylint: disable=unused-import
from .audio_dac import CONF_TAS58XX_ID, DAC_TAS5825M, TAS58xx, tas58xx_ns

CONF_HAVE_FAULT = "have_fault"

# Faults common to TAS5805M and TAS5825M
COMMON_FAULT_SENSORS = (
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
    "over_temp_warning",  # tas582x OTW Level 3 - use tas5805 naming
)
# Additional faults available on TAS5825M
TAS5825M_ONLY_FAULT_SENSORS = (
    "load_eeprom_error",
    "right_channel_cbc_over_current",
    "left_channel_cbc_over_current",
    "left_channel_cbc_over_current_warning",
    "right_channel_cbc_over_current_warning",
    "over_temp_146c_warning",  # tas582x OTW Level 4
    # "over_temp_122c_warning",  # tas582x OTW Level 2 - not currently included
    # "over_temp_112c_warning",  # tas582x OTW Level 1 - not currently included
)

# Each name matches a FaultSensor value in tas58xx.h
FAULT_SENSORS = COMMON_FAULT_SENSORS + TAS5825M_ONLY_FAULT_SENSORS

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


def _final_validate(config: ConfigType) -> ConfigType:
    fconf = fv.full_config.get()
    hub_path = fconf.get_path_for_id(config[CONF_TAS58XX_ID])
    hub_conf = fconf.get_config_for_path(hub_path[:-1])

    if hub_conf[CONF_MODEL] == DAC_TAS5825M:
        return config

    unsupported = sorted(frozenset(TAS5825M_ONLY_FAULT_SENSORS).intersection(config))
    if unsupported:
        raise cv.Invalid(
            f"binary_sensor platform tas58xx: '{', '.join(unsupported)}' is only available for 'model: {DAC_TAS5825M}' "
            f"- Remove from YAML for 'model: {hub_conf[CONF_MODEL]}'"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


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
