import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_AMBIENT_PRESSURE_COMPENSATION,
    CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE,
    CONF_CO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_MEASUREMENT_MODE,
    CONF_TEMPERATURE,
    CONF_TEMPERATURE_SOURCE,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    ICON_MOLECULE_CO2,
    ICON_THERMOMETER,
    ICON_WATER_PERCENT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PARTS_PER_MILLION,
    UNIT_PERCENT,
)

CODEOWNERS = ["@will-tm"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

stcc4_ns = cg.esphome_ns.namespace("stcc4")
STCC4Component = stcc4_ns.class_(
    "STCC4Component", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

MeasurementMode = stcc4_ns.enum("MeasurementMode")
CONF_HUMIDITY_SOURCE = "humidity_source"

MEASUREMENT_MODE_OPTIONS = {
    "continuous": MeasurementMode.CONTINUOUS,
    "single_shot": MeasurementMode.SINGLE_SHOT,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(STCC4Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                icon=ICON_WATER_PERCENT,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE_SOURCE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY_SOURCE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION): cv.pressure,
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE): cv.use_id(
                sensor.Sensor
            ),
            cv.Optional(CONF_MEASUREMENT_MODE, default="continuous"): cv.enum(
                MEASUREMENT_MODE_OPTIONS, lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x64))
)

SENSOR_MAP = {
    CONF_CO2: "set_co2_sensor",
    CONF_TEMPERATURE: "set_temperature_sensor",
    CONF_HUMIDITY: "set_humidity_sensor",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, func_name in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, func_name)(sens))

    if CONF_TEMPERATURE_SOURCE in config:
        sens = await cg.get_variable(config[CONF_TEMPERATURE_SOURCE])
        cg.add(var.set_temperature_source(sens))

    if CONF_HUMIDITY_SOURCE in config:
        sens = await cg.get_variable(config[CONF_HUMIDITY_SOURCE])
        cg.add(var.set_humidity_source(sens))

    if CONF_AMBIENT_PRESSURE_COMPENSATION in config:
        cg.add(
            var.set_ambient_pressure_compensation(
                config[CONF_AMBIENT_PRESSURE_COMPENSATION]
            )
        )

    if CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE in config:
        sens = await cg.get_variable(config[CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE])
        cg.add(var.set_ambient_pressure_source(sens))

    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))
