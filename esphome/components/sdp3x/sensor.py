import esphome.codegen as cg
from esphome.components import i2c, sensirion_common, sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PRESSURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_HECTOPASCAL,
    CONF_MEASUREMENT_MODE,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensirion_common"]
CODEOWNERS = ["@Azimath"]

sdp3x_ns = cg.esphome_ns.namespace("sdp3x")
SDP3XComponent = sdp3x_ns.class_(
    "SDP3XComponent", cg.PollingComponent, sensirion_common.SensirionI2CDevice
)


MeasurementMode = sdp3x_ns.enum("MeasurementMode")
MEASUREMENT_MODE = {
    "mass_flow": MeasurementMode.MASS_FLOW_AVG,
    "differential_pressure": MeasurementMode.DP_AVG,
}


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        SDP3XComponent,
        unit_of_measurement=UNIT_HECTOPASCAL,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_PRESSURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.Optional(
                CONF_MEASUREMENT_MODE, default="differential_pressure"
            ): cv.enum(MEASUREMENT_MODE),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x21))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))
