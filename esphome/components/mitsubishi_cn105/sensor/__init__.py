import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_KILOWATT_HOURS,
    UNIT_WATT,
)
from esphome.types import ConfigType

from .. import (
    CONF_MITSUBISHI_CN105_ID,
    MITSUBISHI_CN105_DEVICE_SCHEMA,
    MitsubishiCN105Component,
    mitsubishi_ns,
    register_mitsubishi_cn105_device,
)

DEPENDENCIES = ["mitsubishi_cn105"]

CONF_OUTSIDE_AIR_TEMPERATURE = "outside_air_temperature"
CONF_INPUT_POWER = "input_power"
CONF_ENERGY = "energy"

MitsubishiCN105Sensor = mitsubishi_ns.class_(
    "MitsubishiCN105Sensor",
    sensor.Sensor,
    cg.Component,
    cg.Parented.template(MitsubishiCN105Component),
)
MitsubishiCN105SensorKind = mitsubishi_ns.enum("MitsubishiCN105SensorKind", is_class=True)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_OUTSIDE_AIR_TEMPERATURE): sensor.sensor_schema(
            MitsubishiCN105Sensor,
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:thermometer",
        ),
        cv.Optional(CONF_INPUT_POWER): sensor.sensor_schema(
            MitsubishiCN105Sensor,
            unit_of_measurement=UNIT_WATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
            icon="mdi:flash",
        ),
        cv.Optional(CONF_ENERGY): sensor.sensor_schema(
            MitsubishiCN105Sensor,
            unit_of_measurement=UNIT_KILOWATT_HOURS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_ENERGY,
            state_class=STATE_CLASS_TOTAL_INCREASING,
            icon="mdi:lightning-bolt",
        ),
    }
).extend(MITSUBISHI_CN105_DEVICE_SCHEMA)


async def _sensor(config: ConfigType, key: str, kind) -> None:
    if conf := config.get(key):
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await sensor.register_sensor(var, conf)
        cg.add(var.set_kind(kind))
        await register_mitsubishi_cn105_device(var, config)


async def to_code(config: ConfigType) -> None:
    await _sensor(
        config,
        CONF_OUTSIDE_AIR_TEMPERATURE,
        MitsubishiCN105SensorKind.OUTSIDE_AIR_TEMPERATURE,
    )
    await _sensor(config, CONF_INPUT_POWER, MitsubishiCN105SensorKind.INPUT_POWER)
    await _sensor(config, CONF_ENERGY, MitsubishiCN105SensorKind.ENERGY)
    if config.get(CONF_INPUT_POWER) is not None or config.get(CONF_ENERGY) is not None:
        hub = await cg.get_variable(config[CONF_MITSUBISHI_CN105_ID])
        cg.add(hub.set_energy_polling_enabled(True))
