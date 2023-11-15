from math import log

import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import sensor, resistance_sampler
from esphome.const import (
    CONF_CALIBRATION,
    CONF_REFERENCE_RESISTANCE,
    CONF_REFERENCE_TEMPERATURE,
    CONF_SENSOR,
    CONF_TEMPERATURE,
    CONF_VALUE,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

AUTO_LOAD = ["resistance_sampler"]

ntc_ns = cg.esphome_ns.namespace("ntc")
NTC = ntc_ns.class_("NTC", cg.Component, sensor.Sensor)

CONF_B_CONSTANT = "b_constant"
CONF_A = "a"
CONF_B = "b"
CONF_C = "c"
ZERO_POINT = 273.15


def validate_calibration_parameter(value):
    if isinstance(value, dict):
        return cv.Schema(
            {
                cv.Required(CONF_TEMPERATURE): cv.float_,
                cv.Required(CONF_VALUE): cv.float_,
            }
        )(value)

    value = cv.string(value)
    parts = value.split("->")
    if len(parts) != 2:
        raise cv.Invalid("Calibration parameter must be of form 3000 -> 23°C")
    voltage = cv.resistance(parts[0].strip())
    temperature = cv.temperature(parts[1].strip())
    return validate_calibration_parameter(
        {
            CONF_TEMPERATURE: temperature,
            CONF_VALUE: voltage,
        }
    )


def calc_steinhart_hart(value):
    r1 = value[0][CONF_VALUE]
    r2 = value[1][CONF_VALUE]
    r3 = value[2][CONF_VALUE]
    t1 = value[0][CONF_TEMPERATURE] + ZERO_POINT
    t2 = value[1][CONF_TEMPERATURE] + ZERO_POINT
    t3 = value[2][CONF_TEMPERATURE] + ZERO_POINT

    l1 = log(r1)
    l2 = log(r2)
    l3 = log(r3)

    y1 = 1 / t1
    y2 = 1 / t2
    y3 = 1 / t3

    g2 = (y2 - y1) / (l2 - l1)
    g3 = (y3 - y1) / (l3 - l1)

    c = (g3 - g2) / (l3 - l2) * 1 / (l1 + l2 + l3)
    b = g2 - c * (l1 * l1 + l1 * l2 + l2 * l2)
    a = y1 - (b + l1 * l1 * c) * l1
    return a, b, c


def calc_b(value):
    beta = value[CONF_B_CONSTANT]
    t0 = value[CONF_REFERENCE_TEMPERATURE] + ZERO_POINT
    r0 = value[CONF_REFERENCE_RESISTANCE]

    a = (1 / t0) - (1 / beta) * log(r0)
    b = 1 / beta
    c = 0

    return a, b, c


def process_calibration(value):
    if isinstance(value, dict):
        value = cv.Schema(
            {
                cv.Required(CONF_B_CONSTANT): cv.float_,
                cv.Required(CONF_REFERENCE_TEMPERATURE): cv.temperature,
                cv.Required(CONF_REFERENCE_RESISTANCE): cv.resistance,
            }
        )(value)
        a, b, c = calc_b(value)
    elif isinstance(value, list):
        if len(value) != 3:
            raise cv.Invalid(
                "Steinhart–Hart Calibration must consist of exactly three values"
            )
        value = cv.Schema([validate_calibration_parameter])(value)
        a, b, c = calc_steinhart_hart(value)
    else:
        raise cv.Invalid(
            f"Calibration parameter accepts either a list for steinhart-hart calibration, or mapping for b-constant calibration, not {type(value)}"
        )

    return {
        CONF_A: a,
        CONF_B: b,
        CONF_C: c,
    }


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        NTC,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Required(CONF_SENSOR): cv.use_id(resistance_sampler.ResistanceSampler),
            cv.Required(CONF_CALIBRATION): process_calibration,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    sens = await cg.get_variable(config[CONF_SENSOR])
    cg.add(var.set_sensor(sens))
    calib = config[CONF_CALIBRATION]
    cg.add(var.set_a(calib[CONF_A]))
    cg.add(var.set_b(calib[CONF_B]))
    cg.add(var.set_c(calib[CONF_C]))
