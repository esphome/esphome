import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_AQI
import esphome.config_validation as cv
from esphome.const import (
    CONF_COMPENSATION,
    CONF_ECO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_TVOC,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ICON_CHEMICAL_WEAPON,
    ICON_MOLECULE_CO2,
    ICON_RADIATOR,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@vincentscode", "@latonita"]
DOMAIN = "ens160_base"

ens160_ns = cg.esphome_ns.namespace("ens160_base")


CONFIG_SCHEMA_BASE = cv.Schema(
    {
        cv.Optional(CONF_ECO2): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_MILLION,
            icon=ICON_MOLECULE_CO2,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_CARBON_DIOXIDE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TVOC): sensor.sensor_schema(
            unit_of_measurement=UNIT_PARTS_PER_BILLION,
            icon=ICON_RADIATOR,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_AQI): sensor.sensor_schema(
            icon=ICON_CHEMICAL_WEAPON,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_AQI,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COMPENSATION): cv.Schema(
            {
                cv.Required(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
                cv.Required(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
            }
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code_base(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ECO2, var.set_co2_sensor)
    await sensors(CONF_TVOC, var.set_tvoc_sensor)
    await sensors(CONF_AQI, var.set_aqi_sensor)

    if compensation_config := config.get(CONF_COMPENSATION):
        await cg.add_variable(
            var.set_temperature, compensation_config[CONF_TEMPERATURE]
        )
        await cg.add_variable(var.set_humidity, compensation_config[CONF_HUMIDITY])

    return var
