import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_PRESSURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_HECTOPASCAL,
)

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@Azimath"]

sdp3x_ns = cg.esphome_ns.namespace("sdp3x")
SDP3XComponent = sdp3x_ns.class_("SDP3XComponent", cg.PollingComponent, i2c.I2CDevice)


MeasurementMode = sdp3x_ns.enum("MeasurementMode")
MEASUREMENT_MODE = {
    "mass_flow": MeasurementMode.MASS_FLOW_AVG,
    "differential_pressure": MeasurementMode.DP_AVG,
}
CONF_MEASUREMENT_MODE = "measurement_mode"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        unit_of_measurement=UNIT_HECTOPASCAL,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_PRESSURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(SDP3XComponent),
            cv.Optional(
                CONF_MEASUREMENT_MODE, default="differential_pressure"
            ): cv.enum(MEASUREMENT_MODE),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x21))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await sensor.register_sensor(var, config)
    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))
