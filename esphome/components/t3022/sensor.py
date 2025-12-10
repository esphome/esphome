import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@aomi"]

t3022_ns = cg.esphome_ns.namespace("t3022")
T3022Component = t3022_ns.class_("T3022Component", cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(T3022Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,  # Accuracy: +/- 75 ppm or 10% of reading (whichever is greater)
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x15))  # Default I2C address for T3022/T67xx series
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_CO2 in config:
        sens = await sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))
