from collections.abc import Callable
from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MODEL,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_COMPENSATION,
    CONF_THRESHOLD,
    CONF_TYPE,
    DEVICE_CLASS_CARBON_MONOXIDE,
    ICON_GAS_CYLINDER,
    ICON_MOLECULE_CO,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@tzabbi"]
DOMAIN = "dfrobot_sen0465_base"

CONF_ALARM = "alarm"
CONF_CONCENTRATION = "concentration"

dfrobot_sen0465_base_ns = cg.esphome_ns.namespace("dfrobot_sen0465_base")
DFRobotSEN0465Component = dfrobot_sen0465_base_ns.class_(
    "DFRobotSEN0465Component", cg.PollingComponent
)
AlarmType = dfrobot_sen0465_base_ns.enum("AlarmType", is_class=True)
ALARM_TYPES = AlarmType.options("LOW", "HIGH", prefix="ALARM_TYPE_")

# One piece of the temperature compensation from the DFRobot_MultiGasSensor library:
# for low < T <= high, compensated = raw / (a * T + b) - (c * T + d)
CompensationRange = tuple[int, int, float, float, float, float]


@dataclass(frozen=True)
class Model:
    """One gas probe of the series."""

    gas: str
    gas_type: int  # Gas type code the sensor reports
    unit: str
    # Also the resolution of the alarm threshold the sensor expects
    accuracy_decimals: int
    compensation: tuple[CompensationRange, ...] = ()
    device_class: str = cv.UNDEFINED
    icon: str = ICON_GAS_CYLINDER


MODELS: dict[str, Model] = {
    "SEN0465": Model("O2", 0x05, UNIT_PERCENT, 1),
    "SEN0466": Model(
        "CO",
        0x04,
        UNIT_PARTS_PER_MILLION,
        0,
        (
            (-20, 20, 0.005, 0.9, 0, 0),
            (20, 40, 0.005, 0.9, 0.3, -6),
        ),
        device_class=DEVICE_CLASS_CARBON_MONOXIDE,
        icon=ICON_MOLECULE_CO,
    ),
    "SEN0467": Model(
        "H2S",
        0x03,
        UNIT_PARTS_PER_MILLION,
        0,
        (
            (-20, 20, 0.005, 0.92, 0, 0),
            (20, 60, 0.015, -0.3, 0, 0),
        ),
    ),
    "SEN0468": Model(
        "Cl2",
        0x31,
        UNIT_PARTS_PER_MILLION,
        1,
        (
            (-20, 0, 0.015, 1.1, -0.0025, 0),
            (0, 20, 0, 1.1, 0.005, 0),
            (20, 40, 0, 1.1, -0.005, 0.3),
        ),
    ),
    "SEN0469": Model(
        "NH3",
        0x02,
        UNIT_PARTS_PER_MILLION,
        0,
        (
            (-20, 0, 0.006, 0.95, -0.006, 0.25),
            (0, 20, 0.006, 0.95, -0.012, 0.25),
            (20, 40, 0.005, 1.08, -0.1, 2),
        ),
    ),
    "SEN0470": Model(
        "SO2",
        0x2B,
        UNIT_PARTS_PER_MILLION,
        1,
        (
            (-40, 40, 0.006, 0.95, 0, 0),
            (40, 60, 0.006, 0.95, 0.05, -2),
        ),
    ),
    "SEN0471": Model(
        "NO2",
        0x2C,
        UNIT_PARTS_PER_MILLION,
        1,
        (
            (-20, 0, 0.005, 0.9, -0.0025, 0.005),
            (0, 20, 0.005, 0.9, 0.005, 0.005),
            (20, 40, 0.005, 0.9, 0.0025, 0.1),
        ),
    ),
    "SEN0472": Model(
        "O3",
        0x2A,
        UNIT_PARTS_PER_MILLION,
        1,
        (
            (-20, 0, 0.015, 1.1, 0, 0.05),
            (0, 20, 0, 1.1, 0.01, 0),
            (20, 40, 0, 1.1, -0.005, 0.3),
        ),
    ),
    # The library's H2 compensation divides by (0.74 * T + 0.007) below 20 °C, which gives
    # nonsense results, so it is not offered.
    "SEN0473": Model("H2", 0x06, UNIT_PARTS_PER_MILLION, 0),
    "SEN0474": Model(
        "HCl",
        0x2E,
        UNIT_PARTS_PER_MILLION,
        1,
        (
            (-20, 0, 0, 1, -0.0075, -0.1),
            (0, 20, 0, 1, 0, -0.1),
            (20, 50, 0, 1, -0.01, 0.1),
        ),
    ),
    "SEN0475": Model(
        "HF",
        0x33,
        UNIT_PARTS_PER_MILLION,
        1,
        (
            (-20, 0, 0, 1, -0.0025, 0),
            (0, 20, 0, 1, 0, -0.1),
            (20, 40, 0, 1, 0.0375, -0.85),
        ),
    ),
    "SEN0476": Model(
        "PH3",
        0x45,
        UNIT_PARTS_PER_MILLION,
        1,
        ((-20, 40, 0.005, 0.9, 0, 0),),
    ),
}

