from math import log

import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_B_CONSTANT
import esphome.config_validation as cv
from esphome.const import (
    CONF_REFERENCE_RESISTANCE,
    CONF_REFERENCE_TEMPERATURE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import CONF_BQ25820_ID, BQ25820BaseSchema

DEPENDENCIES = ["bq25820"]

CONF_VBAT = "vbat"
CONF_VSYS = "vsys"
CONF_VAC = "vac"
CONF_IBAT = "ibat"
CONF_IAC = "iac"
CONF_TEMP_IN_C = "temp_in_c"
# The resistor from REGN to TS, above the NTC thermistor from TS to PGND
CONF_RESISTOR = "resistor"
# In °C; at absolute zero ntc_calibration() divides by zero
validate_reference_temperature = cv.All(
    cv.temperature, cv.Range(min=-sensor.ZERO_POINT, min_included=False)
)

_VOLTAGE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)
_CURRENT_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_AMPERE,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_CURRENT,
    state_class=STATE_CLASS_MEASUREMENT,
)

_SENSOR_KEYS = (CONF_VBAT, CONF_VSYS, CONF_VAC, CONF_IBAT, CONF_IAC, CONF_TEMP_IN_C)


def ntc_calibration(config: ConfigType) -> tuple[float, float]:
    """B-parameter fit of the TS thermistor as 1/T = a + b * ln(R_ntc / resistor)."""
    a, b, _ = sensor.ntc_calc_b_constant(config)
    return a + b * log(config[CONF_RESISTOR]), b


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VBAT): _VOLTAGE_SCHEMA,
            cv.Optional(CONF_VSYS): _VOLTAGE_SCHEMA,
            cv.Optional(CONF_VAC): _VOLTAGE_SCHEMA,
            cv.Optional(CONF_IBAT): _CURRENT_SCHEMA,
            cv.Optional(CONF_IAC): _CURRENT_SCHEMA,
            # The defaults are the 10k B3435 NTC with a 10k resistor the TS thresholds are rated for
            cv.Optional(CONF_TEMP_IN_C): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(
                        CONF_B_CONSTANT, default=3435
                    ): cv.positive_not_null_float,
                    cv.Optional(CONF_REFERENCE_RESISTANCE, default="10kΩ"): cv.All(
                        cv.resistance, cv.positive_not_null_float
                    ),
                    cv.Optional(
                        CONF_REFERENCE_TEMPERATURE, default="25°C"
                    ): validate_reference_temperature,
                    cv.Optional(CONF_RESISTOR, default="10kΩ"): cv.All(
                        cv.resistance, cv.positive_not_null_float
                    ),
                }
            ),
        }
    ).extend(BQ25820BaseSchema),
    cv.has_at_least_one_key(*_SENSOR_KEYS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    cg.add_define("USE_BQ25820_SENSOR")
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VBAT, hub.set_vbat_sensor)
    await sensors(CONF_VSYS, hub.set_vsys_sensor)
    await sensors(CONF_VAC, hub.set_vac_sensor)
    await sensors(CONF_IBAT, hub.set_ibat_sensor)
    await sensors(CONF_IAC, hub.set_iac_sensor)
    if await sensors(CONF_TEMP_IN_C, hub.set_temp_in_c_sensor) is not None:
        cg.add(hub.set_ntc_calibration(*ntc_calibration(config[CONF_TEMP_IN_C])))
