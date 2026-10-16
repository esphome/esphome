"""MiCS-5524 sensor platform: one entry per module, one sub-sensor per configured gas."""

import logging

import esphome.codegen as cg
from esphome.components import sensor, voltage_sampler
import esphome.config_validation as cv
from esphome.const import (
    CONF_CALIBRATION,
    CONF_DELAY,
    CONF_ID,
    CONF_VOLTAGE,
    CONF_WARMUP_TIME,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)
from esphome.types import ConfigType

from . import (
    CONF_AIR_REFERENCE,
    CONF_MAX_PPM,
    CONF_SAMPLE_COUNT,
    CONF_SAMPLE_INTERVAL,
    CONF_VCC,
    CONF_VOLTAGE_MULTIPLIER,
    MiCS5524Component,
    MiCS5524GasSensor,
)
from .coefficients import GASES

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["sensor", "voltage_sampler"]

# The main loop runs every ~16 ms, so a shorter spacing cannot sample more often.
MIN_SAMPLE_INTERVAL_MS = 16

# Conversions per reading run back to back inside one loop pass.
MAX_SAMPLE_COUNT = 16

_request_gas_slot = cg.slot_counter("MICS_5524_GAS_COUNT")


def _gas_schema(gas_key: str) -> cv.Schema:
    """Schema of one gas sub-sensor."""
    curve = GASES[gas_key]
    return sensor.sensor_schema(
        MiCS5524GasSensor,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:molecule",
    ).extend(
        {
            # A ceiling above the vendor range could never be reached.
            cv.Optional(CONF_MAX_PPM, default=curve.max_ppm): cv.float_range(
                min=0, min_included=False, max=curve.max_ppm
            )
        }
    )


CALIBRATION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DELAY, default="60s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SAMPLE_COUNT, default=10): cv.int_range(
            min=2, max=MAX_SAMPLE_COUNT
        ),
        cv.Optional(CONF_SAMPLE_INTERVAL, default="20ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=MIN_SAMPLE_INTERVAL_MS)),
        ),
    }
)


def _validate_config(config: ConfigType) -> ConfigType:
    """Reject an air reference at or above the supply; warn when there is none."""
    if (air_reference := config.get(CONF_AIR_REFERENCE)) is not None:
        # The reference is vcc - V_AO in clean air, so a larger one reads 0 ppm forever.
        if air_reference >= config[CONF_VCC]:
            raise cv.Invalid(
                f"air_reference ({air_reference:g} V) must be below vcc "
                f"({config[CONF_VCC]:g} V): it is how far the analog output sits "
                "below the supply in clean air"
            )
    elif CONF_CALIBRATION not in config:
        _LOGGER.warning(
            "no air reference configured - set 'air_reference:' or 'calibration:', "
            "the PPM values stay unknown until then"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MiCS5524Component),
            cv.Required(CONF_VOLTAGE): cv.use_id(voltage_sampler.VoltageSampler),
            cv.Optional(
                CONF_VOLTAGE_MULTIPLIER, default=1.0
            ): cv.positive_not_null_float,
            cv.Optional(CONF_VCC, default=5.0): cv.positive_not_null_float,
            cv.Optional(CONF_AIR_REFERENCE): cv.positive_not_null_float,
            cv.Optional(CONF_SAMPLE_COUNT, default=4): cv.int_range(
                min=1, max=MAX_SAMPLE_COUNT
            ),
            cv.Optional(
                CONF_WARMUP_TIME, default="3min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_CALIBRATION): CALIBRATION_SCHEMA,
            **{cv.Optional(gas_key): _gas_schema(gas_key) for gas_key in GASES},
        }
    ).extend(cv.polling_component_schema("30s")),
    cv.has_at_least_one_key(*GASES),
    cv.has_at_most_one_key(CONF_AIR_REFERENCE, CONF_CALIBRATION),
    _validate_config,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], await cg.get_variable(config[CONF_VOLTAGE]))
    await cg.register_component(var, config)

    cg.add(var.set_vcc(config[CONF_VCC]))
    cg.add(var.set_voltage_multiplier(config[CONF_VOLTAGE_MULTIPLIER]))
    cg.add(var.set_samples(config[CONF_SAMPLE_COUNT]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))

    if (air_reference := config.get(CONF_AIR_REFERENCE)) is not None:
        cg.add(var.set_air_reference(air_reference))
    elif (calibration := config.get(CONF_CALIBRATION)) is not None:
        cg.add(
            var.set_calibration(
                calibration[CONF_DELAY],
                calibration[CONF_SAMPLE_COUNT],
                calibration[CONF_SAMPLE_INTERVAL],
            )
        )

    sensors = sensor.sub_sensors(config)
    for gas_key, curve in GASES.items():
        if (gas_config := config.get(gas_key)) is None:
            continue
        _request_gas_slot(str(var))
        await sensors(
            gas_key,
            var.register_gas_sensor,
            curve.threshold,
            curve.gain,
            curve.min_ppm,
            gas_config[CONF_MAX_PPM],
        )
