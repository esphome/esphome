import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
from esphome.components.const import CONF_HUMIDITY_SOURCE
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
    CONF_UPDATE_INTERVAL,
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
from esphome.types import ConfigType

CODEOWNERS = ["@j9brown"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

stcc4_ns = cg.esphome_ns.namespace("stcc4")
STCC4Component = stcc4_ns.class_(
    "STCC4Component", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

MeasurementMode = stcc4_ns.enum("MeasurementMode", is_class=True)

MEASUREMENT_MODE_OPTIONS = {
    "continuous": MeasurementMode.CONTINUOUS,
    "single_shot": MeasurementMode.SINGLE_SHOT,
}


def validate_config(config: ConfigType) -> ConfigType:
    if config[CONF_MEASUREMENT_MODE] == "continuous":
        if CONF_UPDATE_INTERVAL in config:
            raise cv.Invalid(
                "update_interval must not be specified in continuous measurement mode"
            )
    elif CONF_UPDATE_INTERVAL not in config:
        config[CONF_UPDATE_INTERVAL] = cv.update_interval("60s")
    return config


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
            cv.Inclusive(CONF_TEMPERATURE_SOURCE, "rht_compensation"): cv.use_id(
                sensor.Sensor
            ),
            cv.Inclusive(CONF_HUMIDITY_SOURCE, "rht_compensation"): cv.use_id(
                sensor.Sensor
            ),
            cv.Exclusive(
                CONF_AMBIENT_PRESSURE_COMPENSATION, "ambient_pressure_compensation"
            ): cv.All(cv.pressure, cv.float_range(min=0.4, max=1.1)),
            cv.Exclusive(
                CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE,
                "ambient_pressure_compensation",
            ): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_MEASUREMENT_MODE, default="continuous"): cv.enum(
                MEASUREMENT_MODE_OPTIONS, lower=True
            ),
            cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x64))
    .add_extra(validate_config)
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_CO2, var.set_co2_sensor)
    await sensors(CONF_TEMPERATURE, var.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, var.set_humidity_sensor)

    if (temperature_source := config.get(CONF_TEMPERATURE_SOURCE)) is not None:
        sens = await cg.get_variable(temperature_source)
        cg.add(var.set_temperature_source(sens))

    if (humidity_source := config.get(CONF_HUMIDITY_SOURCE)) is not None:
        sens = await cg.get_variable(humidity_source)
        cg.add(var.set_humidity_source(sens))

    if (
        ambient_pressure_compensation := config.get(CONF_AMBIENT_PRESSURE_COMPENSATION)
    ) is not None:
        cg.add(
            var.set_ambient_pressure_compensation(
                ambient_pressure_compensation * 1000  # convert bar to hPa
            )
        )

    if (
        ambient_pressure_compensation_source := config.get(
            CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE
        )
    ) is not None:
        sens = await cg.get_variable(ambient_pressure_compensation_source)
        cg.add(var.set_ambient_pressure_source(sens))

    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))
