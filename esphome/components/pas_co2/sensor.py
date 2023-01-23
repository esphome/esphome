import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor

from esphome.const import (
    CONF_ID,
    CONF_CO2,
    DEVICE_CLASS_CARBON_DIOXIDE,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
)

CODEOWNERS = ["@dmpolukhin"]
DEPENDENCIES = ["i2c"]

pas_co2_ns = cg.esphome_ns.namespace("pas_co2")
PASCO2Component = pas_co2_ns.class_(
    "PASCO2Component", cg.PollingComponent, i2c.I2CDevice
)


CONF_AMBIENT_PRESSURE_COMPENSATION = "ambient_pressure_compensation"
CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE = "ambient_pressure_compensation_source"


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(PASCO2Component),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION): cv.pressure,
            cv.Optional(CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE): cv.use_id(
                sensor.Sensor
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x28))
)

SENSOR_MAP = {
    CONF_CO2: "set_co2_sensor",
}

SETTING_MAP = {
    CONF_AMBIENT_PRESSURE_COMPENSATION: "set_ambient_pressure_compensation",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    for key, funcName in SETTING_MAP.items():
        if key in config:
            cg.add(getattr(var, funcName)(config[key]))

    for key, funcName in SENSOR_MAP.items():
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, funcName)(sens))

    if CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE in config:
        sens = await cg.get_variable(config[CONF_AMBIENT_PRESSURE_COMPENSATION_SOURCE])
        cg.add(var.set_ambient_pressure_source(sens))
