from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_BATTERY_VOLTAGE, CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import (
    ADC,
    CONF_NPM1300_ID,
    CONF_THERMISTOR_RESISTANCE,
    CONF_VBUS_CURRENT_LIMIT,
    DATA_PUBLISHER,
    DOMAIN,
    DUMP,
    DUMP_TAG,
    IBAT_SCALE,
    NPM1300_CHILD_SCHEMA,
    THERMISTOR_NONE,
    npm1300_ns,
    thermistor_beta,
)

DEPENDENCIES = ["npm1300"]

CONF_BATTERY_CURRENT = "battery_current"
CONF_BATTERY_TEMPERATURE = "battery_temperature"
CONF_DIE_TEMPERATURE = "die_temperature"
CONF_SYSTEM_VOLTAGE = "system_voltage"
CONF_VBUS_VOLTAGE = "vbus_voltage"

ADC_MAX_CODE = 1023


def _adc(msb: int, lsbs: int, shift: int) -> cg.MockObj:
    """The 10-bit result in adc[msb] and bits shift..shift+1 of adc[lsbs]."""
    return npm1300_ns.adc_code(ADC, msb, lsbs, shift)


# The decoded value and the dump name of each ADC sensor but the battery temperature, which needs the hub config.
# Voltages scale by their full scale (product specification, section ADC).
STATES = {
    CONF_BATTERY_VOLTAGE: (_adc(1, 5, 0) * (5.0 / ADC_MAX_CODE), "Battery Voltage"),
    CONF_BATTERY_CURRENT: (_adc(8, 10, 4) * IBAT_SCALE, "Battery Current"),
    # Product specification: T = 394.67 - 0.7926 * code
    CONF_DIE_TEMPERATURE: (394.67 - 0.7926 * _adc(3, 5, 4), "Die Temperature"),
    CONF_SYSTEM_VOLTAGE: (_adc(4, 5, 6) * (6.375 / ADC_MAX_CODE), "System Voltage"),
    CONF_VBUS_VOLTAGE: (_adc(9, 10, 6) * (7.5 / ADC_MAX_CODE), "VBUS Voltage"),
}


CONFIG_SCHEMA = NPM1300_CHILD_SCHEMA.extend(
    {
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            kind=sensor.VOLTAGE, accuracy_decimals=3
        ),
        cv.Optional(CONF_BATTERY_CURRENT): sensor.sensor_schema(
            kind=sensor.CURRENT, accuracy_decimals=3
        ),
        cv.Optional(CONF_BATTERY_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE
        ),
        cv.Optional(CONF_DIE_TEMPERATURE): sensor.sensor_schema(
            kind=sensor.TEMPERATURE, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_SYSTEM_VOLTAGE): sensor.sensor_schema(
            kind=sensor.VOLTAGE, accuracy_decimals=3
        ),
        cv.Optional(CONF_VBUS_VOLTAGE): sensor.sensor_schema(
            kind=sensor.VOLTAGE, accuracy_decimals=3
        ),
        cv.Optional(CONF_VBUS_CURRENT_LIMIT): sensor.sensor_schema(
            kind=sensor.CURRENT,
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


def _final_validate(config: ConfigType) -> None:
    if CONF_BATTERY_TEMPERATURE not in config:
        return
    hub = fv.get_declaring_config(config[CONF_NPM1300_ID])
    if hub.get(CONF_THERMISTOR_RESISTANCE, THERMISTOR_NONE) == THERMISTOR_NONE:
        raise cv.Invalid(
            f"'{CONF_BATTERY_TEMPERATURE}' needs '{CONF_THERMISTOR_RESISTANCE}' set to the "
            "nominal resistance of the battery thermistor on the npm1300 component",
            path=[CONF_BATTERY_TEMPERATURE],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


def _add_adc_sensor(
    hub: cg.MockObj, var: cg.MockObj, state: cg.MockObj, name: str
) -> None:
    DATA_PUBLISHER.add(hub, parent_ref(var).publish_state(state))
    DUMP.add(
        hub,
        sensor.sensor_ns.log_sensor(
            DUMP_TAG,
            "  ",
            cg.RawExpression(f'LOG_STR_LITERAL("{name}")'),
            parent_ref(var),
        ),
    )


async def to_code(config: ConfigType) -> None:
    hub_id = config[CONF_NPM1300_ID]
    hub = await cg.get_variable(hub_id)
    sensors = sensor.sub_sensors(config)
    for key, (state, name) in STATES.items():
        if (var := await sensors(key, None)) is not None:
            _add_adc_sensor(hub, var, state, name)
    if (var := await sensors(CONF_BATTERY_TEMPERATURE, None)) is not None:
        hub_config = next(c for c in CORE.config[DOMAIN] if c[CONF_ID] == hub_id)
        state = npm1300_ns.ntc_temperature(
            _adc(2, 5, 2), 1.0 / thermistor_beta(hub_config)
        )
        _add_adc_sensor(hub, var, state, "Battery Temperature")
    await sensors(CONF_VBUS_CURRENT_LIMIT, hub.set_vbus_current_limit_sensor)
