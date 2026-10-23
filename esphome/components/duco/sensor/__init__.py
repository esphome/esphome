import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
    UNIT_SECOND,
)
from esphome.types import ConfigType

from .. import CONF_DUCO_ID, DUCO_COMPONENT_SCHEMA, duco_ns

DEPENDENCIES = ["duco"]
CODEOWNERS = ["@kokx"]

UNIT_DAYS = "d"

CONF_BYPASS = "bypass"
CONF_FILTER_REMAINING = "filter_remaining"
CONF_FLOW_LEVEL = "flow_level"
CONF_TEMPERATURE_EHA = "temperature_eha"
CONF_TEMPERATURE_ETA = "temperature_eta"
CONF_TEMPERATURE_ODA = "temperature_oda"
CONF_TEMPERATURE_SUP = "temperature_sup"
CONF_TIME_REMAINING = "time_remaining"

DucoSensor = duco_ns.class_("DucoSensor", cg.PollingComponent, sensor.Sensor)
DucoReading = duco_ns.enum("DucoReading", is_class=True)

BOX_NODE = 1


def _schema(update_interval: str = "60s", **kwargs) -> cv.Schema:
    return sensor.sensor_schema(
        DucoSensor, state_class=STATE_CLASS_MEASUREMENT, **kwargs
    ).extend(cv.polling_component_schema(update_interval))


def _node_schema(**kwargs) -> cv.All:
    """A list of sensors, one per node address."""
    return cv.ensure_list(
        _schema(**kwargs).extend({cv.Required(CONF_ADDRESS): cv.int_range(0, 68)})
    )


_TEMPERATURE = {
    "unit_of_measurement": UNIT_CELSIUS,
    "accuracy_decimals": 1,
    "device_class": DEVICE_CLASS_TEMPERATURE,
}

# key: (what is read, request argument or None for the node address, schema)
SENSORS = {
    CONF_CO2: (
        DucoReading.DUCO_READING_CO2,
        None,
        _node_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
        ),
    ),
    CONF_TEMPERATURE: (
        DucoReading.DUCO_READING_NODE_TEMPERATURE,
        None,
        _node_schema(**_TEMPERATURE),
    ),
    CONF_HUMIDITY: (
        DucoReading.DUCO_READING_HUMIDITY,
        None,
        _node_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_HUMIDITY,
        ),
    ),
    CONF_FILTER_REMAINING: (
        DucoReading.DUCO_READING_FILTER_REMAINING,
        0x30,
        _schema(
            "1200s",
            unit_of_measurement=UNIT_DAYS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
        ),
    ),
    CONF_FLOW_LEVEL: (
        DucoReading.DUCO_READING_FLOW_LEVEL,
        BOX_NODE,
        _schema(unit_of_measurement=UNIT_PERCENT, accuracy_decimals=0),
    ),
    CONF_TIME_REMAINING: (
        DucoReading.DUCO_READING_TIME_REMAINING,
        BOX_NODE,
        _schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_DURATION,
        ),
    ),
    CONF_TEMPERATURE_ODA: (
        DucoReading.DUCO_READING_BOX_TEMPERATURE,
        0x00,
        _schema(**_TEMPERATURE),
    ),
    CONF_TEMPERATURE_SUP: (
        DucoReading.DUCO_READING_BOX_TEMPERATURE,
        0x01,
        _schema(**_TEMPERATURE),
    ),
    CONF_TEMPERATURE_ETA: (
        DucoReading.DUCO_READING_BOX_TEMPERATURE,
        0x02,
        _schema(**_TEMPERATURE),
    ),
    CONF_TEMPERATURE_EHA: (
        DucoReading.DUCO_READING_BOX_TEMPERATURE,
        0x03,
        _schema(**_TEMPERATURE),
    ),
    CONF_BYPASS: (
        DucoReading.DUCO_READING_BYPASS,
        0x10,
        _schema(unit_of_measurement=UNIT_PERCENT, accuracy_decimals=0),
    ),
}

CONFIG_SCHEMA = DUCO_COMPONENT_SCHEMA.extend(
    {cv.Optional(key): schema for key, (_, _, schema) in SENSORS.items()}
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DUCO_ID])
    sensors = sensor.sub_sensors(config, parent=hub)
    for key, (reading, argument, _) in SENSORS.items():
        if (confs := config.get(key)) is None:
            continue
        for conf in confs if isinstance(confs, list) else [confs]:
            var = await sensors.create(
                conf, None, reading, conf.get(CONF_ADDRESS, argument)
            )
            await cg.register_component(var, conf)
