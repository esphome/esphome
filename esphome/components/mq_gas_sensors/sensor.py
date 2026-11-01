"""``mq_gas_sensors`` - ESPHome sensor platform for MQ gas sensors.

One entry configures one MQ sensor (see ``coefficients.py`` for the supported
types). The analog signal comes from an existing voltage sampler (``voltage:``,
e.g. an ``adc`` sensor).
"""

import logging

import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_DELAY,
    CONF_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

from . import (
    CONF_GAS,
    CONF_R0,
    CONF_RATIO_IN_CLEAN_AIR,
    CONF_RL,
    CONF_SAMPLE_COUNT,
    CONF_SAMPLE_INTERVAL,
    CONF_SENSOR_TYPE,
    CONF_VCC,
    CONF_VOLTAGE_MULTIPLIER,
    REGRESSION_METHODS,
    MQGasSensor,
)
from .coefficients import (
    SENSOR_TYPES,
    ResolvedSensor,
    normalize_gas,
    normalize_type,
    resolve_sensor,
    sensor_types,
)

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["sensor", "voltage_sampler"]

MIN_SAMPLE_INTERVAL_MS = 16

MAX_SAMPLE_COUNT = 16


def validate_sensor_type(value: str) -> str:
    key = normalize_type(value)
    if key not in SENSOR_TYPES:
        raise cv.Invalid(
            f"'{value}' is not a supported MQ sensor type, expected one of: "
            f"{', '.join(sensor_types())}"
        )
    return key


def validate_gas(value: str) -> str:
    if not (gas := normalize_gas(value)):
        raise cv.Invalid(
            "gas must not be empty - omit it to use the default gas of the type"
        )
    return gas


CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DELAY, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLE_COUNT, default=50): cv.int_range(min=1, max=1000),
        cv.Optional(CONF_SAMPLE_INTERVAL, default="20ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=MIN_SAMPLE_INTERVAL_MS)),
        ),
    }
)


def _resolve_sensor(config: ConfigType) -> ResolvedSensor:
    """Resolve the curve of the entry.

    The one place that lists the configuration keys the curve is taken from,
    used by the validator and by the code generation alike, so the two cannot
    drift apart.
    """
    return resolve_sensor(
        config[CONF_SENSOR_TYPE],
        config.get(CONF_GAS),
        ratio_in_clean_air=config.get(CONF_RATIO_IN_CLEAN_AIR),
        rl=config.get(CONF_RL),
        vcc=config.get(CONF_VCC),
    )


def _validate_config(config: ConfigType) -> ConfigType:
    """Enforce the type/gas combination and the clean-air ratio at compile time."""
    try:
        resolved = _resolve_sensor(config)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err

    if config.get(CONF_CALIBRATION) is not None and resolved.ratio_in_clean_air <= 0.0:
        raise cv.Invalid(
            f"{resolved.label} has no published RS/R0 in clean air - set '{CONF_RATIO_IN_CLEAN_AIR}:'"
        )

    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        MQGasSensor,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:molecule",
    )
    .extend(
        {
            cv.Required(CONF_SENSOR_TYPE): cv.All(cv.string, validate_sensor_type),
            cv.Optional(CONF_GAS): cv.All(cv.string, validate_gas),
            cv.Required(CONF_VOLTAGE): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(
                CONF_VOLTAGE_MULTIPLIER, default=1.0
            ): cv.positive_not_null_float,
            cv.Optional(CONF_VCC, default=5.0): cv.positive_not_null_float,
            cv.Optional(CONF_RL, default=10.0): cv.positive_not_null_float,
            cv.Optional(CONF_R0): cv.positive_not_null_float,
            cv.Optional(CONF_RATIO_IN_CLEAN_AIR): cv.positive_not_null_float,
            cv.Optional(CONF_SAMPLE_COUNT, default=2): cv.int_range(
                min=1, max=MAX_SAMPLE_COUNT
            ),
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s")),
    cv.has_exactly_one_key(CONF_R0, CONF_CALIBRATION),
    _validate_config,
)


async def to_code(config: ConfigType) -> None:
    resolved = _resolve_sensor(config)

    var = await sensor.new_sensor(config, await cg.get_variable(config[CONF_VOLTAGE]))
    await cg.register_component(var, config)

    cg.add(var.set_type(resolved.label))
    cg.add(var.set_gas(resolved.gas))
    cg.add(var.set_a(resolved.a))
    cg.add(var.set_b(resolved.b))
    cg.add(var.set_regression_method(REGRESSION_METHODS[resolved.method]))
    cg.add(var.set_rl(resolved.rl))
    cg.add(var.set_vcc(resolved.vcc))
    cg.add(var.set_ratio_in_clean_air(resolved.ratio_in_clean_air))
    cg.add(var.set_min_ppm(resolved.min_ppm))
    cg.add(var.set_max_ppm(resolved.max_ppm))
    cg.add(var.set_voltage_multiplier(config[CONF_VOLTAGE_MULTIPLIER]))
    cg.add(var.set_samples(config[CONF_SAMPLE_COUNT]))

    if (r0 := config.get(CONF_R0)) is not None:
        cg.add(var.set_r0(r0))
    elif (calibration := config.get(CONF_CALIBRATION)) is not None:
        cg.add(
            var.set_calibration(
                calibration[CONF_DELAY],
                calibration[CONF_SAMPLE_COUNT],
                calibration[CONF_SAMPLE_INTERVAL],
            )
        )
