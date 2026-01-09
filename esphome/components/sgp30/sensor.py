import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_BASELINE,
    CONF_COMPENSATION,
    CONF_ECO2,
    CONF_ID,
    CONF_STORE_BASELINE,
    CONF_TEMPERATURE_SOURCE,
    CONF_TVOC,
    DEVICE_CLASS_CARBON_DIOXIDE,
    DEVICE_CLASS_VOLATILE_ORGANIC_COMPOUNDS_PARTS,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_MOLECULE_CO2,
    ICON_RADIATOR,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_BILLION,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]

sgp30_ns = cg.esphome_ns.namespace("sgp30")
SGP30Component = sgp30_ns.class_(
    "SGP30Component", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)

CONF_ECO2_BASELINE = "eco2_baseline"
CONF_TVOC_BASELINE = "tvoc_baseline"
CONF_UPTIME = "uptime"
CONF_HUMIDITY_SOURCE = "humidity_source"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SGP30Component),
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
            cv.Optional(CONF_ECO2_BASELINE): sensor.sensor_schema(
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_TVOC_BASELINE): sensor.sensor_schema(
                icon=ICON_RADIATOR,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_STORE_BASELINE, default=True): cv.boolean,
            cv.Optional(CONF_BASELINE): cv.Schema(
                {
                    cv.Required(CONF_ECO2_BASELINE): cv.hex_uint16_t,
                    cv.Required(CONF_TVOC_BASELINE): cv.hex_uint16_t,
                }
            ),
            cv.Optional(CONF_COMPENSATION): cv.Schema(
                {
                    cv.Required(CONF_HUMIDITY_SOURCE): cv.use_id(sensor.Sensor),
                    cv.Required(CONF_TEMPERATURE_SOURCE): cv.use_id(sensor.Sensor),
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x58))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if eco2_config := config.get(CONF_ECO2):
        sens = await sensor.new_sensor(eco2_config)
        cg.add(var.set_eco2_sensor(sens))

    if tvoc_config := config.get(CONF_TVOC):
        sens = await sensor.new_sensor(tvoc_config)
        cg.add(var.set_tvoc_sensor(sens))

    if eco2_baseline_config := config.get(CONF_ECO2_BASELINE):
        sens = await sensor.new_sensor(eco2_baseline_config)
        cg.add(var.set_eco2_baseline_sensor(sens))

    if tvoc_baseline_config := config.get(CONF_TVOC_BASELINE):
        sens = await sensor.new_sensor(tvoc_baseline_config)
        cg.add(var.set_tvoc_baseline_sensor(sens))

    if (store_baseline := config.get(CONF_STORE_BASELINE)) is not None:
        cg.add(var.set_store_baseline(store_baseline))

    if baseline_config := config.get(CONF_BASELINE):
        cg.add(var.set_eco2_baseline(baseline_config[CONF_ECO2_BASELINE]))
        cg.add(var.set_tvoc_baseline(baseline_config[CONF_TVOC_BASELINE]))

    if compensation_config := config.get(CONF_COMPENSATION):
        sens = await cg.get_variable(compensation_config[CONF_HUMIDITY_SOURCE])
        cg.add(var.set_humidity_sensor(sens))
        sens = await cg.get_variable(compensation_config[CONF_TEMPERATURE_SOURCE])
        cg.add(var.set_temperature_sensor(sens))
