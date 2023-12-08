import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ECO2,
    CONF_HUMIDITY,
    CONF_ID,
    CONF_TEMPERATURE,
    CONF_TVOC,
    DEVICE_CLASS_AQI,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
    ICON_CHEMICAL_WEAPON,
    ICON_MOLECULE_CO2,
    ICON_RADIATOR,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
)

CODEOWNERS = ["@vincentscode"]
DEPENDENCIES = ["i2c"]

ens160_ns = cg.esphome_ns.namespace("ens160")
ENS160Component = ens160_ns.class_(
    "ENS160Component", cg.PollingComponent, i2c.I2CDevice, sensor.Sensor
)

CONF_AQI = "aqi"
CONF_COMPENSATION = "compensation"
UNIT_INDEX = "index"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ENS160Component),
            cv.Required(CONF_ECO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_TVOC): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_BILLION,
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_AQI): sensor.sensor_schema(
                unit_of_measurement=UNIT_INDEX,
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
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x53))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sens = await sensor.new_sensor(config[CONF_ECO2])
    cg.add(var.set_co2(sens))
    sens = await sensor.new_sensor(config[CONF_TVOC])
    cg.add(var.set_tvoc(sens))
    sens = await sensor.new_sensor(config[CONF_AQI])
    cg.add(var.set_aqi(sens))

    if CONF_COMPENSATION in config:
        compensation_config = config[CONF_COMPENSATION]
        sens = await cg.get_variable(compensation_config[CONF_TEMPERATURE])
        cg.add(var.set_temperature(sens))
        sens = await cg.get_variable(compensation_config[CONF_HUMIDITY])
        cg.add(var.set_humidity(sens))