# Sizes the C++ range list; the largest table entry sets it
COMPENSATION_RANGES = max(len(model.compensation) for model in MODELS.values())


def _model_schema(
    model: Model, component_class: MockObjClass, bus_schema: cv.Schema
) -> cv.Schema:
    schema = {
        cv.GenerateID(): cv.declare_id(component_class),
        cv.Optional(CONF_CONCENTRATION): sensor.sensor_schema(
            unit_of_measurement=model.unit,
            icon=model.icon,
            accuracy_decimals=model.accuracy_decimals,
            device_class=model.device_class,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(kind=sensor.TEMPERATURE),
    }
    if model.compensation:
        schema[cv.Optional(CONF_TEMPERATURE_COMPENSATION, default=False)] = cv.boolean
    # The sensor sends the threshold as a 16-bit count of its last decimal place
    max_threshold = 0xFFFF / 10**model.accuracy_decimals
    schema[cv.Optional(CONF_ALARM)] = cv.Schema(
        {
            cv.Required(CONF_THRESHOLD): cv.float_range(min=0, max=max_threshold),
            cv.Optional(CONF_TYPE, default="HIGH"): cv.enum(ALARM_TYPES, upper=True),
        }
    )
    return (
        cv.Schema(schema).extend(cv.polling_component_schema("60s")).extend(bus_schema)
    )


def dfrobot_sen0465_schema(
    component_class: MockObjClass, bus_schema: cv.Schema
) -> Callable[[ConfigType], ConfigType]:
    """Return the config schema for one bus variant, with one schema per model."""
    return cv.typed_schema(
        {
            name: _model_schema(model, component_class, bus_schema)
            for name, model in MODELS.items()
        },
        key=CONF_MODEL,
        upper=True,
    )


async def to_code_base(config: ConfigType) -> MockObj:
    model = MODELS[config[CONF_MODEL]]
    var = cg.new_Pvariable(config[CONF_ID], model.gas_type, model.gas)
    await cg.register_component(var, config)

    if config.get(CONF_TEMPERATURE_COMPENSATION):
        cg.add_define("USE_DFROBOT_SEN0465_TEMPERATURE_COMPENSATION")
        cg.add_define("DFROBOT_SEN0465_COMPENSATION_RANGES", COMPENSATION_RANGES)
        for low, high, a, b, c, d in model.compensation:
            cg.add(var.add_compensation_range(low, high, a, b, c, d))

    if (alarm := config.get(CONF_ALARM)) is not None:
        decimals = model.accuracy_decimals
        threshold = round(alarm[CONF_THRESHOLD] * 10**decimals)
        cg.add(var.set_alarm(alarm[CONF_TYPE], threshold, decimals))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_CONCENTRATION, var.set_concentration_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    return var
