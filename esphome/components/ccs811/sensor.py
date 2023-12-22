import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor, text_sensor
from esphome.const import (
    CONF_ID,
    ICON_RADIATOR,
    ICON_RESTART,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    UNIT_PARTS_PER_BILLION,
    CONF_BASELINE,
    CONF_ECO2,
    CONF_TEMPERATURE,
    CONF_TVOC,
    CONF_HUMIDITY,
    CONF_VERSION,
    ICON_MOLECULE_CO2,
)

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


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sens = await sensor.new_sensor(config[CONF_ECO2])
    cg.add(var.set_co2(sens))
    sens = await sensor.new_sensor(config[CONF_TVOC])
    cg.add(var.set_tvoc(sens))

    if version_config := config.get(CONF_VERSION):
        sens = await text_sensor.new_text_sensor(version_config)
        cg.add(var.set_version(sens))

    if (baseline := config.get(CONF_BASELINE)) is not None:
        cg.add(var.set_baseline(baseline))

    if temperature_id := config.get(CONF_TEMPERATURE):
        sens = await cg.get_variable(temperature_id)
        cg.add(var.set_temperature(sens))
    if humidity_id := config.get(CONF_HUMIDITY):
        sens = await cg.get_variable(humidity_id)
        cg.add(var.set_humidity(sens))
