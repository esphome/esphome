import esphome.codegen as cg
from esphome.components import i2c, sensor, text_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BASELINE,
    CONF_ECO2,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_TVOC,
    CONF_VERSION,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ICON_MOLECULE_CO2,
    ICON_RADIATOR,
    ICON_RESTART,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
)
from esphome.types import ConfigType

AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@habbie"]
DEPENDENCIES = ["i2c"]

ccs811_ns = cg.esphome_ns.namespace("ccs811")
CCS811Component = ccs811_ns.class_(
    "CCS811Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CCS811Component),
            cv.Optional(CONF_ECO2): sensor.sensor_schema(
                kind=sensor.CARBON_DIOXIDE, icon=ICON_MOLECULE_CO2, accuracy_decimals=0
            ),
            cv.Optional(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
                icon=ICON_RESTART
            ),
            cv.Optional(CONF_BASELINE): cv.hex_uint16_t,
            cv.Optional(CONF_TEMPERATURE): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_HUMIDITY): cv.use_id(sensor.Sensor),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x5A))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_ECO2, var.set_co2_sensor)
    await sensors(CONF_TVOC, var.set_tvoc_sensor)

    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_VERSION, var.set_version_text_sensor)

    cg.add_optional(var.set_baseline, config.get(CONF_BASELINE))

    await cg.add_variable(var.set_temperature_sensor, config.get(CONF_TEMPERATURE))
    await cg.add_variable(var.set_humidity_sensor, config.get(CONF_HUMIDITY))
